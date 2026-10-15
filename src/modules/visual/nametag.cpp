#include "nametag.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../services/game/overlay_context.hpp"
#include "../../platform/adapter/game_adapter.hpp"
#include "../ui_redirect.hpp"
#include "../../gui/gui.hpp"

#include <imgui/imgui.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
	struct nametag_entry
	{
		double x = 0.0;
		double y = 0.0;
		double z = 0.0;
		double prev_x = 0.0;
		double prev_y = 0.0;
		double prev_z = 0.0;
		double curr_x = 0.0;
		double curr_y = 0.0;
		double curr_z = 0.0;
		double height_offset = 0.48;
		bool has_prev = false;
		std::string name;
		float health = 20.0f;
		float max_health = 20.0f;
		float distance = 0.0f;
		// Helmet, chestplate, leggings, boots.
		std::array<std::string, 4> armor{ "---", "---", "---", "---" };
	};

	enum class carrier_kind
	{
		armor_stand,
		text_display
	};

	struct player_anchor
	{
		maths::vector3d position{};
		std::string name;
	};

	struct carrier_snapshot
	{
		jobject entity = nullptr;
		carrier_kind kind = carrier_kind::armor_stand;
		maths::vector3d position{};
		std::string text;
	};

	struct suppressed_carrier
	{
		jobject entity = nullptr;
		jobject original_text = nullptr;
		carrier_kind kind = carrier_kind::armor_stand;
		std::string text;
		jboolean original_name_visible = JNI_FALSE;
		bool active = false;
	};

	std::mutex tag_mutex;
    services::game::overlay_session session;
	std::vector<nametag_entry> tag_entries;
	std::atomic_bool camera_valid{ false };
	std::atomic<double> camera_x{ 0.0 };
	std::atomic<double> camera_y{ 0.0 };
	std::atomic<double> camera_z{ 0.0 };
	std::atomic<float> camera_yaw{ 0.0f };
	std::atomic<float> camera_pitch{ 0.0f };
	std::atomic<float> camera_fov{ 70.0f };
    void clear_tag_snapshot()
    {
        camera_valid.store(false, std::memory_order_release);
        std::lock_guard lock(tag_mutex);
        if (jni::get_env()) session.reset(); else session.invalidate();
        tag_entries.clear();
    }

	std::unordered_map<std::string, std::string> original_team_by_player;
	std::string hidden_team_name;
	jobject scoreboard_identity = nullptr;
	std::vector<suppressed_carrier> suppressed_carriers;
