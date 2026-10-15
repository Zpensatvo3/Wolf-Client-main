#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "config_page.hpp"
#include "config_page_model.hpp"
#include "../gui/gui.hpp"
#include "../gui/surface_style.hpp"
#include "../modules/core/module_registry.hpp"
#include "../modules/core/public_config.hpp"
#include "../friends/friends.hpp"
#include <cfloat>
#include <cmath>

namespace gui::config_page
{
    namespace
    {
        enum class operation { none, refresh, publish, load, remove };
        struct page_state
        {
            bool requested = false, editor = false, narrow_details = false;
            int selected = 0, editing = 0, deleting = 0, sort = 0;
            model::collection tab = model::collection::community;
            std::unordered_set<int> favourites;
            char search[96]{}, name[49]{}, description[181]{};
            operation pending = operation::none;
            std::uint64_t started_revision = 0;
            std::string delete_name;
        };
        page_state state;
        constexpr ImU32 primary = IM_COL32(234, 238, 246, 255);
        constexpr ImU32 muted = IM_COL32(143, 151, 169, 255);
        constexpr ImU32 faint = IM_COL32(103, 113, 133, 255);
        constexpr ImU32 danger = IM_COL32(247, 117, 132, 255);

        ImFont* font(bool bold = false)
        {
            ImFont* result = bold ? gui::ui_bold_font : gui::ui_font;
            return result ? result : ImGui::GetFont();
        }
        ImU32 tint(ImU32 color, float alpha)
        {
            auto value = ImGui::ColorConvertU32ToFloat4(color);
            value.w *= alpha;
            return ImGui::GetColorU32(value);
        }
        ImU32 accent() { return gui::get_dynamic_color(); }

