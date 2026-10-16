#include "grim_ui.hpp"
#include "hud_editor.hpp"

#include "../gui/gui.hpp"
#include "../gui/module_descriptions.hpp"
#include "../gui/surface_style.hpp"
#include "../gui/control_draw.hpp"
#include "../gui/ui_state.hpp"
#include "../gui/theme_palette.hpp"
#include "../modules/core/module.hpp"
#include "../modules/core/module_registry.hpp"
#include "../modules/render/arraylist.hpp"
#include "../modules/visual/player_esp.hpp"
#include "../modules/visual/spotify_hud.hpp"
#include "../friends/friends.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    ImU32 accent = IM_COL32(155, 69, 255, 255);
    ImU32 accent_hover = IM_COL32(173, 96, 255, 255);
    constexpr ImU32 primary = IM_COL32(244, 241, 247, 255);
    constexpr ImU32 secondary = IM_COL32(194, 189, 201, 255);
    constexpr ImU32 muted = IM_COL32(133, 128, 142, 255);
    constexpr ImU32 shell = IM_COL32(3, 3, 5, 178);
    // The shell already provides the glass tint. Sidebar is only a subtle
    // tonal layer so it never looks like a second opaque window inside it.
    constexpr ImU32 sidebar = IM_COL32(7, 6, 10, 54);
    constexpr ImU32 inspector = IM_COL32(16, 16, 21, 242);

    enum class page
    {
        combat,
        vape,
        movement,
        player,
        visuals,
        client,
        friends,
        configs,
        hud,
        themes
    };

    unsigned generate_session_uid()
    {
        const auto clock_seed = static_cast<unsigned>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        std::random_device device;
        std::mt19937 generator(clock_seed ^ device());
        return std::uniform_int_distribution<unsigned>(100000, 999999)(generator);
    }

    struct runtime_state
    {
        page current = page::combat;
        modules::module* selected = nullptr;
        modules::module* animated_module = nullptr;
        float inspector_animation = 0.0f;
        bool search_open = false;
        bool focus_search = false;
        float search_animation = 0.0f;
        int search_selection = 0;
        char search[96]{};
        std::string search_completion_target;
        std::size_t search_completion_cursor = 0;
        double search_completion_step_at = 0.0;
        bool search_completion_active = false;
        bool search_completion_pending = false;
        float player_preview_yaw = 0.0f;
        unsigned session_uid = generate_session_uid();
        modules::module* transfer_module = nullptr;
        ImVec2 transfer_min{};
        ImVec2 transfer_max{};
        float transfer_progress = 1.0f;
        bool positioned = false;
        ImVec2 position{};
        std::unordered_map<ImGuiID, float> hover_animation;
        std::unordered_map<ImGuiID, float> toggle_animation;
        std::unordered_map<ImGuiID, float> nav_hover_animation;
        std::unordered_map<ImGuiID, float> nav_active_animation;
        std::unordered_map<ImGuiID, float> card_selected_animation;
        std::unordered_map<ImGuiID, float> card_enabled_animation;
        std::unordered_map<modules::module*, float> options_content_height;
        float page_fade = 1.0f;
    } state;

    struct navigation_entry
    {
        page target;
        const char* label;
        const char* glyph;
    };

    constexpr navigation_entry feature_navigation[] = {
        { page::combat,   "Combat",   "\xEF\x81\x9B" },
        { page::movement, "Movement", "\xEF\x95\x94" },
        { page::player,   "Player",   "\xEF\x80\x87" },
        { page::visuals,  "Visuals",  "\xEF\x81\xAE" },
        { page::client,   "Client",   "\xEF\x87\x9E" },
        { page::vape,     "Vape",     "\xEF\x83\xA7" },
    };

    constexpr navigation_entry other_navigation[] = {
        { page::friends, "Friends", "\xEF\x83\x80" },
        { page::configs, "Configs", "\xEF\x81\xBB" },
        { page::themes,  "Themes",  "\xEF\x94\xBF" }, // palette (f53f)
        { page::hud,     "Hud",     "\xEF\x84\x88" },
    };

    float approach(float current, float target, float speed = 12.0f)
    {
        if (gui::aegis::get_preferences().reduce_motion) return target;
        const float dt = std::clamp(ImGui::GetIO().DeltaTime, 0.0001f, 0.05f);
        const float diff = target - current;
        if (std::abs(diff) < 0.0005f)
            return target;
        return current + diff * (1.0f - std::exp(-speed * dt));
    }

    ImU32 alpha(ImU32 color, float multiplier)
    {
        ImVec4 value = ImGui::ColorConvertU32ToFloat4(color);
        value.w *= std::clamp(multiplier, 0.0f, 1.0f);
        return ImGui::ColorConvertFloat4ToU32(value);
    }

    ImU32 blend(ImU32 a, ImU32 b, float amount)
    {
        const ImVec4 av = ImGui::ColorConvertU32ToFloat4(a);
        const ImVec4 bv = ImGui::ColorConvertU32ToFloat4(b);
        amount = std::clamp(amount, 0.0f, 1.0f);
        return ImGui::ColorConvertFloat4ToU32(ImVec4(
            av.x + (bv.x - av.x) * amount,
            av.y + (bv.y - av.y) * amount,
            av.z + (bv.z - av.z) * amount,
            av.w + (bv.w - av.w) * amount));
    }

    std::string title_case(const char* source)
    {
        std::string result = source ? source : "Unknown";
        bool upper = true;
        for (char& character : result)
        {
            if (upper && character >= 'a' && character <= 'z')
                character = static_cast<char>(character - 'a' + 'A');
            upper = character == ' ';
        }
        return result;
    }

    const char* description(const char* name)
    {
        return gui::module_description(name);
    }

    const char* page_label(page value)
    {
        switch (value)
        {
        case page::combat: return "Combat";
        case page::vape: return "Vape";
        case page::movement: return "Movement";
        case page::player: return "Player";
        case page::visuals: return "Visuals";
        case page::client: return "Client";
        case page::friends: return "Friends";
        case page::configs: return "Configs";
        case page::hud: return "Hud";
        case page::themes: return gui::text("Themes", "Temalar");
        }
        return "onestep";
    }

    const char* page_description(page value)
    {
        switch (value)
        {
        case page::combat: return "Combat assistance and timing";
        case page::vape: return gui::text("Independent combat engines", "Bağımsız combat motorları");
        case page::movement: return "Movement and bridging tools";
        case page::player: return "Player and inventory automation";
        case page::visuals: return "World visualization modules";
        case page::client: return "Client interface and safety";
        case page::friends: return "Trusted players in this session";
        case page::configs: return "Browse and manage community setups";
        case page::hud: return "Drag and position your on-screen overlays";
        case page::themes: return gui::theme::palettes[std::clamp(gui::active_theme, 0, gui::theme::count - 1)].name;
        }
        return "";
    }

    bool belongs_to_page(const modules::module* module, page target)
    {
        if (!module) return false;
        const char* category = module->get_category();
        if (target == page::combat) return std::strcmp(category, "Combat") == 0;
        if (target == page::vape) return std::strcmp(category, "Vape") == 0;
        if (target == page::movement) return std::strcmp(category, "Movement") == 0;
        if (target == page::player) return std::strcmp(category, "Player") == 0 || std::strcmp(category, "World") == 0;
        if (target == page::visuals) return std::strcmp(category, "Visual") == 0;
        if (target == page::client) return std::strcmp(category, "Client") == 0;
        if (target == page::hud) return std::strcmp(category, "Render") == 0;
        return false;
    }

    bool has_options(modules::module* module)
    {
        return module && module->get_options_height() > 45.0f;
    }

    bool matches(const modules::module* module, const char* query)
    {
        if (!module || !query || !*query) return true;
        std::string needle(query);
        std::string haystack = std::string(module->get_name()) + " " + description(module->get_name());
        std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::transform(haystack.begin(), haystack.end(), haystack.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return haystack.find(needle) != std::string::npos;
    }

    bool case_insensitive_prefix(const std::string& value, const std::string& target)
    {
        if (value.size() > target.size()) return false;
        for (std::size_t index = 0; index < value.size(); ++index)
            if (std::tolower(static_cast<unsigned char>(value[index])) !=
                std::tolower(static_cast<unsigned char>(target[index]))) return false;
        return true;
    }

    void open_search_result(modules::module* module)
    {
        if (!module) return;
        const char* category = module->get_category();
        if (std::strcmp(category, "Combat") == 0) state.current = page::combat;
        else if (std::strcmp(category, "Vape") == 0) state.current = page::vape;
        else if (std::strcmp(category, "Movement") == 0) state.current = page::movement;
        else if (std::strcmp(category, "Player") == 0 || std::strcmp(category, "World") == 0) state.current = page::player;
        else if (std::strcmp(category, "Visual") == 0) state.current = page::visuals;
        else if (std::strcmp(category, "Render") == 0) state.current = page::hud;
        else state.current = page::client;

        state.selected = has_options(module) ? module : nullptr;
        state.search_open = false;
        state.search_selection = 0;
        state.search_completion_active = false;
        state.search_completion_pending = false;
        state.search_completion_target.clear();
        state.search[0] = '\0';
    }

    int search_completion_callback(ImGuiInputTextCallbackData* data)
    {
        if (!data) return 0;
        if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion && data->UserData)
        {
            const modules::module* module = static_cast<const modules::module*>(data->UserData);
            state.search_completion_target = module->get_name();
            state.search_completion_cursor = std::min<std::size_t>(std::strlen(data->Buf), state.search_completion_target.size());
            state.search_completion_step_at = ImGui::GetTime();
            state.search_completion_active = state.search_completion_cursor < state.search_completion_target.size();
            state.search_completion_pending = false;
        }
        else if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways && state.search_completion_active &&
            ImGui::GetTime() >= state.search_completion_step_at)
        {
            state.search_completion_cursor = std::min(state.search_completion_cursor + 1, state.search_completion_target.size());
            const std::string value = state.search_completion_target.substr(0, state.search_completion_cursor);
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, value.c_str());
            state.search_completion_step_at = ImGui::GetTime() + 0.028;
            state.search_completion_active = state.search_completion_cursor < state.search_completion_target.size();
        }
        return 0;
    }

    void draw_text(ImDrawList* draw, ImFont* font, float size, const ImVec2& position, ImU32 color, const char* value)
    {
        draw->AddText(font ? font : ImGui::GetFont(), size, position, color, value ? value : "");
    }

    void icon(ImDrawList* draw, const char* glyph, const ImVec2& center, ImU32 color, float size = 16.0f)
    {
        ImFont* font = gui::icon_font ? gui::icon_font : ImGui::GetFont();
        const char* value = gui::icon_font ? glyph : "o";
        const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, value);
        draw->AddText(font, size, ImVec2(center.x - extent.x * 0.5f, center.y - extent.y * 0.5f), color, value);
    }

    bool navigation_button(const navigation_entry& entry, const ImVec2& position, float width)
    {
        ImGui::PushID(static_cast<int>(entry.target));
        ImGui::SetCursorScreenPos(position);
        ImGui::InvisibleButton("##navigation", ImVec2(width, 34.0f));
        const bool clicked = ImGui::IsItemClicked();
        const bool hovered = ImGui::IsItemHovered();
        const bool selected = state.current == entry.target;
        const ImGuiID id = ImGui::GetItemID();

        float& hover = state.nav_hover_animation[id];
        hover = approach(hover, hovered ? 1.0f : 0.0f, 13.0f);

        float& active = state.nav_active_animation[id];
        active = approach(active, selected ? 1.0f : 0.0f, 14.0f);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        if (active > 0.001f)
        {
            draw->AddRectFilled(position, ImVec2(position.x + width, position.y + 34.0f),
                alpha(accent, active), 7.0f);
            const float bar_h = 18.0f * active;
            const float bar_y = position.y + (34.0f - bar_h) * 0.5f;
            draw->AddRectFilled(ImVec2(position.x + 2.0f, bar_y),
                ImVec2(position.x + 5.0f, bar_y + bar_h),
                IM_COL32(255, 255, 255, static_cast<int>(240.0f * active)), 1.5f);
        }
        else if (hover > 0.001f)
        {
            draw->AddRectFilled(position, ImVec2(position.x + width, position.y + 34.0f),
                IM_COL32(255, 255, 255, static_cast<int>(12.0f * hover)), 7.0f);
        }

        const ImU32 base_col = blend(muted, secondary, hover);
        const ImU32 text_col = blend(base_col, primary, active);

        icon(draw, entry.glyph, ImVec2(position.x + 18.0f, position.y + 17.0f), text_col, 15.0f);

        ImFont* label_font = gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont();
        constexpr float label_size = 15.5f;
        const char* label = entry.target == page::themes ? gui::text("Themes", "Temalar") : entry.label;
        const ImVec2 label_extent = label_font->CalcTextSizeA(label_size, FLT_MAX, 0.0f, label);
        draw_text(draw, label_font, label_size,
            ImVec2(position.x + 38.0f, position.y + (34.0f - label_extent.y) * 0.5f),
            text_col, label);
        ImGui::PopID();
        if (clicked && state.current != entry.target)
        {
            state.current = entry.target;
            state.page_fade = 0.0f;
            state.search_open = false;
            if (entry.target == page::themes) state.selected = nullptr;
        }
        return clicked;
    }

    bool toggle(const char* id, bool* value, const ImVec2& position)
    {
        constexpr float toggle_width = 38.0f;
        constexpr float toggle_height = 21.0f;
        constexpr float knob_travel = 17.0f;
        constexpr float knob_radius = 7.2f;

        ImGui::SetCursorScreenPos(position);
        ImGui::InvisibleButton(id, ImVec2(toggle_width, toggle_height));
        const bool clicked = ImGui::IsItemClicked();
        if (clicked) *value = !*value;
        float& progress = state.toggle_animation[ImGui::GetItemID()];
        progress = approach(progress, *value ? 1.0f : 0.0f, 13.0f);
        const float eased = progress * progress * (3.0f - 2.0f * progress);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImU32 track = blend(IM_COL32(52, 49, 58, 255), accent, eased);

        // Snap the track and the knob onto the same whole-pixel grid. With raw
        // fractional coordinates the rounded ends land on different raster edges
        // and the capsule visibly breaks into stair steps.
        const ImVec2 track_min(std::floor(position.x), std::floor(position.y));
        const ImVec2 track_max(std::ceil(position.x + toggle_width), std::ceil(position.y + toggle_height));
        draw->AddRectFilled(track_min, track_max, track, toggle_height * 0.5f);

        // Round the travel so the knob hops between whole pixels instead of
        // drifting through halves, which is what makes the edge crawl.
        const float knob_x = std::floor(position.x + 10.5f + std::round(eased * knob_travel));
        const float knob_y = std::floor(position.y + 10.5f);
        draw->AddCircleFilled(ImVec2(knob_x, knob_y), knob_radius,
            IM_COL32(239, 237, 243, 255), 0);
        return clicked;
    }

    void render_card(modules::module* module, float width, float height)
    {
        if (!module) return;
        ImGui::PushID(module->get_id());
        const ImVec2 minimum = ImGui::GetCursorScreenPos();
        const ImVec2 maximum(minimum.x + width, minimum.y + height);
        ImGui::InvisibleButton("##module_card", ImVec2(width, height));
        const ImGuiID id = ImGui::GetItemID();
        const bool hovered = ImGui::IsItemHovered();
        const bool clicked = ImGui::IsItemClicked();

        float& hover = state.hover_animation[id];
        hover = approach(hover, hovered ? 1.0f : 0.0f, 13.0f);

        const bool is_selected = (state.selected == module);
        float& sel_anim = state.card_selected_animation[id];
        sel_anim = approach(sel_anim, is_selected ? 1.0f : 0.0f, 14.0f);

        float& en_anim = state.card_enabled_animation[id];
        en_anim = approach(en_anim, module->enabled ? 1.0f : 0.0f, 14.0f);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        gui::surface_style::module_card(draw, minimum, maximum, accent, hover, sel_anim);

        const std::string title = title_case(module->get_name());
        const float title_y = minimum.y + std::max(7.0f, (height - 48.0f) * 0.5f + 5.0f);
        const ImU32 title_col = blend(primary, accent_hover, en_anim);
        draw_text(draw, gui::ui_bold_font, 16.5f, ImVec2(minimum.x + 12.0f, title_y),
            title_col, title.c_str());

        const ImVec4 clip(minimum.x + 12.0f, title_y + 23.0f, maximum.x - 11.0f, maximum.y - 5.0f);
        draw->AddText(gui::ui_font ? gui::ui_font : ImGui::GetFont(), 13.5f,
            ImVec2(minimum.x + 12.0f, title_y + 24.0f), muted, description(module->get_name()), nullptr, 0.0f, &clip);

        if (clicked)
        {
            if (std::strcmp(module->get_id(), "panic") == 0)
                gui::request_panic();
            else if (has_options(module))
            {
                state.selected = (state.selected == module) ? nullptr : module;
            }
            else
                module->enabled = !module->enabled;
        }
        ImGui::PopID();
    }

    void render_grid(const ImVec2& minimum, const ImVec2& size)
    {
        ImGui::SetCursorScreenPos(minimum);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        if (ImGui::BeginChild("##grim_module_grid", size, ImGuiChildFlags_None))
        {
            const float gap = 8.0f;
            const float width = (ImGui::GetContentRegionAvail().x - gap) * 0.5f;
            std::vector<modules::module*> visible;
            for (modules::module* module : modules::get_modules())
            {
                if (!belongs_to_page(module, state.current)) continue;
                visible.push_back(module);
            }
            const std::size_t row_count = std::max<std::size_t>(1, (visible.size() + 1) / 2);
            const float row_pitch = std::clamp(size.y / static_cast<float>(row_count), 65.0f, 82.0f);
            const float card_height = std::max(57.0f, row_pitch - 8.0f);
            for (std::size_t index = 0; index < visible.size(); index += 2)
            {
                const ImVec2 row = ImGui::GetCursorScreenPos();
                ImGui::SetCursorScreenPos(row);
                render_card(visible[index], width, card_height);
                if (index + 1 < visible.size())
                {
                    ImGui::SetCursorScreenPos(ImVec2(row.x + width + gap, row.y));
                    render_card(visible[index + 1], width, card_height);
                }
                ImGui::SetCursorScreenPos(ImVec2(row.x, row.y + row_pitch));
            }
            if (visible.empty())
            {
                ImGui::Dummy(ImVec2(0.0f, 70.0f));
                const char* empty = state.search[0] ? "No matching modules" : "No modules in this category";
                const float text_width = ImGui::CalcTextSize(empty).x;
                ImGui::SetCursorPosX(std::max(0.0f, (ImGui::GetContentRegionAvail().x - text_width) * 0.5f));
                ImGui::TextColored(ImVec4(0.52f, 0.50f, 0.56f, 1.0f), "%s", empty);
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }

    void render_module_transfer(const ImVec2& inspector_position)
    {
        if (!state.transfer_module || state.transfer_progress >= 1.0f) return;
        const float dt = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.05f);
        state.transfer_progress = std::min(1.0f, state.transfer_progress + dt / 0.30f);
        const float t = state.transfer_progress * state.transfer_progress * (3.0f - 2.0f * state.transfer_progress);
        const ImVec2 destination_min(inspector_position.x, inspector_position.y);
        const ImVec2 destination_max(inspector_position.x + 304.0f, inspector_position.y + 52.0f);
        const ImVec2 minimum(
            state.transfer_min.x + (destination_min.x - state.transfer_min.x) * t,
            state.transfer_min.y + (destination_min.y - state.transfer_min.y) * t);
        const ImVec2 maximum(
            state.transfer_max.x + (destination_max.x - state.transfer_max.x) * t,
            state.transfer_max.y + (destination_max.y - state.transfer_max.y) * t);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float visibility = 1.0f - std::max(0.0f, (t - 0.68f) / 0.32f);
        draw->AddRectFilled(minimum, maximum, alpha(IM_COL32(42, 25, 57, 230), visibility), 10.0f);
        draw->AddRect(minimum, maximum, alpha(accent_hover, 0.52f * visibility), 10.0f, 0, 1.3f);
        const std::string title = title_case(state.transfer_module->get_name());
        ImFont* font = gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont();
        const float font_size = 16.0f;
        const ImVec2 extent = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, title.c_str());
        draw->AddText(font, font_size,
            ImVec2(minimum.x + 13.0f, minimum.y + std::max(0.0f, (maximum.y - minimum.y - extent.y) * 0.5f)),
            alpha(primary, visibility), title.c_str());
        if (state.transfer_progress >= 1.0f) state.transfer_module = nullptr;
    }

    void render_hud_editor_launcher(const ImVec2& minimum, const ImVec2& size)
    {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 maximum(minimum.x + size.x, minimum.y + size.y);
        draw->AddRectFilled(minimum, maximum, IM_COL32(3, 4, 7, 92), 12.0f);
        draw->AddRect(minimum, maximum, IM_COL32(255, 255, 255, 10), 12.0f);

        draw_text(draw, gui::ui_bold_font, 18.0f, ImVec2(minimum.x + 20.0f, minimum.y + 19.0f), primary, "HUD Layout");
        draw_text(draw, gui::ui_font, 13.5f, ImVec2(minimum.x + 20.0f, minimum.y + 46.0f), muted,
            "Place Watermark, ArrayList and Spotify HUD directly on the game screen");

        const ImVec2 button_size(214.0f, 42.0f);
        const ImVec2 button_min(minimum.x + 20.0f, minimum.y + 82.0f);
        ImGui::SetCursorScreenPos(button_min);
        ImGui::InvisibleButton("##open_hud_layout_editor", button_size);
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked())
            gui::hud_layout_editor_active = true;
        draw->AddRectFilled(button_min, ImVec2(button_min.x + button_size.x, button_min.y + button_size.y),
            alpha(hovered ? accent_hover : accent, hovered ? 0.88f : 0.82f), 9.0f);
        ImFont* font = gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont();
        const char* label = "Edit on screen";
        const ImVec2 label_size = font->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, label);
        draw_text(draw, font, 14.0f, ImVec2(button_min.x + (button_size.x - label_size.x) * 0.5f,
            button_min.y + (button_size.y - label_size.y) * 0.5f), primary, label);

        draw_text(draw, gui::ui_font, 12.5f, ImVec2(minimum.x + 20.0f, minimum.y + 143.0f), muted,
            gui::text("Select, align and drag overlays. Layout is saved automatically.", "HUD seç, hizala ve sürükle. Yerleşim otomatik kaydedilir."));
    }

    void render_direct_hud_editor(const ImGuiViewport* viewport)
    {
        gui::hud_editor::render(viewport);
    }

    void render_search(const ImVec2& shell_position)
    {
        const float target = state.search_open ? 1.0f : 0.0f;
        state.search_animation = approach(state.search_animation, target, 13.0f);
        if (!state.search_open && state.search_animation < 0.01f) return;
        if (state.search_open && ImGui::IsKeyPressed(ImGuiKey_Escape)) state.search_open = false;
        const float eased = state.search_animation * state.search_animation * (3.0f - 2.0f * state.search_animation);
        auto collect_results = []
        {
            std::vector<modules::module*> found;
            if (state.search[0])
                for (modules::module* module : modules::get_modules())
                    if (matches(module, state.search)) found.push_back(module);
            if (found.size() > 7) found.resize(7);
            return found;
        };
        std::vector<modules::module*> results = collect_results();
        state.search_selection = results.empty() ? 0 : std::clamp(state.search_selection, 0, static_cast<int>(results.size()) - 1);
        constexpr float result_height = 46.0f;
        const float full_height = 108.0f + static_cast<float>(std::max<std::size_t>(1, results.size())) * result_height;
        const ImVec2 size(610.0f * (0.94f + eased * 0.06f), (54.0f + (full_height - 54.0f) * eased));
        const ImVec2 position(shell_position.x + 960.0f - size.x - 18.0f, shell_position.y + 8.0f);
        ImGui::SetCursorScreenPos(position);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::max(0.02f, eased));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 13.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.012f, 0.012f, 0.017f, 0.94f));
        if (ImGui::BeginChild("##grim_search", size, ImGuiChildFlags_Border,
            ImGuiWindowFlags_NoScrollWithMouse))
        {
            if (state.focus_search)
            {
                ImGui::SetKeyboardFocusHere();
                state.focus_search = false;
            }
            ImGui::SetNextItemWidth(-1.0f);
            if (state.search_completion_pending && ImGui::GetTime() >= state.search_completion_step_at)
            {
                state.search_completion_pending = false;
                state.search_completion_active = true;
            }
            const std::string query_before = state.search;
            const bool completion_was_active = state.search_completion_active;
            modules::module* completion = results.empty() ? nullptr : results[static_cast<std::size_t>(state.search_selection)];
            const bool submitted = ImGui::InputTextWithHint("##search_input", "Search modules", state.search,
                IM_ARRAYSIZE(state.search), ImGuiInputTextFlags_EnterReturnsTrue |
                    ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackAlways,
                search_completion_callback, completion);
            if (query_before != state.search)
            {
                state.search_selection = 0;
                results = collect_results();
                if (!completion_was_active && !results.empty())
                {
                    const std::string target_name = results.front()->get_name();
                    if (state.search[0] && case_insensitive_prefix(state.search, target_name) &&
                        std::strlen(state.search) < target_name.size())
                    {
                        state.search_completion_target = target_name;
                        state.search_completion_cursor = std::strlen(state.search);
                        state.search_completion_step_at = ImGui::GetTime() + 0.34;
                        state.search_completion_pending = true;
                    }
                    else
                    {
                        state.search_completion_pending = false;
                        state.search_completion_active = false;
                    }
                }
                else if (results.empty())
                {
                    state.search_completion_pending = false;
                    state.search_completion_active = false;
                }
            }
            if (!results.empty())
            {
                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
                    state.search_selection = (state.search_selection + 1) % static_cast<int>(results.size());
                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
                    state.search_selection = (state.search_selection + static_cast<int>(results.size()) - 1) % static_cast<int>(results.size());
                if (submitted)
                {
                    open_search_result(results[static_cast<std::size_t>(state.search_selection)]);
                    ImGui::EndChild();
                    ImGui::PopStyleColor();
                    ImGui::PopStyleVar(3);
                    return;
                }
            }
            if (state.search[0])
            {
                ImGui::TextColored(ImVec4(0.48f, 0.45f, 0.52f, 1.0f), "Results");
                if (results.empty())
                {
                    const ImVec2 row = ImGui::GetCursorScreenPos();
                    const float row_width = ImGui::GetContentRegionAvail().x;
                    const char* empty = "No matching modules";
                    ImFont* font = gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont();
                    const ImVec2 extent = font->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, empty);
                    draw_text(ImGui::GetWindowDrawList(), font, 14.0f,
                        ImVec2(row.x + (row_width - extent.x) * 0.5f, row.y + (42.0f - extent.y) * 0.5f), muted, empty);
                }
                for (std::size_t result_index = 0; result_index < results.size(); ++result_index)
                {
                    modules::module* module = results[result_index];
                    ImGui::PushID(module->get_id());
                    const ImVec2 row = ImGui::GetCursorScreenPos();
                    const float row_width = ImGui::GetContentRegionAvail().x;
                    constexpr float row_height = result_height;
                    ImGui::InvisibleButton("##result", ImVec2(row_width, row_height));
                    const bool keyboard_selected = static_cast<int>(result_index) == state.search_selection;
                    if (ImGui::IsItemHovered()) state.search_selection = static_cast<int>(result_index);
                    if (ImGui::IsItemHovered() || keyboard_selected)
                        ImGui::GetWindowDrawList()->AddRectFilled(row,
                            ImVec2(row.x + row_width, row.y + row_height),
                            keyboard_selected ? alpha(accent, 28.0f / 255.0f) : IM_COL32(255, 255, 255, 8), 7.0f);
                    const std::string title = std::string("Modules  â¢  ") + title_case(module->get_name());
                    const std::string safe_title = title_case(module->get_category()) + "  -  " + title_case(module->get_name());
                    ImFont* font = gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont();
                    constexpr float font_size = 14.5f;
                    const ImVec2 extent = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, safe_title.c_str());
                    const ImVec4 clip(row.x + 10.0f, row.y, row.x + row_width - 10.0f, row.y + row_height);
                    ImGui::GetWindowDrawList()->AddText(font, font_size,
                        ImVec2(row.x + 10.0f, row.y + (row_height - extent.y) * 0.5f), secondary,
                        safe_title.c_str(), nullptr, 0.0f, &clip);
                    if (ImGui::IsItemClicked())
                        open_search_result(module);
                    ImGui::PopID();
                }
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }

    void render_inspector(const ImVec2& position)
    {
        if (!state.animated_module || state.inspector_animation < 0.005f)
            return;

        modules::module* module = state.animated_module;
        const float eased = state.inspector_animation * state.inspector_animation * (3.0f - 2.0f * state.inspector_animation);
        const auto measured = state.options_content_height.find(module);
        const float options_height = measured != state.options_content_height.end()
            ? measured->second : module->get_options_height();
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float maximum_height = std::max(150.0f, std::min(620.0f,
            viewport->WorkPos.y + viewport->WorkSize.y - position.y - 12.0f));
        const float desired_height = std::clamp(options_height + 142.0f, 150.0f, maximum_height);
        const float visible_height = 50.0f + (desired_height - 50.0f) * eased;
        const float slide_x = (1.0f - eased) * 12.0f;
        const ImVec2 anim_pos(position.x - slide_x, position.y);
        const ImVec2 maximum(anim_pos.x + 304.0f, anim_pos.y + visible_height);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->PushClipRect(position, ImVec2(position.x + 304.0f, position.y + visible_height), true);
        draw->AddRectFilled(anim_pos, maximum, alpha(inspector, eased), 12.0f);
        draw->AddRect(anim_pos, maximum, alpha(IM_COL32(255, 255, 255, 22), eased), 12.0f);
        draw->AddLine(ImVec2(anim_pos.x + 16.0f, anim_pos.y + 46.0f),
            ImVec2(anim_pos.x + 288.0f, anim_pos.y + 46.0f), alpha(IM_COL32(255, 255, 255, 14), eased));

        const std::string title = title_case(module->get_name());
        ImFont* title_font = gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont();
        const ImVec4 title_clip(anim_pos.x + 16.0f, anim_pos.y + 10.0f, anim_pos.x + 263.0f, anim_pos.y + 42.0f);
        draw->AddText(title_font, 16.0f, ImVec2(anim_pos.x + 16.0f, anim_pos.y + 15.0f),
            alpha(primary, eased), title.c_str(), nullptr, 0.0f, &title_clip);

        ImGui::PushID(module->get_id());
        ImGui::SetCursorScreenPos(ImVec2(anim_pos.x + 270.0f, anim_pos.y + 10.0f));
        ImGui::InvisibleButton("##close_inspector", ImVec2(24.0f, 24.0f));
        if (ImGui::IsItemClicked()) state.selected = nullptr;

        const ImVec2 close(anim_pos.x + 282.0f, anim_pos.y + 22.0f);
        if (ImGui::IsItemHovered())
            draw->AddCircleFilled(close, 11.0f, alpha(IM_COL32(255, 255, 255, 14), eased));
        gui::control_draw::close_icon(draw, close, alpha(ImGui::IsItemHovered() ? primary : secondary, eased));

        draw_text(draw, gui::ui_font, 13.5f, ImVec2(anim_pos.x + 16.0f, anim_pos.y + 62.0f), alpha(secondary, eased), gui::text("Enabled", "Etkin"));
        toggle("##module_enabled", &module->enabled, ImVec2(anim_pos.x + 251.0f, anim_pos.y + 57.0f));

        draw_text(draw, gui::ui_font, 13.5f, ImVec2(anim_pos.x + 16.0f, anim_pos.y + 95.0f), alpha(secondary, eased), gui::text("Keybind", "Tuş"));
        ImGui::SetCursorScreenPos(ImVec2(anim_pos.x + 215.0f, anim_pos.y + 87.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * eased);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 1.0f, 0.045f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.085f));
        module->key_bind_selector();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();
        draw->AddLine(ImVec2(anim_pos.x + 16.0f, anim_pos.y + 118.0f),
            ImVec2(anim_pos.x + 288.0f, anim_pos.y + 118.0f), alpha(IM_COL32(255, 255, 255, 18), eased));

        if (visible_height > 128.0f)
        {
            ImGui::SetCursorScreenPos(ImVec2(anim_pos.x + 6.0f, anim_pos.y + 126.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 4.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 3.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 4.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * eased);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            const ImVec4 control_accent = gui::get_dynamic_color().Value;
            ImGui::PushStyleColor(ImGuiCol_CheckMark, control_accent);
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, control_accent);
            if (ImGui::BeginChild("##inspector_options", ImVec2(292.0f, std::max(1.0f, visible_height - 134.0f)),
                ImGuiChildFlags_AlwaysUseWindowPadding))
            {
                const ImGuiWindow* options_window = ImGui::GetCurrentWindow();
                const float content_start = options_window->DC.CursorStartPos.y;
                ImGui::PushItemWidth(-8.0f);
                module->render_options();
                ImGui::PopItemWidth();
                // Include off-screen rows, so scrolling and conditional settings
                // never change the measured height or leave a guessed empty tail.
                state.options_content_height[module] = std::max(0.0f,
                    std::max(options_window->DC.CursorMaxPos.y,
                        options_window->DC.CursorPos.y - ImGui::GetStyle().ItemSpacing.y) - content_start);
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(4);
        }
        ImGui::PopID();
        draw->PopClipRect();
    }

    bool player_preview_requested()
    {
        return state.current == page::visuals && state.selected &&
            std::strcmp(state.selected->get_id(), "player_esp") == 0;
    }

    void render_player_preview(const ImVec2& position)
    {
        static double last_skin_refresh = -10.0;
        const double now = ImGui::GetTime();
        if (now - last_skin_refresh > 0.75)
        {
            modules::player_esp::refresh_local_skin_texture();
            last_skin_refresh = now;
        }
        constexpr ImVec2 size(260.0f, 460.0f);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(position, ImVec2(position.x + size.x, position.y + size.y),
            IM_COL32(3, 4, 7, 222), 16.0f);
        draw->AddRect(position, ImVec2(position.x + size.x, position.y + size.y),
            IM_COL32(255, 255, 255, 15), 16.0f);

        const ImVec2 header_max(position.x + size.x, position.y + 52.0f);
        draw->AddRectFilled(position, header_max, IM_COL32(255, 255, 255, 5), 16.0f,
            ImDrawFlags_RoundCornersTop);
        icon(draw, "\xEF\x80\x87", ImVec2(position.x + 34.0f, position.y + 24.0f), accent_hover, 16.0f);
        draw_text(draw, gui::ui_bold_font, 16.5f, ImVec2(position.x + 54.0f, position.y + 13.0f), primary, "Player Preview");
        draw_text(draw, gui::ui_bold_font, 11.5f, ImVec2(position.x + 211.0f, position.y + 17.0f), accent_hover, "LIVE");

        const ImVec2 stage_min(position.x + 18.0f, position.y + 67.0f);
        const ImVec2 stage_max(position.x + 242.0f, position.y + 350.0f);
        draw->AddRectFilled(stage_min, stage_max, IM_COL32(1, 2, 4, 202), 12.0f);
        draw->AddRect(stage_min, stage_max, IM_COL32(255, 255, 255, 12), 12.0f);
        const ImVec2 box_min(position.x + 68.0f, position.y + 88.0f);
        const ImVec2 box_max(position.x + 192.0f, position.y + 332.0f);

        const auto settings = static_cast<modules::player_esp*>(state.selected)->get_preview_settings();
        ImGui::PushID("player_preview_stage");
        ImGui::SetCursorScreenPos(stage_min);
        ImGui::InvisibleButton("##rotate", ImVec2(stage_max.x - stage_min.x, stage_max.y - stage_min.y));
        const bool stage_hovered = ImGui::IsItemHovered();
        if (settings.mode == 2 && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            state.player_preview_yaw += ImGui::GetIO().MouseDelta.x * 0.018f;
        if (settings.mode == 2 && stage_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            state.player_preview_yaw = 0.0f;
        ImGui::PopID();

        ImVec4 preview_color(settings.color[0], settings.color[1], settings.color[2], settings.color[3]);
        if (settings.theme_color)
            preview_color = gui::get_dynamic_color().Value;
        const ImU32 esp_color = ImGui::ColorConvertFloat4ToU32(preview_color);

        if (settings.mode == 2)
        {
            ImVec4 fill = preview_color;
            fill.w = std::clamp(settings.fill_opacity, 0.03f, 0.45f);
            draw->AddRectFilled(box_min, box_max, ImGui::ColorConvertFloat4ToU32(fill), 5.0f);
        }

        const std::uintptr_t skin_texture_value = modules::player_esp::local_skin_texture_id();
        if (skin_texture_value)
        {
            const ImTextureID skin = reinterpret_cast<ImTextureID>(skin_texture_value);
            struct face_uv
            {
                float u0;
                float v0;
                float u1;
                float v1;
            };
            float normalized_yaw = settings.mode == 2 ? std::fmod(state.player_preview_yaw, 6.28318530718f) : 0.0f;
            if (normalized_yaw < 0.0f) normalized_yaw += 6.28318530718f;
            const int direction = static_cast<int>(std::floor((normalized_yaw + 0.78539816339f) / 1.57079632679f)) % 4;
            const float model_scale_x = direction == 1 || direction == 3 ? 0.70f : 1.0f;
            auto skin_part = [&](const ImVec2& minimum, const ImVec2& maximum,
                const std::array<face_uv, 4>& faces)
            {
                const float model_center = position.x + 130.0f;
                const float part_center = model_center + ((minimum.x + maximum.x) * 0.5f - model_center) * model_scale_x;
                const float half_width = (maximum.x - minimum.x) * 0.5f * model_scale_x;
                const ImVec2 scaled_min(part_center - half_width, minimum.y);
                const ImVec2 scaled_max(part_center + half_width, maximum.y);
                const face_uv& uv = faces[static_cast<std::size_t>(direction)];
                draw->AddImage(skin, scaled_min, scaled_max,
                    ImVec2(uv.u0 / 64.0f, uv.v0 / 64.0f), ImVec2(uv.u1 / 64.0f, uv.v1 / 64.0f));
            };
            const std::array<face_uv, 4> head{{ { 8, 8, 16, 16 }, { 0, 8, 8, 16 }, { 24, 8, 32, 16 }, { 16, 8, 24, 16 } }};
            const std::array<face_uv, 4> hat{{ { 40, 8, 48, 16 }, { 32, 8, 40, 16 }, { 56, 8, 64, 16 }, { 48, 8, 56, 16 } }};
            const std::array<face_uv, 4> torso{{ { 20, 20, 28, 32 }, { 16, 20, 20, 32 }, { 32, 20, 40, 32 }, { 28, 20, 32, 32 } }};
            const std::array<face_uv, 4> right_arm{{ { 44, 20, 48, 32 }, { 40, 20, 44, 32 }, { 52, 20, 56, 32 }, { 48, 20, 52, 32 } }};
            const std::array<face_uv, 4> left_arm{{ { 36, 52, 40, 64 }, { 32, 52, 36, 64 }, { 44, 52, 48, 64 }, { 40, 52, 44, 64 } }};
            const std::array<face_uv, 4> right_leg{{ { 4, 20, 8, 32 }, { 0, 20, 4, 32 }, { 12, 20, 16, 32 }, { 8, 20, 12, 32 } }};
            const std::array<face_uv, 4> left_leg{{ { 20, 52, 24, 64 }, { 16, 52, 20, 64 }, { 28, 52, 32, 64 }, { 24, 52, 28, 64 } }};
            skin_part(ImVec2(position.x + 100.0f, position.y + 101.0f), ImVec2(position.x + 160.0f, position.y + 161.0f), head);
            skin_part(ImVec2(position.x + 100.0f, position.y + 101.0f), ImVec2(position.x + 160.0f, position.y + 161.0f), hat);
            skin_part(ImVec2(position.x + 94.0f, position.y + 165.0f), ImVec2(position.x + 166.0f, position.y + 261.0f), torso);
            skin_part(ImVec2(position.x + 70.0f, position.y + 165.0f), ImVec2(position.x + 94.0f, position.y + 261.0f), right_arm);
            skin_part(ImVec2(position.x + 166.0f, position.y + 165.0f), ImVec2(position.x + 190.0f, position.y + 261.0f), left_arm);
            skin_part(ImVec2(position.x + 98.0f, position.y + 261.0f), ImVec2(position.x + 128.0f, position.y + 328.0f), right_leg);
            skin_part(ImVec2(position.x + 132.0f, position.y + 261.0f), ImVec2(position.x + 162.0f, position.y + 328.0f), left_leg);
        }
        else
        {
            const ImU32 body = alpha(blend(IM_COL32(28, 30, 38, 255), accent, 0.58f), 0.88f);
            const ImU32 body_dark = alpha(blend(IM_COL32(20, 22, 28, 255), accent, 0.35f), 0.92f);
            draw->AddRectFilled(ImVec2(position.x + 100.0f, position.y + 101.0f), ImVec2(position.x + 160.0f, position.y + 161.0f), body, 2.0f);
            draw->AddRectFilled(ImVec2(position.x + 94.0f, position.y + 165.0f), ImVec2(position.x + 166.0f, position.y + 261.0f), body, 2.0f);
            draw->AddRectFilled(ImVec2(position.x + 70.0f, position.y + 165.0f), ImVec2(position.x + 94.0f, position.y + 261.0f), body_dark, 2.0f);
            draw->AddRectFilled(ImVec2(position.x + 166.0f, position.y + 165.0f), ImVec2(position.x + 190.0f, position.y + 261.0f), body_dark, 2.0f);
            draw->AddRectFilled(ImVec2(position.x + 98.0f, position.y + 261.0f), ImVec2(position.x + 128.0f, position.y + 328.0f), body_dark, 2.0f);
            draw->AddRectFilled(ImVec2(position.x + 132.0f, position.y + 261.0f), ImVec2(position.x + 162.0f, position.y + 328.0f), body_dark, 2.0f);
        }
        if (settings.mode == 0)
            draw->AddRect(box_min, box_max, esp_color, 4.0f, 0, std::max(1.0f, settings.thickness));
        else if (settings.mode == 1)
        {
            const float length = 24.0f;
            const float thickness = std::max(1.0f, settings.thickness);
            draw->AddLine(box_min, ImVec2(box_min.x + length, box_min.y), esp_color, thickness);
            draw->AddLine(box_min, ImVec2(box_min.x, box_min.y + length), esp_color, thickness);
            draw->AddLine(ImVec2(box_max.x, box_min.y), ImVec2(box_max.x - length, box_min.y), esp_color, thickness);
            draw->AddLine(ImVec2(box_max.x, box_min.y), ImVec2(box_max.x, box_min.y + length), esp_color, thickness);
            draw->AddLine(ImVec2(box_min.x, box_max.y), ImVec2(box_min.x + length, box_max.y), esp_color, thickness);
            draw->AddLine(ImVec2(box_min.x, box_max.y), ImVec2(box_min.x, box_max.y - length), esp_color, thickness);
            draw->AddLine(box_max, ImVec2(box_max.x - length, box_max.y), esp_color, thickness);
            draw->AddLine(box_max, ImVec2(box_max.x, box_max.y - length), esp_color, thickness);
        }
        else
        {
            draw->AddRect(box_min, box_max, esp_color, 4.0f, 0, std::max(1.0f, settings.thickness));
            draw->AddLine(ImVec2(box_min.x, box_min.y), ImVec2(box_min.x + 14.0f, box_min.y - 8.0f), esp_color, settings.thickness);
            draw->AddLine(ImVec2(box_max.x, box_min.y), ImVec2(box_max.x + 14.0f, box_min.y - 8.0f), esp_color, settings.thickness);
        }
        if (settings.health)
        {
            draw->AddRectFilled(ImVec2(position.x + 57.0f, position.y + 88.0f), ImVec2(position.x + 62.0f, position.y + 332.0f), IM_COL32(16, 18, 24, 205), 2.5f);
            draw->AddRectFilled(ImVec2(position.x + 58.0f, position.y + 137.0f), ImVec2(position.x + 61.0f, position.y + 331.0f), IM_COL32(78, 230, 132, 255), 2.0f);
        }
        if (settings.names)
        {
            const char* player_name = "Player";
            const ImVec2 extent = (gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont())->CalcTextSizeA(13.0f, FLT_MAX, 0.0f, player_name);
            draw_text(draw, gui::ui_bold_font, 13.0f, ImVec2(position.x + 130.0f - extent.x * 0.5f, position.y + 72.0f), primary, player_name);
        }
        if (settings.distance)
            draw_text(draw, gui::ui_bold_font, 12.0f, ImVec2(position.x + 164.0f, position.y + 335.0f), muted, "4.2m");
        if (settings.mode == 2 && stage_hovered)
        {
            const char* hint = "Drag to rotate";
            ImFont* hint_font = gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont();
            const ImVec2 extent = hint_font->CalcTextSizeA(10.5f, FLT_MAX, 0.0f, hint);
            draw->AddRectFilled(ImVec2(position.x + 130.0f - extent.x * 0.5f - 7.0f, position.y + 315.0f),
                ImVec2(position.x + 130.0f + extent.x * 0.5f + 7.0f, position.y + 337.0f), IM_COL32(4, 4, 7, 188), 6.0f);
            draw_text(draw, hint_font, 10.5f,
                ImVec2(position.x + 130.0f - extent.x * 0.5f, position.y + 320.0f), secondary, hint);
        }

        struct metric { const char* name; const char* value; };
        const metric metrics[] = {
            { "Style", settings.mode == 0 ? "2D Box" : (settings.mode == 1 ? "Corners" : "3D Box") },
            { "Health", settings.health ? "Visible" : "Hidden" },
            { "Distance", settings.distance ? "Visible" : "Hidden" }
        };
        float metric_x = position.x + 18.0f;
        for (const metric& value : metrics)
        {
            const ImVec2 chip(metric_x, position.y + 365.0f);
            draw->AddRectFilled(chip, ImVec2(metric_x + 69.0f, position.y + 415.0f), IM_COL32(13, 14, 19, 210), 8.0f);
            draw_text(draw, gui::ui_bold_font, 10.5f, ImVec2(chip.x + 8.0f, chip.y + 8.0f), muted, value.name);
            draw_text(draw, gui::ui_bold_font, 12.0f, ImVec2(chip.x + 8.0f, chip.y + 26.0f), primary, value.value);
            metric_x += 77.0f;
        }
        char thickness[48]{};
        std::snprintf(thickness, sizeof(thickness), "Thickness %.1f  /  Alpha %d%%", settings.thickness,
            static_cast<int>(preview_color.w * 100.0f));
        draw_text(draw, gui::ui_bold_font, 12.0f, ImVec2(position.x + 20.0f, position.y + 432.0f), muted, thickness);
    }
}

void gui::grim::reset_runtime_state()
{
    state = {};
}

void gui::grim::render(const ImGuiViewport* viewport, const page_callbacks& callbacks)
{
    if (!viewport) return;
    accent = gui::get_dynamic_color();
    accent_hover = blend(accent, primary, 0.16f);
    if (gui::hud_layout_editor_active)
    {
        gui::hud_editor_active = true;
        render_direct_hud_editor(viewport);
        return;
    }
    gui::hud_editor_active = false;
    ImGui::GetBackgroundDrawList()->AddRectFilled(viewport->Pos,
        ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y), IM_COL32(0, 0, 0, 32));
    constexpr ImVec2 main_size(960.0f, 620.0f);
    constexpr float sidebar_width = 204.0f;
    constexpr float inspector_gap = 16.0f;
    constexpr float preview_gap = 16.0f;
    constexpr float preview_width = 260.0f;
    const bool inspector_requested = state.selected && has_options(state.selected);
    if (inspector_requested)
        state.animated_module = state.selected;
    const float inspector_target = inspector_requested ? 1.0f : 0.0f;
    state.inspector_animation = approach(state.inspector_animation, inspector_target, 12.0f);
    if (inspector_target == 0.0f && state.inspector_animation < 0.005f)
        state.animated_module = nullptr;

    const float inspector_eased = state.inspector_animation * state.inspector_animation * (3.0f - 2.0f * state.inspector_animation);
    const bool preview_requested = player_preview_requested();
    const float left_extra = preview_requested ? preview_width + preview_gap : 0.0f;
    const float right_extra = (304.0f + inspector_gap) * inspector_eased;
    const float group_width = left_extra + main_size.x + right_extra;
    if (!state.positioned)
    {
        state.position = ImVec2(viewport->Pos.x + (viewport->Size.x - main_size.x - (304.0f + inspector_gap) * 0.5f) * 0.5f,
            viewport->Pos.y + (viewport->Size.y - main_size.y) * 0.5f);
        state.positioned = true;
    }
    const float minimum_x = viewport->Pos.x + 8.0f + left_extra;
    const float maximum_x = std::max(minimum_x, viewport->Pos.x + viewport->Size.x - main_size.x - right_extra - 8.0f);
    const float minimum_y = viewport->Pos.y + 8.0f;
    const float maximum_y = std::max(minimum_y, viewport->Pos.y + viewport->Size.y - main_size.y - 8.0f);
    state.position.x = std::clamp(state.position.x, minimum_x, maximum_x);
    state.position.y = std::clamp(state.position.y, minimum_y, maximum_y);

    const ImVec2 root_position(state.position.x - left_extra, state.position.y);
    const struct { float spread; int alpha; } shadow_layers[] = {
        { 16.0f, 10 },
        { 8.0f, 18 },
        { 3.0f, 28 }
    };
    for (const auto& layer : shadow_layers)
    {
        ImGui::GetBackgroundDrawList()->AddRectFilled(
            ImVec2(root_position.x - layer.spread, root_position.y - layer.spread),
            ImVec2(root_position.x + group_width + layer.spread, root_position.y + main_size.y + layer.spread),
            IM_COL32(0, 0, 0, layer.alpha), 20.0f + layer.spread);
    }
    ImGui::SetNextWindowPos(root_position, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(group_width, main_size.y), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    if (ImGui::Begin("##ThedoriusGrimUI", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove))
    {
        const ImVec2 root_origin = ImGui::GetWindowPos();
        const ImVec2 origin(root_origin.x + left_extra, root_origin.y);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        if (preview_requested)
            render_player_preview(root_origin);
        draw->AddRectFilled(origin, ImVec2(origin.x + main_size.x, origin.y + main_size.y), shell, 20.0f);
        draw->AddRect(origin, ImVec2(origin.x + main_size.x, origin.y + main_size.y), IM_COL32(255, 255, 255, 12), 20.0f);
        draw->AddRectFilled(origin, ImVec2(origin.x + sidebar_width, origin.y + main_size.y), sidebar, 20.0f, ImDrawFlags_RoundCornersLeft);
        draw->AddRectFilledMultiColor(ImVec2(origin.x + 1.0f, origin.y + 1.0f),
            ImVec2(origin.x + sidebar_width - 1.0f, origin.y + 82.0f),
            alpha(accent, 7.0f / 255.0f), alpha(accent, 2.0f / 255.0f),
            alpha(accent, 0.0f), alpha(accent, 0.0f));

        ImGui::SetCursorScreenPos(ImVec2(origin.x + 8.0f, origin.y + 7.0f));
        ImGui::InvisibleButton("##drag_shell", ImVec2(sidebar_width - 16.0f, 47.0f));
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            state.position = ImVec2(state.position.x + ImGui::GetIO().MouseDelta.x,
                state.position.y + ImGui::GetIO().MouseDelta.y);

        if (const ImTextureID helmet = gui::get_icon_texture(gui::icon_asset::helmet))
            draw->AddImage(helmet, ImVec2(origin.x + 16.0f, origin.y + 14.0f), ImVec2(origin.x + 52.0f, origin.y + 50.0f));
        const float logo_sz = 19.0f;
        const ImVec2 one_sz = gui::ui_bold_font->CalcTextSizeA(logo_sz, FLT_MAX, 0.0f, "one");
        draw_text(draw, gui::ui_bold_font, logo_sz, ImVec2(origin.x + 60.0f, origin.y + 14.0f), IM_COL32(245, 245, 248, 255), "one");
        draw_text(draw, gui::ui_bold_font, logo_sz, ImVec2(origin.x + 60.0f + one_sz.x, origin.y + 14.0f), gui::get_dynamic_color(0), "step");
        draw_text(draw, gui::ui_bold_font, 12.0f, ImVec2(origin.x + 60.0f, origin.y + 38.0f), muted, "v1.0");

        const float nav_x = origin.x + 14.0f;
        draw_text(draw, gui::ui_bold_font, 10.5f, ImVec2(nav_x + 4.0f, origin.y + 73.0f), muted, "FEATURES");
        float nav_y = origin.y + 89.0f;
        for (const navigation_entry& entry : feature_navigation)
        {
            navigation_button(entry, ImVec2(nav_x, nav_y), 176.0f);
            nav_y += 40.0f;
        }
        nav_y += 5.0f;
        draw_text(draw, gui::ui_bold_font, 10.5f, ImVec2(nav_x + 4.0f, nav_y), muted, "OTHER");
        nav_y += 17.0f;
        for (const navigation_entry& entry : other_navigation)
        {
            navigation_button(entry, ImVec2(nav_x, nav_y), 176.0f);
            nav_y += 40.0f;
        }

        const ImVec2 profile(origin.x + 13.0f, origin.y + main_size.y - 58.0f);
        draw->AddRectFilled(profile, ImVec2(origin.x + sidebar_width - 13.0f, origin.y + main_size.y - 12.0f),
            IM_COL32(255, 255, 255, 10), 9.0f);
        draw->AddRect(profile, ImVec2(origin.x + sidebar_width - 13.0f, origin.y + main_size.y - 12.0f),
            IM_COL32(255, 255, 255, 8), 9.0f);
        icon(draw, "\xEF\x80\x87", ImVec2(profile.x + 18.0f, profile.y + 23.0f), secondary, 16.0f);
        std::string player = friends::local_player_name();
        if (player.empty()) player = "Player";
        if (player.size() > 16) player = player.substr(0, 14) + "...";
        draw_text(draw, gui::ui_bold_font, 14.0f, ImVec2(profile.x + 38.0f, profile.y + 7.0f), secondary, player.c_str());
        char uid[32]{};
        std::snprintf(uid, sizeof(uid), "UID: %06u", state.session_uid);
        draw_text(draw, gui::ui_font, 12.0f, ImVec2(profile.x + 38.0f, profile.y + 28.0f), muted, uid);

        const ImVec2 content(origin.x + sidebar_width + 18.0f, origin.y + 13.0f);
        const float content_width = main_size.x - sidebar_width - 36.0f;
        const char* page_title = page_label(state.current);
        const navigation_entry* active_entry = nullptr;
        for (const navigation_entry& entry : feature_navigation) if (entry.target == state.current) active_entry = &entry;
        for (const navigation_entry& entry : other_navigation) if (entry.target == state.current) active_entry = &entry;
        if (active_entry) icon(draw, active_entry->glyph, ImVec2(content.x + 9.0f, content.y + 12.0f), accent_hover, 16.0f);
        draw_text(draw, gui::ui_bold_font, 19.0f, ImVec2(content.x + 28.0f, content.y), primary, page_title);
        draw_text(draw, gui::ui_bold_font, 13.0f, ImVec2(content.x + 28.0f, content.y + 26.0f), muted, page_description(state.current));

        const ImVec2 search_button(content.x + content_width - 34.0f, content.y - 3.0f);
        ImGui::SetCursorScreenPos(search_button);
        ImGui::InvisibleButton("##open_search", ImVec2(34.0f, 34.0f));
        if (ImGui::IsItemClicked())
        {
            state.search_open = !state.search_open;
            state.focus_search = state.search_open;
            state.search_selection = 0;
            state.search_completion_active = false;
            state.search_completion_pending = false;
            state.search_completion_target.clear();
        }
        if (ImGui::IsItemHovered() || state.search_open)
            draw->AddRectFilled(search_button, ImVec2(search_button.x + 34.0f, search_button.y + 34.0f), IM_COL32(255, 255, 255, 8), 8.0f);
        const ImVec2 search_center(search_button.x + 16.0f, search_button.y + 15.0f);
        draw->AddCircle(search_center, 5.5f, secondary, 0, 1.5f);
        draw->AddLine(ImVec2(search_center.x + 4.0f, search_center.y + 4.0f),
            ImVec2(search_center.x + 8.0f, search_center.y + 8.0f), secondary, 1.5f);

        const ImVec2 body(content.x, content.y + 57.0f);
        const ImVec2 body_size(content_width, main_size.y - 77.0f);
        state.page_fade = approach(state.page_fade, 1.0f, 14.0f);
        const float page_alpha = state.page_fade * state.page_fade * (3.0f - 2.0f * state.page_fade);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::clamp(page_alpha, 0.02f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        if (state.current == page::friends && callbacks.friends)
        {
            ImGui::SetCursorScreenPos(body);
            if (ImGui::BeginChild("##friends_page", body_size)) callbacks.friends();
            ImGui::EndChild();
        }
        else if (state.current == page::configs && callbacks.configs)
        {
            ImGui::SetCursorScreenPos(body);
            if (ImGui::BeginChild("##configs_page", body_size)) callbacks.configs();
            ImGui::EndChild();
        }
        else if (state.current == page::hud)
        {
            render_hud_editor_launcher(body, body_size);
        }
        else if (state.current == page::themes)
        {
            ImGui::SetCursorScreenPos(body);
            if (ImGui::BeginChild("##themes_page", body_size)) gui::render_theme_selection();
            ImGui::EndChild();
        }
        else
        {
            render_grid(body, body_size);
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        const ImVec2 inspector_position(origin.x + main_size.x + inspector_gap, origin.y);
        render_search(origin);
        render_inspector(inspector_position);
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}