std::string text_to_string(const maps::Text& text)
	{
		if (!text)
			return {};
		maps::String value = text.getString.call();
		if (platform::jni_context::clear_exception() || !value)
			return {};
		return value.to_string();
	}

	std::string lower_ascii(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character)
		{
			return static_cast<char>(std::tolower(character));
		});
		return value;
	}

	suppressed_carrier* find_suppressed_carrier(jobject entity)
	{
		JNIEnv* env = jni::get_env();
		if (!env || !entity)
			return nullptr;
		for (suppressed_carrier& carrier : suppressed_carriers)
		{
			if (carrier.entity && env->IsSameObject(carrier.entity, entity) == JNI_TRUE)
				return &carrier;
		}
		return nullptr;
	}

	void restore_and_release_carrier(suppressed_carrier& carrier, bool restore)
	{
		JNIEnv* env = jni::get_env();
		if (!env)
			return;
		if (restore && carrier.entity)
		{
			if (carrier.kind == carrier_kind::armor_stand)
			{
				maps::Entity entity(carrier.entity, false);
				entity.setCustomNameVisible.call(carrier.original_name_visible);
			}
			else if (carrier.original_text)
			{
				maps::TextDisplayEntity display(carrier.entity, false);
				display.setText.call(maps::Text(carrier.original_text, false));
			}
			platform::jni_context::clear_exception();
		}
		if (carrier.original_text)
			env->DeleteGlobalRef(carrier.original_text);
		if (carrier.entity)
			env->DeleteGlobalRef(carrier.entity);
		carrier.original_text = nullptr;
		carrier.entity = nullptr;
	}

	void release_suppressed_carriers(bool restore)
	{
		for (suppressed_carrier& carrier : suppressed_carriers)
			restore_and_release_carrier(carrier, restore);
		suppressed_carriers.clear();
	}

	std::string read_carrier_text(jobject object, carrier_kind kind)
	{
		if (!object)
			return {};
		std::string current;
		if (kind == carrier_kind::armor_stand)
		{
			maps::Entity entity(object, false);
			current = text_to_string(entity.getCustomName.call());
		}
		else
		{
			maps::TextDisplayEntity display(object, false);
			current = text_to_string(display.getText.call());
		}
		platform::jni_context::clear_exception();
		if (!current.empty())
			return current;
		const suppressed_carrier* saved = find_suppressed_carrier(object);
		return saved ? saved->text : std::string{};
	}

	void suppress_carrier(jobject object, carrier_kind kind, const std::string& visible_text)
	{
		JNIEnv* env = jni::get_env();
		if (!env || !object)
			return;

		suppressed_carrier* saved = find_suppressed_carrier(object);
		if (!saved)
		{
			suppressed_carrier created{};
			created.entity = env->NewGlobalRef(object);
			created.kind = kind;
			created.text = visible_text;
			created.active = true;
			if (kind == carrier_kind::armor_stand)
			{
				maps::Entity entity(object, false);
				created.original_name_visible = entity.isCustomNameVisible.call();
				if (platform::jni_context::clear_exception())
					created.original_name_visible = JNI_TRUE;
			}
			else
			{
				maps::TextDisplayEntity display(object, false);
				maps::Text original = display.getText.call();
				if (!platform::jni_context::clear_exception() && original)
					created.original_text = env->NewGlobalRef(original.object_instance);
			}
			suppressed_carriers.push_back(std::move(created));
			saved = &suppressed_carriers.back();
		}
		else
		{
			saved->active = true;
			// A server packet can refresh TextDisplay contents while the module is
			// active. Preserve the newest value before blanking it again.
			if (kind == carrier_kind::text_display)
			{
				maps::TextDisplayEntity display(object, false);
				maps::Text current = display.getText.call();
				const std::string current_string = text_to_string(current);
				if (!platform::jni_context::clear_exception() && current && !current_string.empty())
				{
					if (saved->original_text)
						env->DeleteGlobalRef(saved->original_text);
					saved->original_text = env->NewGlobalRef(current.object_instance);
					saved->text = current_string;
				}
			}
		}

		if (kind == carrier_kind::armor_stand)
		{
			maps::Entity entity(object, false);
			entity.setCustomNameVisible.call(JNI_FALSE);
		}
		else
		{
			maps::TextDisplayEntity display(object, false);
			maps::Text empty = maps::Text{}.of.call(maps::String::create(""));
			if (empty && !platform::jni_context::clear_exception())
				display.setText.call(empty);
		}
		platform::jni_context::clear_exception();
	}

	bool world_to_screen(
		double world_x,
		double world_y,
		double world_z,
		double view_x,
		double view_y,
		double view_z,
		float yaw,
		float pitch,
		float width,
		float height,
		float fov,
		float& screen_x,
		float& screen_y)
	{
		const double offset_x = world_x - view_x;
		const double offset_y = world_y - view_y;
		const double offset_z = world_z - view_z;
		constexpr double pi = 3.14159265358979323846;
		const double yaw_radians = static_cast<double>(yaw) * pi / 180.0;
		const double pitch_radians = static_cast<double>(pitch) * pi / 180.0;
		const double sin_yaw = std::sin(yaw_radians);
		const double cos_yaw = std::cos(yaw_radians);
		const double sin_pitch = std::sin(pitch_radians);
		const double cos_pitch = std::cos(pitch_radians);

		const double forward_x = -sin_yaw * cos_pitch;
		const double forward_y = -sin_pitch;
		const double forward_z = cos_yaw * cos_pitch;
		const double right_x = cos_yaw;
		const double right_z = sin_yaw;
		const double up_x = -sin_yaw * sin_pitch;
		const double up_y = cos_pitch;
		const double up_z = cos_yaw * sin_pitch;

		const double depth = offset_x * forward_x + offset_y * forward_y + offset_z * forward_z;
		if (depth <= 0.05)
			return false;
		const double horizontal = offset_x * -right_x + offset_z * -right_z;
		const double vertical = offset_x * up_x + offset_y * up_y + offset_z * up_z;
		const double focal = (height * 0.5) / std::tan(static_cast<double>(fov) * pi / 360.0);
		screen_x = static_cast<float>(width * 0.5 + horizontal * focal / depth);
		screen_y = static_cast<float>(height * 0.5 - vertical * focal / depth);
		return true;
	}

	std::string armor_code(const std::string& translation_key)
	{
		if (translation_key.find("netherite_") != std::string::npos) return "NET";
		if (translation_key.find("diamond_") != std::string::npos) return "DIA";
		if (translation_key.find("chainmail_") != std::string::npos) return "CHN";
		if (translation_key.find("golden_") != std::string::npos) return "GLD";
		if (translation_key.find("iron_") != std::string::npos) return "IRN";
		if (translation_key.find("leather_") != std::string::npos) return "LEA";
		if (translation_key.find("turtle_helmet") != std::string::npos) return "TUR";
		if (translation_key.find("elytra") != std::string::npos) return "ELY";
		return "---";
	}

	ImU32 armor_color(const std::string& code, float alpha = 1.0f)
	{
		const int a = static_cast<int>(255.0f * std::clamp(alpha, 0.0f, 1.0f));
		if (code == "NET") return IM_COL32(145, 111, 180, a);
		if (code == "DIA") return IM_COL32(86, 222, 211, a);
		if (code == "CHN") return IM_COL32(170, 177, 188, a);
		if (code == "GLD") return IM_COL32(244, 196, 67, a);
		if (code == "IRN") return IM_COL32(218, 221, 225, a);
		if (code == "LEA") return IM_COL32(170, 105, 66, a);
		if (code == "TUR") return IM_COL32(66, 185, 129, a);
		if (code == "ELY") return IM_COL32(132, 118, 159, a);
		return IM_COL32(84, 86, 96, a);
	}

	maps::Team get_or_create_hidden_team(const maps::Scoreboard& scoreboard)
	{
		if (!scoreboard)
			return {};

		if (!hidden_team_name.empty())
		{
			maps::String name = maps::String::create(hidden_team_name.c_str());
			maps::Team team = scoreboard.getTeam(name);
			if (platform::jni_context::clear_exception())
				return {};
			if (!team)
				team = scoreboard.addTeam(name);
			if (platform::jni_context::clear_exception())
				return {};
			return team;
		}

		for (int suffix = 0; suffix < 100; ++suffix)
		{
			char candidate[16]{};
			std::snprintf(candidate, sizeof(candidate), "wc_nt_%02d", suffix);
			maps::String name = maps::String::create(candidate);
			maps::Team existing = scoreboard.getTeam(name);
			if (platform::jni_context::clear_exception())
				return {};
			if (existing)
				continue;
			maps::Team created = scoreboard.addTeam(name);
			if (platform::jni_context::clear_exception() || !created)
				return {};
			hidden_team_name = candidate;
			return created;
		}
		return {};
	}

	void suppress_vanilla_tag(
		const maps::Scoreboard& scoreboard,
		const maps::Team& hidden_team,
		const std::string& player_name)
	{
		if (!scoreboard || !hidden_team || player_name.empty())
			return;
		maps::String java_name = maps::String::create(player_name.c_str());
		maps::Team current_team = scoreboard.getScoreHolderTeam(java_name);
		if (platform::jni_context::clear_exception())
			return;

		std::string current_team_name;
		if (current_team)
		{
			maps::String current_name = current_team.getName();
			if (current_name && !platform::jni_context::clear_exception())
				current_team_name = current_name.to_string();
		}
		if (current_team_name == hidden_team_name)
			return;
		if (!original_team_by_player.contains(player_name))
			original_team_by_player[player_name] = current_team_name;
		scoreboard.addScoreHolderToTeam(java_name, hidden_team);
		platform::jni_context::clear_exception();
	}
}