        std::string elide(std::string_view value, float width, float size, bool bold = false)
        {
            std::string output(value);
            if (font(bold)->CalcTextSizeA(size, FLT_MAX, 0, output.c_str()).x <= width) return output;
            const float dots = font(bold)->CalcTextSizeA(size, FLT_MAX, 0, "...").x;
            if (width <= dots) return "";
            while (!output.empty()) {
                std::size_t end = output.size() - 1;
                while (end && (static_cast<unsigned char>(output[end]) & 0xc0) == 0x80) --end;
                output.resize(end);
                if (font(bold)->CalcTextSizeA(size, FLT_MAX, 0, output.c_str()).x + dots <= width) break;
            }
            return output + "...";
        }
        void label(ImVec2 at, std::string_view text, float size = 13.f, ImU32 color = muted,
            bool bold = false, float width = FLT_MAX)
        {
            const auto display = elide(text, width, size, bold);
            ImGui::GetWindowDrawList()->AddText(font(bold), size, at, ImGui::GetColorU32(ImGui::ColorConvertU32ToFloat4(color)), display.c_str());
        }
        void tooltip(const char* value)
        {
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", value);
        }
        enum class icon { none, plus, refresh, back, close, star };
        void draw_icon(ImVec2 c, icon glyph, ImU32 color)
        {
            auto* draw = ImGui::GetWindowDrawList();
            if (glyph == icon::plus) {
                draw->AddLine(ImVec2(c.x - 5, c.y), ImVec2(c.x + 5, c.y), color, 1.6f);
                draw->AddLine(ImVec2(c.x, c.y - 5), ImVec2(c.x, c.y + 5), color, 1.6f);
            } else if (glyph == icon::close) {
                draw->AddLine(ImVec2(c.x - 4, c.y - 4), ImVec2(c.x + 4, c.y + 4), color, 1.5f);
                draw->AddLine(ImVec2(c.x - 4, c.y + 4), ImVec2(c.x + 4, c.y - 4), color, 1.5f);
            } else if (glyph == icon::back) {
                draw->AddLine(ImVec2(c.x + 3, c.y - 5), ImVec2(c.x - 3, c.y), color, 1.5f);
                draw->AddLine(ImVec2(c.x - 3, c.y), ImVec2(c.x + 3, c.y + 5), color, 1.5f);
            } else if (glyph == icon::refresh) {
                draw->PathArcTo(c, 5.5f, .65f, 5.8f);
                draw->PathStroke(color, 0, 1.5f);
                draw->AddTriangleFilled(ImVec2(c.x + 7, c.y - 5), ImVec2(c.x + 2, c.y - 5), ImVec2(c.x + 6, c.y), color);
            } else if (glyph == icon::star) {
                for (int p = 0; p < 10; ++p) {
                    const float angle = -1.5707963f + p * .6283185f;
                    const float radius = p % 2 ? 3.1f : 7.f;
                    draw->PathLineTo(ImVec2(c.x + std::cos(angle) * radius, c.y + std::sin(angle) * radius));
                }
                draw->PathStroke(color, ImDrawFlags_Closed, 1.4f);
            }
        }
        bool button(const char* id, const char* text, ImVec2 size, bool strong = false,
            bool destructive = false, icon glyph = icon::none)
        {
            const auto p = ImGui::GetCursorScreenPos();
            const bool pressed = ImGui::InvisibleButton(id, size);
            const bool hover = ImGui::IsItemHovered(), held = ImGui::IsItemActive();
            const auto tone = destructive ? danger : accent();
            auto* draw = ImGui::GetWindowDrawList();
            const auto fill = strong ? gui::surface_style::mix(IM_COL32(26, 30, 39, 245), tone, held ? .38f : hover ? .28f : .20f)
                : destructive ? tint(danger, hover ? .14f : .06f) : IM_COL32(255, 255, 255, held ? 20 : hover ? 14 : 6);
            draw->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), ImGui::GetColorU32(ImGui::ColorConvertU32ToFloat4(fill)), 7.f);
            draw->AddRect(p, ImVec2(p.x + size.x, p.y + size.y), tint(strong || destructive ? tone : IM_COL32_WHITE, strong ? .36f : .08f), 7.f);
            const auto color = ImGui::GetColorU32(ImGui::ColorConvertU32ToFloat4(destructive ? danger : strong ? tone : primary));
            const auto display = elide(text, size.x - (glyph == icon::none ? 14.f : 36.f), 12.5f, true);
            const float text_width = font(true)->CalcTextSizeA(12.5f, FLT_MAX, 0, display.c_str()).x;
            const float group_width = text_width + (glyph == icon::none || !text[0] ? 0 : 20.f);
            const float x = p.x + (size.x - group_width) * .5f;
            if (glyph != icon::none) draw_icon(ImVec2(text[0] ? x + 6 : p.x + size.x * .5f, p.y + size.y * .5f), glyph, color);
            if (text[0]) draw->AddText(font(true), 12.5f, ImVec2(x + (glyph == icon::none ? 0 : 20), p.y + (size.y - 12.5f) * .5f - 1), color, display.c_str());
            return pressed;
        }
        void track(operation op, const configs::snapshot& library)
        {
            state.pending = op;
            state.started_revision = library.revision;
        }
        void open_editor(const configs::public_config* entry = nullptr)
        {
            state.editor = true;
            state.editing = entry ? entry->id : 0;
            model::copy_utf8(state.name, entry ? entry->name : "");
            model::copy_utf8(state.description, entry ? entry->description : "");
        }
        bool begin_panel(const char* id, ImVec2 size)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 11.f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 14));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(.047f, .056f, .072f, .74f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, .065f));
            return ImGui::BeginChild(id, size, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        }
        void end_panel()
        {
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(2);
        }
        void document(ImVec2 p, float size, bool selected = false)
        {
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(p, ImVec2(p.x + size, p.y + size), tint(accent(), selected ? .17f : .08f), size * .24f);
            const ImVec2 a(p.x + size * .30f, p.y + size * .24f), b(p.x + size * .70f, p.y + size * .76f);
            draw->AddRect(a, b, tint(accent(), .85f), 2.f, 0, 1.25f);
            for (int i = 0; i < 3; ++i) draw->AddLine(ImVec2(a.x + 3, a.y + 5 + i * size * .11f), ImVec2(b.x - 3, a.y + 5 + i * size * .11f), tint(accent(), .65f));
        }
        void wrapped(const char* text, ImU32 color = muted)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::PushTextWrapPos(0);
            ImGui::TextUnformatted(text);
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
        }
        void section(const char* text)
        {
            const auto p = ImGui::GetCursorScreenPos();
            label(p, text, 11, faint, true);
            ImGui::Dummy(ImVec2(0, 16));
        }
        void render_library(const configs::snapshot& library, const std::vector<const configs::public_config*>& entries)
        {
            const float width = ImGui::GetContentRegionAvail().x;
            const char* tabs[] = { gui::text("Community", "Topluluk"), gui::text("Mine", "Benim"), gui::text("Favourites", "Favoriler") };
            for (int i = 0; i < 3; ++i) {
                if (i) ImGui::SameLine(0, 5);
                if (button(tabs[i], tabs[i], ImVec2((width - 10) / 3, 31), static_cast<int>(state.tab) == i)) state.tab = static_cast<model::collection>(i);
            }
            ImGui::Dummy(ImVec2(0, 3));
            ImGui::SetNextItemWidth(width - 33);
            ImGui::InputTextWithHint("##search", gui::text("Search setups, authors...", "Config veya oyuncu ara..."), state.search, IM_ARRAYSIZE(state.search));
            ImGui::SameLine(0, 5);
            if (button("clear_search", "", ImVec2(28, ImGui::GetFrameHeight()), false, false, icon::close)) state.search[0] = '\0';
            tooltip(gui::text("Clear search", "Aramayi temizle"));
            const auto row = ImGui::GetCursorScreenPos();
            label(ImVec2(row.x, row.y + 6), std::to_string(entries.size()) + gui::text(" setups", " config"), 11.5f, faint);
            ImGui::SetCursorScreenPos(ImVec2(row.x + std::max(0.f, width - 128), row.y));
            ImGui::SetNextItemWidth(128);
            const char* sorts[] = { gui::text("Recent", "En yeni"), gui::text("Popular", "Populer"), gui::text("Name", "Isim") };
            ImGui::Combo("##sort", &state.sort, sorts, 3);
            ImGui::SetCursorScreenPos(ImVec2(row.x, row.y + ImGui::GetFrameHeight() + 8));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
            if (ImGui::BeginChild("##rows", ImVec2(0, 0))) {
                for (const auto* entry : entries) {
                    ImGui::PushID(entry->id);
                    const auto p = ImGui::GetCursorScreenPos();
                    const float row_width = ImGui::GetContentRegionAvail().x;
                    if (ImGui::InvisibleButton("row", ImVec2(row_width, 72))) {
                        state.selected = entry->id;
                        state.narrow_details = true;
                    }
                    const bool selected = state.selected == entry->id;
                    gui::surface_style::module_card(ImGui::GetWindowDrawList(), p, ImVec2(p.x + row_width, p.y + 72), accent(), ImGui::IsItemHovered() ? 1.f : 0.f, selected ? .65f : 0.f);
                    document(ImVec2(p.x + 10, p.y + 15), 36, selected);
                    label(ImVec2(p.x + 57, p.y + 12), entry->name, 14, selected ? accent() : primary, true, row_width - 72);
                    label(ImVec2(p.x + 57, p.y + 33), entry->author_name, 11.5f, muted, false, row_width - 94);
                    label(ImVec2(p.x + 57, p.y + 51), std::to_string(entry->module_count) + gui::text(" modules  /  ", " modul  /  ") + std::to_string(entry->downloads) + gui::text(" loads", " yukleme"), 10.5f, faint, false, row_width - 72);
                    if (state.favourites.contains(entry->id)) draw_icon(ImVec2(p.x + row_width - 16, p.y + 40), icon::star, tint(accent(), .8f));
                    tooltip(entry->name.c_str());
                    ImGui::Dummy(ImVec2(0, 3));
                    ImGui::PopID();
                }
                if (entries.empty()) {
                    ImGui::Dummy(ImVec2(0, 32));
                    document(ImGui::GetCursorScreenPos(), 44);
                    ImGui::Dummy(ImVec2(0, 58));
                    wrapped(library.busy ? gui::text("Fetching configs...", "Configler getiriliyor...") : gui::text("No matching configs", "Eslesen config yok"), primary);
                    wrapped(state.search[0] ? gui::text("Try another name or clear your search.", "Baska bir isim dene veya aramayi temizle.")
                        : state.tab == model::collection::favourites ? gui::text("Star a config to find it here. Favourites last for this session.", "Bir configi favoriye ekle. Favoriler bu oturum boyunca tutulur.")
                        : state.tab == model::collection::mine ? gui::text("Publish your first setup with New config.", "Yeni config ile ilk ayar paketini paylas.")
                        : gui::text("Refresh to check the community library.", "Topluluk kutuphanesini yenilemeyi dene."));
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        void render_details(const configs::snapshot& library, const configs::public_config* selected)
        {
            const auto top = ImGui::GetCursorScreenPos();
            const float width = ImGui::GetContentRegionAvail().x, height = ImGui::GetContentRegionAvail().y;
            if (!selected) {
                ImGui::Dummy(ImVec2(0, 34));
                document(ImGui::GetCursorScreenPos(), 52);
                ImGui::Dummy(ImVec2(0, 68));
                wrapped(gui::text("Your next setup starts here", "Yeni ayarlarin burada basliyor"), primary);
                ImGui::Dummy(ImVec2(0, 4));
                wrapped(gui::text("Choose a config to see its details, or share your own settings with the community.", "Detaylari gormek icin bir config sec veya kendi ayarlarini toplulukla paylas."));
                return;
            }
            const float footer = selected->mine ? 114.f : 74.f;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
            if (ImGui::BeginChild("##detail_scroll", ImVec2(0, std::max(40.f, height - footer)))) {
                section(selected->mine ? gui::text("YOUR CONFIG", "SENIN CONFIGIN") : gui::text("COMMUNITY CONFIG", "TOPLULUK CONFIGI"));
                document(ImGui::GetCursorScreenPos(), 48, true);
                ImGui::Dummy(ImVec2(0, 62));
                ImGui::PushFont(font(true));
                wrapped(selected->name.c_str(), primary);
                ImGui::PopFont();
                wrapped((gui::text("by ", "Hazirlayan: ") + selected->author_name).c_str());
                ImGui::Dummy(ImVec2(0, 12));
                const auto p = ImGui::GetCursorScreenPos();
                const float tile_width = (width - 8) / 2;
                for (int i = 0; i < 2; ++i) {
                    const auto tile = ImVec2(p.x + i * (tile_width + 8), p.y);
                    ImGui::GetWindowDrawList()->AddRectFilled(tile, ImVec2(tile.x + tile_width, tile.y + 59), IM_COL32(255, 255, 255, 5), 7);
                    label(ImVec2(tile.x + 10, tile.y + 8), std::to_string(i ? selected->downloads : selected->module_count), 19, primary, true, tile_width - 20);
                    label(ImVec2(tile.x + 10, tile.y + 35), i ? gui::text("Downloads", "Yukleme") : gui::text("Active modules", "Aktif modul"), 10.5f, muted, false, tile_width - 20);
                }
                ImGui::Dummy(ImVec2(0, 74));
                section(gui::text("ABOUT THIS SETUP", "BU CONFIG HAKKINDA"));
                wrapped(selected->description.empty() ? gui::text("No description provided.", "Aciklama eklenmemis.") : selected->description.c_str());
                ImGui::Dummy(ImVec2(0, 15));
                wrapped((gui::text("Updated  ", "Guncelleme  ") + model::date(selected->updated_at.empty() ? selected->created_at : selected->updated_at)).c_str(), faint);
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::SetCursorScreenPos(ImVec2(top.x, top.y + height - footer + 9));
            ImGui::GetWindowDrawList()->AddLine(ImVec2(top.x, top.y + height - footer), ImVec2(top.x + width, top.y + height - footer), IM_COL32(255, 255, 255, 12));
            ImGui::BeginDisabled(library.busy || state.pending != operation::none);
            if (button("load", gui::text("Load config", "Configi yukle"), ImVec2(width - 46, 35), true)) {
                track(operation::load, library);
                configs::load(selected->id);
            }
            ImGui::EndDisabled();
            ImGui::SameLine(0, 8);
            const bool favourite = state.favourites.contains(selected->id);
            if (button("favourite", "", ImVec2(38, 35), favourite, false, icon::star)) {
                if (favourite) state.favourites.erase(selected->id); else state.favourites.insert(selected->id);
            }
            tooltip(favourite ? gui::text("Remove favourite", "Favoriden cikar") : gui::text("Add to session favourites", "Oturum favorilerine ekle"));
            if (selected->mine) {
                ImGui::BeginDisabled(library.busy || state.pending != operation::none);
                if (button("edit", gui::text("Edit", "Duzenle"), ImVec2((width - 8) / 2, 30))) open_editor(selected);
                ImGui::SameLine(0, 8);
                if (button("delete", gui::text("Delete", "Sil"), ImVec2((width - 8) / 2, 30), false, true)) {
                    state.deleting = selected->id;
                    state.delete_name = selected->name;
                }
                ImGui::EndDisabled();
            }
            label(ImGui::GetCursorScreenPos(), gui::text("Loading replaces your current module settings.", "Yukleme, mevcut modul ayarlarini degistirir."), 10.5f, faint, false, width);
        }

        void render_editor(const configs::snapshot& library)
        {
            const auto top = ImGui::GetCursorScreenPos();
            const float width = ImGui::GetContentRegionAvail().x, height = ImGui::GetContentRegionAvail().y;
            const bool busy = library.busy || state.pending != operation::none;
            const std::string author = friends::local_player_name();
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
            if (ImGui::BeginChild("##form_scroll", ImVec2(0, std::max(40.f, height - 87)))) {
                section(state.editing ? gui::text("EDIT CONFIG", "CONFIG DUZENLE") : gui::text("NEW CONFIG", "YENI CONFIG"));
                ImGui::PushFont(font(true));
                wrapped(state.editing ? gui::text("Refine your setup", "Configini guncelle") : gui::text("Share your setup", "Ayarlarini paylas"), primary);
                ImGui::PopFont();
                wrapped(gui::text("Your current modules and their settings will be shared publicly.", "Mevcut modullerin ve ayarlari herkese acik paylasilir."));
                ImGui::Dummy(ImVec2(0, 2));
                ImGui::BeginDisabled(busy);
                section(gui::text("CONFIG NAME", "CONFIG ADI"));
                ImGui::SetNextItemWidth(-1);
                ImGui::InputTextWithHint("##name", gui::text("e.g. Clean survival", "Orn. Sade survival"), state.name, IM_ARRAYSIZE(state.name));
                label(ImGui::GetCursorScreenPos(), std::to_string(std::strlen(state.name)) + " / 48", 10, faint);
                ImGui::Dummy(ImVec2(0, 12));
                section(gui::text("DESCRIPTION / OPTIONAL", "ACIKLAMA / ISTEGE BAGLI"));
                ImGui::InputTextMultiline("##description", state.description, IM_ARRAYSIZE(state.description), ImVec2(-1, 72));
                label(ImGui::GetCursorScreenPos(), std::to_string(std::strlen(state.description)) + " / 180", 10, faint);
                ImGui::Dummy(ImVec2(0, 15));
                ImGui::EndDisabled();
                section(gui::text("PUBLISHING AS", "YAYINLAYAN"));
                wrapped(author.empty() ? gui::text("Join a world to publish a config.", "Config yayinlamak icin bir dunyaya gir.") : author.c_str(), author.empty() ? danger : primary);
                wrapped((std::to_string(modules::enabled_module_count()) + gui::text(" active modules included", " aktif modul dahil edilecek")).c_str(), faint);
                if (state.editing) {
                    ImGui::Dummy(ImVec2(0, 8));
                    wrapped(gui::text("Saving also replaces this config's shared settings with your current ones.", "Kaydetme, bu configin paylasilan ayarlarini mevcut ayarlarinla degistirir."), faint);
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::SetCursorScreenPos(ImVec2(top.x, top.y + height - 76));
            ImGui::BeginDisabled(busy || author.empty() || model::trim(state.name).empty());
            if (button("publish", state.editing ? gui::text("Save changes", "Degisiklikleri kaydet") : gui::text("Publish config", "Configi yayinla"), ImVec2(width, 36), true)) {
                track(operation::publish, library);
                configs::publish(state.editing, model::trim(state.name), model::trim(state.description), author,
                    modules::export_public_config(), modules::enabled_module_count());
            }
            ImGui::EndDisabled();
            ImGui::BeginDisabled(busy);
            if (button("cancel_edit", gui::text("Cancel", "Vazgec"), ImVec2(width, 29))) state.editor = false;
            ImGui::EndDisabled();
        }
        void render_delete(const configs::snapshot& library)
        {
            if (state.deleting && !ImGui::IsPopupOpen("##config_delete")) ImGui::OpenPopup("##config_delete");
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(.5f, .5f));
            ImGui::SetNextWindowSize(ImVec2(362, 0), ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22, 20));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12);
            ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(.06f, .07f, .09f, .99f));
            if (ImGui::BeginPopupModal("##config_delete", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize)) {
                section(gui::text("DELETE CONFIG", "CONFIG SIL"));
                ImGui::PushFont(font(true));
                wrapped(state.delete_name.c_str(), primary);
                ImGui::PopFont();
                ImGui::Dummy(ImVec2(0, 7));
                wrapped(gui::text("This removes the public config permanently. Your current module settings stay unchanged.", "Bu config kalici olarak silinir. Mevcut modul ayarlarin degismez."));
                ImGui::Dummy(ImVec2(0, 14));
                const float half = (ImGui::GetContentRegionAvail().x - 8) / 2;
                if (button("cancel_delete", gui::text("Keep config", "Vazgec"), ImVec2(half, 35))) {
                    state.deleting = 0;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine(0, 8);
                const auto owned = std::find_if(library.entries.begin(), library.entries.end(), [](const auto& entry) { return entry.id == state.deleting && entry.mine; });
                ImGui::BeginDisabled(library.busy || state.pending != operation::none || owned == library.entries.end());
                if (button("confirm_delete", gui::text("Delete config", "Configi sil"), ImVec2(half, 35), false, true)) {
                    track(operation::remove, library);
                    configs::remove(state.deleting);
                    state.deleting = 0;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndDisabled();
                ImGui::EndPopup();
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
        }
    }

    void render(bool embedded)
    {
        ImGui::PushID("config_workspace");
        std::string payload, downloaded_name;
        if (configs::consume_pending_payload(payload, downloaded_name)) {
            std::string result;
            const bool applied = modules::apply_public_config(payload, result);
            gui::push_workspace_toast(downloaded_name + ": " + result, applied);
        }
        auto library = configs::get_snapshot();
        if (!state.requested && !library.busy) {
            track(operation::refresh, library);
            configs::refresh();
            state.requested = true;
            library = configs::get_snapshot();
        }
        if (state.pending != operation::none && library.revision > state.started_revision && !library.busy) {
            if (state.pending == operation::publish && library.success) {
                state.editor = false;
                state.tab = model::collection::mine;
                state.search[0] = '\0';
                if (state.editing) state.selected = state.editing;
            }
            if (state.pending != operation::load || !library.success) gui::push_workspace_toast(library.status, library.success);
            state.pending = operation::none;
        }
        if (!embedded) gui::page_header(gui::text("Configs", "Configler"), gui::text("Your community library", "Topluluk kutuphanen"));
        ImGui::PushFont(font());
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(9, 7));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1, 1, 1, .035f));
        const auto theme_fill = [](float amount) {
            return gui::surface_style::mix(IM_COL32(24, 28, 36, 245), accent(), amount);
        };
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, theme_fill(.09f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, theme_fill(.14f));
        ImGui::PushStyleColor(ImGuiCol_Button, theme_fill(.12f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme_fill(.22f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme_fill(.30f));
        ImGui::PushStyleColor(ImGuiCol_Header, theme_fill(.12f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, theme_fill(.22f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, theme_fill(.30f));
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, tint(accent(), .30f));
        const auto top = ImGui::GetCursorScreenPos();
        const auto available = ImGui::GetContentRegionAvail();
        const bool narrow = available.x < 620;
        const bool detail_only = narrow && (state.editor || state.narrow_details);
        if (detail_only) {
            ImGui::BeginDisabled(state.pending == operation::publish);
            if (button("back_to_library", "", ImVec2(30, 32), false, false, icon::back)) {
                state.narrow_details = false;
                if (state.pending != operation::publish) state.editor = false;
            }
            tooltip(gui::text("Back to library", "Kutuphaneye don"));
            ImGui::EndDisabled();
        } else {
            label(ImVec2(top.x, top.y + 1), gui::text("Config library", "Config kutuphanesi"), 15, primary, true, available.x - 180);
            label(ImVec2(top.x, top.y + 21), gui::text("Discover. Save. Make it yours.", "Kesfet. Kaydet. Kendine uyarla."), 10.5f, faint, false, available.x - 180);
        }
        ImGui::SetCursorScreenPos(ImVec2(top.x + available.x - 164, top.y));
        ImGui::BeginDisabled(library.busy || state.pending != operation::none || state.editor);
        if (button("new_config", gui::text("New config", "Yeni config"), ImVec2(124, 32), true, false, icon::plus)) open_editor();
        ImGui::EndDisabled();
        ImGui::SameLine(0, 8);
        ImGui::BeginDisabled(library.busy || state.pending != operation::none);
        if (button("refresh_library", "", ImVec2(32, 32), false, false, icon::refresh)) {
            track(operation::refresh, library);
            configs::refresh();
        }
        tooltip(gui::text("Refresh library", "Kutuphaneyi yenile"));
        ImGui::EndDisabled();
        const auto entries = model::filter(library, state.tab, state.search, state.favourites, static_cast<model::order>(state.sort));
        const configs::public_config* selected = nullptr;
        for (const auto* entry : entries) if (entry->id == state.selected) selected = entry;
        if (!selected && !entries.empty()) {
            selected = entries.front();
            state.selected = selected->id;
        }
        const float body_height = std::max(110.f, available.y - 77);
        const float left_width = narrow ? available.x : std::clamp(available.x * .46f, 260.f, 360.f);
        ImGui::SetCursorScreenPos(ImVec2(top.x, top.y + 46));
        if (!detail_only) {
            if (begin_panel("##library", ImVec2(left_width, body_height))) render_library(library, entries);
            end_panel();
        }
        if (!narrow || detail_only) {
            // Selection can change in the list above; resolve against the stable frame snapshot.
            selected = nullptr;
            for (const auto* entry : entries) if (entry->id == state.selected) selected = entry;
            ImGui::SetCursorScreenPos(ImVec2(top.x + (narrow ? 0 : left_width + 12), top.y + 46));
            if (begin_panel("##inspector", ImVec2(narrow ? available.x : available.x - left_width - 12, body_height))) {
                if (state.editor) render_editor(library); else render_details(library, selected);
            }
            end_panel();
        }
        const auto footer = ImVec2(top.x, top.y + 46 + body_height + 10);
        ImGui::SetCursorScreenPos(footer);
        const bool working = library.busy || state.pending != operation::none;
        const auto status_color = working ? accent() : library.success ? muted : danger;
        ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(footer.x + 4, footer.y + 7), 2.5f, status_color);
        label(ImVec2(footer.x + 14, footer.y), working ? gui::text("Working on your request...", "Islemin tamamlanmasi bekleniyor...") : library.status.empty() ? gui::text("Ready to explore", "Kesfetmeye hazir") : library.status.c_str(), 11, status_color, false, available.x - 20);
        ImGui::Dummy(ImVec2(available.x, 17));
        tooltip(library.status.c_str());
        render_delete(library);
        ImGui::PopStyleColor(10);
        ImGui::PopStyleVar(3);
        ImGui::PopFont();
        ImGui::PopID();
    }
}
