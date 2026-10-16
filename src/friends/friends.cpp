#include "friends.hpp"
#include "../platform/jni/jni_context.hpp"

#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include <imgui/imgui.h>

#include "../gui/gui.hpp"
#include "../modules/friends/friend_manager.hpp"
#include "../modules/ui_redirect.hpp"
namespace
{
	struct friend_state
	{
		std::mutex mutex;
		std::unordered_map<std::string, std::string> names;
		std::vector<std::string> server_players;
		std::string local_player;
		std::string status;
	};

	friend_state state;
	bool middle_was_down = false;
	std::chrono::steady_clock::time_point next_server_refresh{};

	std::string trim(std::string value)
	{
		const auto not_space = [](unsigned char character)
		{
			return !std::isspace(character);
		};
		value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
		value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
		return value;
	}

	std::string normalize(std::string_view name)
	{
		std::string result(name);
		std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character)
		{
			return static_cast<char>(std::tolower(character));
		});
		return result;
	}
std::string player_name(const maps::Entity& entity)
	{
		if (!entity || !entity.is_instance_of<maps::PlayerEntity>() || platform::jni_context::clear_exception())
			return {};

		maps::PlayerEntity player(entity.object_instance, false);
		maps::GameProfile profile = player.getGameProfile.call();
		maps::String name = profile ? profile.getName.call() : maps::String{};
		if (!profile || !name || platform::jni_context::clear_exception())
			return {};
		return name.to_string();
	}

	bool contains_case_insensitive(std::string_view text, std::string_view query)
	{
		return normalize(text).find(normalize(query)) != std::string::npos;
	}
}

bool friends::is_friend(std::string_view name)
{
	const std::string key = normalize(trim(std::string(name)));
	if (key.empty())
		return false;
	std::lock_guard lock(state.mutex);
	return state.names.contains(key);
}

bool friends::is_friend(const maps::Entity& entity)
{
	const std::string name = player_name(entity);
	return !name.empty() && is_friend(name);
}

void friends::add(std::string_view name)
{
	const std::string display = trim(std::string(name));
	if (display.empty())
		return;

	std::lock_guard lock(state.mutex);
	state.names[normalize(display)] = display;
	state.status = display + gui::text(" added as friend", " arkadaş olarak eklendi");
}

void friends::remove(std::string_view name)
{
	const std::string display = trim(std::string(name));
	if (display.empty())
		return;

	std::lock_guard lock(state.mutex);
	if (state.names.erase(normalize(display)) > 0)
		state.status = display + gui::text(" removed from friends", " arkadaşlardan çıkarıldı");
}

void friends::toggle(std::string_view name)
{
	const std::string display = trim(std::string(name));
	if (display.empty())
		return;

	std::lock_guard lock(state.mutex);
	const std::string key = normalize(display);
	if (state.names.erase(key) > 0)
		state.status = display + gui::text(" removed from friends", " arkadaşlardan çıkarıldı");
	else
	{
		state.names[key] = display;
		state.status = display + gui::text(" added as friend", " arkadaş olarak eklendi");
	}
}

std::vector<friends::server_player> friends::server_players_snapshot()
{
	std::lock_guard lock(state.mutex);
	std::vector<server_player> result;
	result.reserve(state.server_players.size());
	for (const std::string& name : state.server_players)
	{
		result.push_back({
			name,
			normalize(name) == normalize(state.local_player),
			state.names.contains(normalize(name))
		});
	}
	return result;
}

std::vector<std::string> friends::friend_names_snapshot()
{
	std::lock_guard lock(state.mutex);
	std::vector<std::string> result;
	result.reserve(state.names.size());
	for (const auto& [key, display] : state.names)
		result.push_back(display);
	std::sort(result.begin(), result.end(), [](const std::string& left, const std::string& right)
	{
		return normalize(left) < normalize(right);
	});
	return result;
}

std::string friends::local_player_name()
{
	std::lock_guard lock(state.mutex);
	return state.local_player;
}

std::string friends::status_message()
{
	std::lock_guard lock(state.mutex);
	return state.status;
}