void modules::nametag::run(::cache& cache)
{
    auto settings_snapshot = settings_.read();
	jni::frame frame{};
	JNIEnv* env = jni::get_env();
	if (!env || !cache.is_valid() || !cache.instance || !cache.world || !cache.player) { clear_tag_snapshot(); return; }

	try
	{
        std::uint64_t generation;
        {
            std::lock_guard lock(tag_mutex);
            if (session.observe({cache.world, cache.player})) tag_entries.clear();
            generation = session.ticket();
        }
		float tick_delta = platform::adapter::get().get_tick_delta(cache.instance);
		auto cam_data = platform::adapter::get().get_camera_data(cache.instance, cache.player, tick_delta);
		if (cam_data.valid)
		{
			camera_x.store(cam_data.pos.x, std::memory_order_relaxed);
			camera_y.store(cam_data.pos.y, std::memory_order_relaxed);
			camera_z.store(cam_data.pos.z, std::memory_order_relaxed);
			camera_yaw.store(cam_data.yaw, std::memory_order_relaxed);
			camera_pitch.store(cam_data.pitch, std::memory_order_relaxed);
			camera_fov.store(cam_data.fov, std::memory_order_relaxed);
			camera_valid.store(true, std::memory_order_release);
		}
		else
		{
			camera_valid.store(false, std::memory_order_release);
		}

		maps::Scoreboard scoreboard{};
		maps::Team hidden_team{};
		try
		{
			scoreboard = cache.world.getScoreboard();
			if (scoreboard)
			{
				if (scoreboard_identity &&
					env->IsSameObject(scoreboard_identity, scoreboard.object_instance) != JNI_TRUE)
				{
					release_suppressed_carriers(false);
					env->DeleteGlobalRef(scoreboard_identity);
					scoreboard_identity = nullptr;
					original_team_by_player.clear();
					hidden_team_name.clear();
				}
				if (!scoreboard_identity)
					scoreboard_identity = env->NewGlobalRef(scoreboard.object_instance);

				hidden_team = get_or_create_hidden_team(scoreboard);
				maps::TeamVisibilityRule never_rule = maps::TeamVisibilityRule{}.NEVER.get();
				if (hidden_team && never_rule)
					hidden_team.setNameTagVisibilityRule(never_rule);
				platform::jni_context::clear_exception();
			}
		}
		catch (...)
		{
			platform::jni_context::clear_exception();
		}

		const maths::vector3d local_position = cache.player.get_position();
		std::vector<nametag_entry> next_entries;
		std::vector<player_anchor> player_anchors;
		std::vector<carrier_snapshot> carrier_snapshots;
		for (suppressed_carrier& carrier : suppressed_carriers)
			carrier.active = false;
		maps::Iterable entities = platform::adapter::get().get_entities(cache.world);
		maps::Iterator iterator = entities ? entities.iterator.call() : maps::Iterator{};
		while (iterator && iterator.hasNext.call() == JNI_TRUE)
		{
			maps::Object object = iterator.next.call();
			if (!object)
				continue;
			maps::Entity entity(object.object_instance);
			if (!entity)
			{
				continue;
			}

			const bool is_player = entity.is_instance_of<maps::PlayerEntity>();
			if (!is_player)
			{
				carrier_snapshot carrier{};
				if (entity.is_instance_of<maps::ArmorStandEntity>())
					carrier.kind = carrier_kind::armor_stand;
				else if (entity.is_instance_of<maps::TextDisplayEntity>())
					carrier.kind = carrier_kind::text_display;
				else
					continue;

				carrier.entity = object.object_instance;
				carrier.position = entity.get_position();
				carrier.text = read_carrier_text(object.object_instance, carrier.kind);
				if (!carrier.text.empty())
					carrier_snapshots.push_back(std::move(carrier));
				continue;
			}

			if (entity.is_same_object(cache.player) || entity.isAlive.call() == JNI_FALSE)
				continue;

			maps::PlayerEntity player(entity.object_instance);
			maps::LivingEntity living(entity.object_instance);
			maps::Box bounds = entity.get_bounding_box();
			if (!bounds)
				continue;

			const maths::vector3d current = entity.get_position();
			const maths::vector3d previous = entity.get_prev_position();
			const double interpolated_x = previous.x + (current.x - previous.x) * tick_delta;
			const double interpolated_y = previous.y + (current.y - previous.y) * tick_delta;
			const double interpolated_z = previous.z + (current.z - previous.z) * tick_delta;
			const double shift_x = interpolated_x - current.x;
			const double shift_y = interpolated_y - current.y;
			const double shift_z = interpolated_z - current.z;

			nametag_entry entry{};
			const bool valid_prev = (previous.x != 0.0 || previous.y != 0.0 || previous.z != 0.0);
			entry.prev_x = valid_prev ? previous.x : current.x;
			entry.prev_y = valid_prev ? previous.y : current.y;
			entry.prev_z = valid_prev ? previous.z : current.z;
			entry.curr_x = current.x;
			entry.curr_y = current.y;
			entry.curr_z = current.z;
			entry.height_offset = (bounds.maxY.get() - current.y) + 0.48;
			entry.has_prev = true;
			entry.x = (bounds.minX.get() + bounds.maxX.get()) * 0.5 + shift_x;
			entry.y = bounds.maxY.get() + 0.48 + shift_y;
			entry.z = (bounds.minZ.get() + bounds.maxZ.get()) * 0.5 + shift_z;
			const double dx = entry.x - local_position.x;
			const double dy = entry.y - local_position.y;
			const double dz = entry.z - local_position.z;
			entry.distance = static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz));
			if (entry.distance > settings_snapshot.max_distance)
				continue;

			try
			{
				maps::String name = entity.getEntityName();
				if (name)
					entry.name = name.to_string();
				entry.health = living.getHealth();
				entry.max_health = living.getMaxHealth();
				if (!std::isfinite(entry.health) || !std::isfinite(entry.max_health) || entry.max_health <= 0.0f)
				{
					entry.health = 20.0f;
					entry.max_health = 20.0f;
				}
				entry.health = std::clamp(entry.health, 0.0f, entry.max_health);

				maps::PlayerInventory inventory = player.getInventory();
				maps::DefaultedList armor = inventory ? inventory.armor.get() : maps::DefaultedList{};
				static constexpr int armor_indices[4] = { 3, 2, 1, 0 };
				for (int slot = 0; slot < 4 && armor; ++slot)
				{
					maps::Object armor_object = armor.get.call(static_cast<jint>(armor_indices[slot]));
					maps::ItemStack stack(armor_object ? armor_object.object_instance : nullptr);
					if (!stack || stack.isEmpty.call() == JNI_TRUE || platform::jni_context::clear_exception())
						continue;
					maps::Item item = stack.item.get();
					maps::String key = item ? item.getTranslationKey() : maps::String{};
					if (key && !platform::jni_context::clear_exception())
						entry.armor[slot] = armor_code(key.to_string());
				}
			}
			catch (...)
			{
				platform::jni_context::clear_exception();
			}

			if (!entry.name.empty())
			{
				suppress_vanilla_tag(scoreboard, hidden_team, entry.name);
				player_anchors.push_back({ current, entry.name });
				next_entries.push_back(std::move(entry));
			}
		}

		// Server-side nametag plugins commonly attach one or more invisible
		// ArmorStand/TextDisplay entities around a player's head. Confirm a
		// cluster by finding the player's name in at least one row, then hide all
		// rows in that same tight head-space cluster (rank, health, guild, etc.).
		const auto carrier_is_near_player = [](const carrier_snapshot& carrier, const player_anchor& player)
		{
			const double dx = carrier.position.x - player.position.x;
			const double dz = carrier.position.z - player.position.z;
			const double dy = carrier.position.y - player.position.y;
			return dx * dx + dz * dz <= 1.65 * 1.65 && dy >= -0.85 && dy <= 5.0;
		};

		std::vector<bool> has_server_tag_cluster(player_anchors.size(), false);
		for (std::size_t player_index = 0; player_index < player_anchors.size(); ++player_index)
		{
			const std::string player_name = lower_ascii(player_anchors[player_index].name);
			for (const carrier_snapshot& carrier : carrier_snapshots)
			{
				if (carrier_is_near_player(carrier, player_anchors[player_index]) &&
					lower_ascii(carrier.text).find(player_name) != std::string::npos)
				{
					has_server_tag_cluster[player_index] = true;
					break;
				}
			}
		}

		for (const carrier_snapshot& carrier : carrier_snapshots)
		{
			for (std::size_t player_index = 0; player_index < player_anchors.size(); ++player_index)
			{
				if (has_server_tag_cluster[player_index] &&
					carrier_is_near_player(carrier, player_anchors[player_index]))
				{
					suppress_carrier(carrier.entity, carrier.kind, carrier.text);
					break;
				}
			}
		}

		for (auto iterator = suppressed_carriers.begin(); iterator != suppressed_carriers.end();)
		{
			if (!iterator->active)
			{
				restore_and_release_carrier(*iterator, true);
				iterator = suppressed_carriers.erase(iterator);
			}
			else
			{
				++iterator;
			}
		}

		std::lock_guard lock(tag_mutex);
        if (!session.accepts(generation)) return;
		tag_entries = std::move(next_entries);
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
        clear_tag_snapshot();
	}
}

void modules::nametag::on_disable(::cache& cache)
{
    clear_tag_snapshot();

	jni::frame frame{};
	JNIEnv* env = jni::get_env();
	try
	{
		if (env)
			release_suppressed_carriers(true);
		if (env && cache.is_valid() && cache.world && !hidden_team_name.empty())
		{
			maps::Scoreboard scoreboard = cache.world.getScoreboard();
			maps::String hidden_name = maps::String::create(hidden_team_name.c_str());
			maps::Team hidden_team = scoreboard ? scoreboard.getTeam(hidden_name) : maps::Team{};
			if (scoreboard && hidden_team && !platform::jni_context::clear_exception())
			{
				for (const auto& [player_name, original_team_name] : original_team_by_player)
				{
					maps::String java_player_name = maps::String::create(player_name.c_str());
					maps::Team current_team = scoreboard.getScoreHolderTeam(java_player_name);
					if (platform::jni_context::clear_exception())
						continue;
					if (current_team && current_team.is_same_object(hidden_team))
					{
						scoreboard.removeScoreHolderFromTeam(java_player_name, hidden_team);
						if (platform::jni_context::clear_exception())
							continue;
					}
					if (!original_team_name.empty())
					{
						maps::String original_name = maps::String::create(original_team_name.c_str());
						maps::Team original_team = scoreboard.getTeam(original_name);
						if (original_team && !platform::jni_context::clear_exception())
							scoreboard.addScoreHolderToTeam(java_player_name, original_team);
						platform::jni_context::clear_exception();
					}
				}
				scoreboard.removeTeam(hidden_team);
				platform::jni_context::clear_exception();
			}
		}
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
	}

	original_team_by_player.clear();
	hidden_team_name.clear();
	if (env && scoreboard_identity)
		env->DeleteGlobalRef(scoreboard_identity);
	scoreboard_identity = nullptr;
}