void friends::update_server_players(::cache& cache)
{
	const auto now = std::chrono::steady_clock::now();
	if (now < next_server_refresh)
		return;
	next_server_refresh = now + std::chrono::milliseconds(500);

	if (!cache.is_valid() || !cache.instance || !cache.player || !jni::get_env())
	{
		std::lock_guard lock(state.mutex);
		state.server_players.clear();
		state.local_player.clear();
		return;
	}

	std::vector<std::string> players;
	std::unordered_set<std::string> seen;
	auto append_name = [&players, &seen](const std::string& display)
	{
		if (!display.empty() && seen.insert(normalize(display)).second)
			players.push_back(display);
	};

	// The network tab list contains everyone advertised by the server, including
	// players outside the world's current entity tracking distance.
	try
	{
		maps::ClientPlayNetworkHandler handler = cache.instance.getNetworkHandler();
		maps::Collection collection = handler ? handler.getPlayerList.call() : maps::Collection{};
		jni::array<maps::Object> array = collection ? collection.toArray.call() : jni::array<maps::Object>{ nullptr };
		if (handler && collection && array && !platform::jni_context::clear_exception())
		{
			for (const maps::Object& object : array.to_vector())
			{
				maps::PlayerListEntry entry(object.object_instance, false);
				maps::GameProfile profile = entry ? entry.getProfile.call() : maps::GameProfile{};
				maps::String name = profile ? profile.getName.call() : maps::String{};
				if (!entry || !profile || !name || platform::jni_context::clear_exception())
					continue;
				append_name(name.to_string());
			}
		}
		platform::jni_context::clear_exception();
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
	}

	// Merge the world's loaded player entities as a fallback for clients or
	// servers that temporarily expose an empty tab list during login/respawn.
	try
	{
		maps::List loaded_players = cache.players;
		jni::array<maps::Object> array = loaded_players
			? loaded_players.toArray.call()
			: jni::array<maps::Object>{ nullptr };
		if (loaded_players && array && !platform::jni_context::clear_exception())
		{
			for (const maps::Object& object : array.to_vector())
			{
				maps::Entity entity(object.object_instance, false);
				append_name(player_name(entity));
			}
		}
		platform::jni_context::clear_exception();
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
	}

	std::string local;
	try
	{
		maps::GameProfile local_profile = cache.player.getGameProfile.call();
		maps::String local_name = local_profile ? local_profile.getName.call() : maps::String{};
		local = local_name && !platform::jni_context::clear_exception()
			? local_name.to_string()
			: std::string{};
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
	}
	append_name(local);

	std::sort(players.begin(), players.end(), [](const std::string& left, const std::string& right)
	{
		return normalize(left) < normalize(right);
	});

	std::lock_guard lock(state.mutex);
	state.server_players = std::move(players);
	state.local_player = local;
}

void friends::handle_middle_click(::cache& cache, bool active)
{
	const bool middle_down = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
	const bool pressed = middle_down && !middle_was_down;
	middle_was_down = middle_down;

	if (!active || !pressed || !cache.is_valid() || !cache.instance || !cache.player)
		return;

	try
	{
		if (cache.instance.currentScreen.get() || platform::jni_context::clear_exception())
			return;

		maps::HitResult hit = cache.instance.crosshairTarget.get();
		maps::HitResult$Type type = hit ? hit.getType() : maps::HitResult$Type{};
		maps::HitResult$Type entity_type = maps::HitResult$Type{}.ENTITY.get();
		if (!hit || !type || !entity_type || platform::jni_context::clear_exception() ||
			!type.is_same_object(entity_type))
			return;

		maps::Entity target = maps::EntityHitResult(hit.object_instance, false).entity.get();
		if (!target || target.is_same_object(cache.player) ||
			!target.is_instance_of<maps::PlayerEntity>() || platform::jni_context::clear_exception())
			return;

		const std::string name = player_name(target);
		if (!name.empty())
			toggle(name);
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
	}
}