void modules::nametag::render_overlay()
{
    auto settings_snapshot = settings_.read();
    if (!platform::jni_context::bind_current_thread()) { clear_tag_snapshot(); return; }
    jni::frame render_frame{96};
    services::game::client_context context;
    if (!services::game::capture_client_context(context, true, false)) { clear_tag_snapshot(); return; }
	ImDrawList* draw_list = ImGui::GetForegroundDrawList();
	ImFont* font = ImGui::GetFont();
	const ImVec2 display = ImGui::GetIO().DisplaySize;
	if (!draw_list || !font || display.x < 1.0f || display.y < 1.0f)
		return;

    auto& adapter = platform::adapter::get();
    const float render_tick_delta = adapter.get_tick_delta(context.instance);
    const auto cam_data = adapter.get_camera_data(context.instance, context.player, render_tick_delta);
    if (platform::jni_context::clear_exception() || !cam_data.valid) { clear_tag_snapshot(); return; }
    camera_x.store(cam_data.pos.x, std::memory_order_relaxed);
    camera_y.store(cam_data.pos.y, std::memory_order_relaxed);
    camera_z.store(cam_data.pos.z, std::memory_order_relaxed);
    camera_yaw.store(cam_data.yaw, std::memory_order_relaxed);
    camera_pitch.store(cam_data.pitch, std::memory_order_relaxed);
    camera_fov.store(cam_data.fov, std::memory_order_relaxed);
    camera_valid.store(true, std::memory_order_release);

	std::vector<nametag_entry> entries;
	{
		std::lock_guard lock(tag_mutex);
        if (session.observe({context.world, context.player})) { tag_entries.clear(); return; }
		entries = tag_entries;
	}

	const double view_x = camera_x.load(std::memory_order_relaxed);
	const double view_y = camera_y.load(std::memory_order_relaxed);
	const double view_z = camera_z.load(std::memory_order_relaxed);
	const float yaw = camera_yaw.load(std::memory_order_relaxed);
	const float pitch = camera_pitch.load(std::memory_order_relaxed);
	const float fov = camera_fov.load(std::memory_order_relaxed);
	const float base_scale = std::clamp(settings_snapshot.tag_scale, 0.55f, 1.75f);
	const ImColor accent = gui::get_dynamic_color(20);

	for (const nametag_entry& entry : entries)
	{
		double ent_x = entry.x;
		double ent_y = entry.y;
		double ent_z = entry.z;
		if (entry.has_prev)
		{
			ent_x = entry.prev_x + (entry.curr_x - entry.prev_x) * render_tick_delta;
			ent_y = entry.prev_y + (entry.curr_y - entry.prev_y) * render_tick_delta + entry.height_offset;
			ent_z = entry.prev_z + (entry.curr_z - entry.prev_z) * render_tick_delta;
		}

		float anchor_x = 0.0f;
		float anchor_y = 0.0f;
		if (!world_to_screen(ent_x, ent_y, ent_z, view_x, view_y, view_z,
			yaw, pitch, display.x, display.y, fov, anchor_x, anchor_y))
		{
			continue;
		}
		if (anchor_x < -300.0f || anchor_x > display.x + 300.0f ||
			anchor_y < -120.0f || anchor_y > display.y + 120.0f)
		{
			continue;
		}

		// A fixed pixel-sized panel looks progressively larger than the player at
		// range. Apply a gentle perspective falloff while retaining readability.
		const float distance_scale = std::clamp(
			std::sqrt(12.0f / std::max(entry.distance, 12.0f)), 0.46f, 1.0f);
		const float scale = base_scale * distance_scale;
		const float font_size = font->FontSize * scale * 0.88f;
		const float small_font_size = font_size * 0.72f;

		// Vanilla Minecraft authentic nametag styling: dark semi-transparent pill background with drop-shadowed text
		char health_text[32]{};
		std::snprintf(health_text, sizeof(health_text), "%.0f HP", entry.health);

		const ImVec2 name_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, entry.name.c_str());
		const ImVec2 health_size = settings_snapshot.show_health
			? font->CalcTextSizeA(small_font_size, FLT_MAX, 0.0f, health_text)
			: ImVec2{};
		const float top_gap = settings_snapshot.show_health ? 8.0f * scale : 0.0f;
		const float top_width = name_size.x + top_gap + health_size.x;
		const float horizontal_padding = 6.0f * scale;
		const float vertical_padding = 3.0f * scale;
		const float chip_width = 38.0f * scale;
		const float chip_height = 16.0f * scale;
		const float chip_gap = 3.0f * scale;
		const float armor_width = chip_width * 4.0f + chip_gap * 3.0f;
		const float panel_width = std::max(top_width, settings_snapshot.show_armor ? armor_width : 0.0f) + horizontal_padding * 2.0f;
		const float panel_height = vertical_padding * 2.0f + std::max(name_size.y, health_size.y) +
			(settings_snapshot.show_armor ? chip_height + 4.0f * scale : 0.0f);
		const ImVec2 panel_min(anchor_x - panel_width * 0.5f,
			anchor_y - std::max(name_size.y, health_size.y) * 0.5f - vertical_padding);
		const ImVec2 panel_max(panel_min.x + panel_width, panel_min.y + panel_height);

		// Vanilla semi-transparent dark background (0x90000000 -> 144 alpha)
		draw_list->AddRectFilled(panel_min, panel_max, IM_COL32(0, 0, 0, 150), 3.0f * scale);
		draw_list->AddRect(panel_min, panel_max, IM_COL32(0, 0, 0, 190), 3.0f * scale, 0, std::max(1.0f, scale));

		float top_x = panel_min.x + (panel_width - top_width) * 0.5f;
		const float top_center_y = panel_min.y + vertical_padding + std::max(name_size.y, health_size.y) * 0.5f;
		const ImVec2 name_pos(top_x, top_center_y - name_size.y * 0.5f);

		// Drop shadow for text (Vanilla font shadow style)
		draw_list->AddText(font, font_size, ImVec2(name_pos.x + 1.0f * scale, name_pos.y + 1.0f * scale),
			IM_COL32(32, 32, 32, 220), entry.name.c_str());
		draw_list->AddText(font, font_size, name_pos,
			IM_COL32(255, 255, 255, 255), entry.name.c_str());

		top_x += name_size.x + top_gap;
		if (settings_snapshot.show_health)
		{
			const float health_ratio = entry.max_health > 0.0f
				? std::clamp(entry.health / entry.max_health, 0.0f, 1.0f)
				: 0.0f;
			const int red = static_cast<int>((1.0f - health_ratio) * 235.0f + 40.0f);
			const int green = static_cast<int>(health_ratio * 205.0f + 40.0f);
			const ImU32 hp_color = IM_COL32(std::min(red, 255), std::min(green, 255), 72, 255);
			const ImVec2 hp_pos(top_x, top_center_y - health_size.y * 0.5f);

			draw_list->AddText(font, small_font_size, ImVec2(hp_pos.x + 1.0f * scale, hp_pos.y + 1.0f * scale),
				IM_COL32(20, 20, 20, 220), health_text);
			draw_list->AddText(font, small_font_size, hp_pos, hp_color, health_text);
		}

		if (settings_snapshot.show_armor)
		{
			static constexpr const char* slot_names[4] = { "H", "C", "L", "B" };
			float chip_x = panel_min.x + (panel_width - armor_width) * 0.5f;
			const float chip_y = panel_max.y - vertical_padding - chip_height;
			for (int slot = 0; slot < 4; ++slot)
			{
				const ImVec2 chip_min(chip_x, chip_y);
				const ImVec2 chip_max(chip_x + chip_width, chip_y + chip_height);
				const bool equipped = entry.armor[slot] != "---";
				draw_list->AddRectFilled(chip_min, chip_max,
					equipped ? IM_COL32(10, 10, 14, 180) : IM_COL32(5, 5, 8, 160), 2.0f * scale);
				draw_list->AddRect(chip_min, chip_max, armor_color(entry.armor[slot], equipped ? 0.50f : 0.15f),
					2.0f * scale, 0, std::max(1.0f, 0.8f * scale));
				const std::string label = std::string(slot_names[slot]) + " " + entry.armor[slot];
				const ImVec2 label_size = font->CalcTextSizeA(small_font_size, FLT_MAX, 0.0f, label.c_str());
				draw_list->AddText(font, small_font_size,
					ImVec2(chip_min.x + (chip_width - label_size.x) * 0.5f,
						chip_min.y + (chip_height - label_size.y) * 0.5f),
					armor_color(entry.armor[slot], equipped ? 1.0f : 0.55f), label.c_str());
				chip_x += chip_width + chip_gap;
			}
		}
	}
}

void modules::nametag::render_options()
{
    gui::settings::render(settings_, module_settings::nametag::schema());
}