namespace
{
	void render_manager_v3(bool embedded)
	{
		static int active_tab = 0;
		static char search[64]{};
		static char add_name[64]{};
		const std::vector<friends::server_player> players = friends::server_players_snapshot();
		const std::vector<std::string> saved = friends::friend_names_snapshot();

		if (!embedded)
			gui::page_header(gui::text("Friends", "Arkada\xC5\x9Flar"),
				gui::text("Session trust list", "Oturum guven listesi"));
		const float full_width = ImGui::GetContentRegionAvail().x;
		const float tab_width = std::min(176.0f, (full_width - 8.0f) * 0.5f);
		const std::string server_tab = gui::text("Server Players", "Sunucu Oyunculari") + std::string("  ") + std::to_string(players.size());
		const std::string friends_tab = gui::text("Friends", "Arkadaslar") + std::string("  ") + std::to_string(saved.size());
		if (gui::action_button(server_tab.c_str(), ImVec2(tab_width, 32.0f), active_tab == 0 ? gui::action_tone::accent : gui::action_tone::neutral))
			active_tab = 0;
		ImGui::SameLine(0.0f, 8.0f);
		if (gui::action_button(friends_tab.c_str(), ImVec2(tab_width, 32.0f), active_tab == 1 ? gui::action_tone::accent : gui::action_tone::neutral))
			active_tab = 1;
		ImGui::Dummy(ImVec2(0.0f, 8.0f));

		if (gui::begin_surface("##friends_compact_v3", ImVec2(0.0f, ImGui::GetContentRegionAvail().y), ImVec2(14.0f, 12.0f), embedded))
		{
			if (active_tab == 0)
			{
				ImGui::SetNextItemWidth(-1.0f);
				ImGui::InputTextWithHint("##friend_server_search_v3", gui::text("Search server players", "Sunucu oyuncularinda ara"),
					search, IM_ARRAYSIZE(search));
			}
			else
			{
				const float available = ImGui::GetContentRegionAvail().x;
				ImGui::SetNextItemWidth(std::max(120.0f, available - 43.0f));
				ImGui::InputTextWithHint("##friend_manual_v3", gui::text("Add a player by name", "Oyuncuyu isimle ekle"),
					add_name, IM_ARRAYSIZE(add_name));
				ImGui::SameLine(0.0f, 7.0f);
				if (gui::icon_action("friend_manual_add_v3", "\xEF\x88\xB4", "+", gui::text("Add friend", "Arkadas ekle"), false, true) && add_name[0])
				{
					const std::string added = add_name;
					friends::add(added);
					add_name[0] = '\0';
					gui::push_workspace_toast(added + gui::text(" added", " eklendi"));
				}
			}
			ImGui::Dummy(ImVec2(0.0f, 7.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 1.0f);
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			if (ImGui::BeginChild("##friends_rows_v3", ImVec2(0.0f, 0.0f)))
			{
				ImFont* bold = gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont();
				ImFont* regular = gui::ui_font ? gui::ui_font : ImGui::GetFont();
				int visible = 0;
				const float gap = 8.0f;
				const int columns = ImGui::GetContentRegionAvail().x < 560.0f ? 1 : 2;
				const float card_width = (ImGui::GetContentRegionAvail().x - gap * (columns - 1) - 2.0f) / columns;
				int card_index = 0;
				ImVec2 row = ImGui::GetCursorScreenPos();
				auto begin_card = [&](const char* id, bool selected)
				{
					if ((card_index % columns) == 0) row = ImGui::GetCursorScreenPos();
					const ImVec2 card_min(row.x + (card_index % columns) * (card_width + gap), row.y);
					const ImVec2 card_max(card_min.x + card_width, card_min.y + 64.0f);
					ImGui::PushID(id);
					ImGui::SetCursorScreenPos(card_min);
					ImGui::Dummy(ImVec2(card_width, 64.0f));
					gui::list_row_background(card_min, card_max, ImGui::IsMouseHoveringRect(card_min, card_max, true), selected);
					return std::pair<ImVec2, ImVec2>(card_min, card_max);
				};
				auto end_card = [&]()
				{
					ImGui::PopID();
					++card_index;
					if ((card_index % columns) == 0)
						ImGui::SetCursorScreenPos(ImVec2(row.x, row.y + 72.0f));
				};
				if (active_tab == 0)
				{
					for (const friends::server_player& player : players)
					{
						if (search[0] && !contains_case_insensitive(player.name, search)) continue;
						++visible;
						auto [row_min, row_max] = begin_card(player.name.c_str(), player.friend_added);
						ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(row_min.x + 25.0f, row_min.y + 32.0f), 15.0f,
							player.friend_added ? ImGui::GetColorU32(ImGuiCol_CheckMark) : IM_COL32(63, 65, 76, 255));
						const char initial[2] = { player.name.empty() ? '?' : static_cast<char>(std::toupper(static_cast<unsigned char>(player.name[0]))), 0 };
						const ImVec2 initial_extent = bold->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, initial);
						ImGui::GetWindowDrawList()->AddText(bold, 14.0f,
							ImVec2(row_min.x + 25.0f - initial_extent.x * 0.5f, row_min.y + 32.0f - initial_extent.y * 0.5f), IM_COL32_WHITE, initial);
						const ImVec4 clip(row_min.x + 49.0f, row_min.y + 4.0f, row_max.x - 46.0f, row_max.y - 4.0f);
						ImGui::GetWindowDrawList()->AddText(bold, 15.5f, ImVec2(row_min.x + 49.0f, row_min.y + 11.0f),
							IM_COL32(232, 234, 241, 255), player.name.c_str(), nullptr, 0.0f, &clip);
						const char* state_label = player.local ? gui::text("YOU", "SEN")
							: (player.friend_added ? gui::text("FRIEND", "ARKADAS") : gui::text("ONLINE", "CEVRIMICI"));
						ImGui::GetWindowDrawList()->AddText(regular, 11.5f, ImVec2(row_min.x + 49.0f, row_min.y + 37.0f),
							player.friend_added ? ImGui::GetColorU32(ImGuiCol_CheckMark) : IM_COL32(139, 133, 151, 255), state_label, nullptr, 0.0f, &clip);
						if (!player.local)
						{
							ImGui::SetCursorScreenPos(ImVec2(row_max.x - 37.0f, row_min.y + 17.0f));
							if (gui::icon_action("friend_toggle_v3", player.friend_added ? "\xEF\x94\x83" : "\xEF\x88\xB4",
								player.friend_added ? "-" : "+", player.friend_added ? gui::text("Remove friend", "Arkadasi cikar")
								: gui::text("Add friend", "Arkadas ekle"), player.friend_added, !player.friend_added))
								friends::toggle(player.name);
						}
						end_card();
					}
				}
				else
				{
					for (const std::string& name : saved)
					{
						++visible;
						auto [row_min, row_max] = begin_card(name.c_str(), true);
						ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(row_min.x + 25.0f, row_min.y + 32.0f), 15.0f,
							ImGui::GetColorU32(ImGuiCol_CheckMark));
						const char initial[2] = { name.empty() ? '?' : static_cast<char>(std::toupper(static_cast<unsigned char>(name[0]))), 0 };
						const ImVec2 initial_extent = bold->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, initial);
						ImGui::GetWindowDrawList()->AddText(bold, 14.0f,
							ImVec2(row_min.x + 25.0f - initial_extent.x * 0.5f, row_min.y + 32.0f - initial_extent.y * 0.5f), IM_COL32_WHITE, initial);
						const ImVec4 clip(row_min.x + 49.0f, row_min.y + 4.0f, row_max.x - 46.0f, row_max.y - 4.0f);
						ImGui::GetWindowDrawList()->AddText(bold, 15.5f, ImVec2(row_min.x + 49.0f, row_min.y + 21.0f),
							IM_COL32(232, 234, 241, 255), name.c_str(), nullptr, 0.0f, &clip);
						ImGui::SetCursorScreenPos(ImVec2(row_max.x - 37.0f, row_min.y + 17.0f));
						if (gui::icon_action("friend_remove_v3", "\xEF\x94\x83", "-", gui::text("Remove friend", "Arkadasi cikar"), true))
							friends::remove(name);
						end_card();
					}
				}
				if ((card_index % columns) != 0)
					ImGui::SetCursorScreenPos(ImVec2(row.x, row.y + 72.0f));
				if (!visible)
				{
					ImGui::Dummy(ImVec2(0.0f, 28.0f));
					const char* empty = active_tab == 0 ? gui::text("No server players available", "Sunucu oyuncusu bulunamadi")
						: gui::text("No friends in this session", "Bu oturumda arkadas yok");
					const ImVec2 extent = ImGui::CalcTextSize(empty);
					ImGui::SetCursorPosX(std::max(0.0f, (ImGui::GetContentRegionAvail().x - extent.x) * 0.5f));
					ImGui::TextDisabled("%s", empty);
				}
			}
			ImGui::EndChild();
			ImGui::PopStyleColor();
			ImGui::PopStyleVar();
		}
		gui::end_surface();
	}
}

void friends::render_manager(bool embedded)
{
	render_manager_v3(embedded);
}

void modules::friend_manager::run(::cache& cache)
{
	try
	{
		jni::frame frame{ 160 };
		friends::update_server_players(cache);
		const HWND minecraft_window = gui::get_window();
		const bool active = !gui::draw && minecraft_window &&
			GetForegroundWindow() == minecraft_window;
		friends::handle_middle_click(cache, active);
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
	}
}

void modules::friend_manager::on_disable(::cache& cache)
{
	friends::handle_middle_click(cache, false);
}

void modules::friend_manager::render_options()
{
	friends::render_manager();
}
