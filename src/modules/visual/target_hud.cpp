#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "target_hud.hpp"
#include "../../services/game/name_protect.hpp"
#include "../../gui/gui.hpp"
#include "../../gui/ui_state.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../services/game/overlay_context.hpp"
#include "../../services/game/anti_bot_service.hpp"
#include "../../ui/hud_editor_layout.hpp"
#include "../../ui/settings_renderer.hpp"
#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <bit>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace
{
    using clock_type = std::chrono::steady_clock;
    constexpr float card_width = 232.f, card_height = 94.f;

    struct icon_quad
    {
        std::array<ImVec2, 4> position{}, uv{};
        std::uintptr_t texture = 0;
        ImU32 tint = IM_COL32_WHITE;
        float depth = 0.f;
    };

    struct equipment
    {
        bool known = false, present = false, enchanted = false;
        int damage = 0, maximum = 0;
        std::string name, key;
        std::vector<icon_quad> icon;
    };
    struct target_info
    {
        bool valid = false;
        int id = -1;
        std::string name, uuid;
        float health = 0.f, maximum_health = 20.f, distance = 0.f;
        std::uintptr_t skin = 0; // Borrowed from Minecraft's texture manager.
        std::array<equipment, 4> armor{};
        equipment hand;
    };

    std::string read_text(const maps::Text& text)
    {
        if (platform::jni_context::clear_exception() || !text) return {};
        maps::String value = text.getString.call();
        if (platform::jni_context::clear_exception() || !value) return {};
        std::string result = value.to_string();
        if (platform::jni_context::clear_exception()) return {};
        for (char& character : result)
            if (character == '\r' || character == '\n' || character == '\t') character = ' ';
        return result;
    }

    glm::vec3 model_vector(const maps::ModelVector3f& vector)
    {
        if (!vector) return {};
        return {vector.x.get(), vector.y.get(), vector.z.get()};
    }

    std::vector<icon_quad> read_item_icon(const maps::ItemStack& stack,
        const services::game::client_context& context, const maps::PlayerEntity& player, const std::string& key)
    {
        using platform::jni_context::clear_exception;
        std::vector<icon_quad> result;
        jni::frame model_frame{96};
        maps::ItemRenderer renderer = context.instance.getItemRenderer.call();
        if (clear_exception() || !renderer) return result;
        maps::World world(context.world.object_instance, false);
        maps::LivingEntity entity(player.object_instance, false);
        maps::ItemBakedModel model = renderer.getModel.call(stack, world, entity, 0);
        if (clear_exception() || !model) return result;
        // The held trident/spyglass models differ from their inventory models.
        if (key.ends_with(".trident") || key.ends_with(".spyglass"))
        {
            maps::ItemModels models = renderer.getModels.call();
            if (clear_exception() || !models) return result;
            model = models.getModel.call(stack);
            if (clear_exception() || !model) return result;
        }
        maps::ItemModelTransformation transformations = model.getTransformation.call();
        if (clear_exception() || !transformations) return result;
        maps::ItemTransformation gui = transformations.gui.get();
        if (clear_exception() || !gui) return result;
        const glm::vec3 rotation = model_vector(gui.rotation.get());
        if (clear_exception()) return result;
        const glm::vec3 translation = model_vector(gui.translation.get());
        if (clear_exception()) return result;
        const glm::vec3 scaling = model_vector(gui.scale.get());
        if (clear_exception()) return result;
        for (int axis = 0; axis < 3; ++axis)
            if (!std::isfinite(rotation[axis]) || !std::isfinite(translation[axis]) || !std::isfinite(scaling[axis])) return result;
        glm::mat4 transform = glm::translate(glm::mat4(1.f), translation);
        transform = glm::rotate(transform, glm::radians(rotation.x), glm::vec3(1, 0, 0));
        transform = glm::rotate(transform, glm::radians(rotation.y), glm::vec3(0, 1, 0));
        transform = glm::rotate(transform, glm::radians(rotation.z), glm::vec3(0, 0, 1));
        transform = glm::scale(transform, scaling);
        const bool side_lit = model.isSideLit.call() == JNI_TRUE;
        if (clear_exception()) return result;
        maps::ItemColors colors = renderer.colors.get();
        if (clear_exception()) return result;
        maps::TextureManager manager = context.instance.getTextureManager.call();
        if (clear_exception() || !manager) return result;
        maps::ModelRandom random = maps::ModelRandom{}.createSeeded.call(static_cast<jlong>(42));
        if (clear_exception() || !random) return result;
        const std::array<maps::Direction, 7> faces = {maps::Direction(nullptr), maps::Direction{}.DOWN.get(),
            maps::Direction{}.UP.get(), maps::Direction{}.NORTH.get(), maps::Direction{}.SOUTH.get(),
            maps::Direction{}.WEST.get(), maps::Direction{}.EAST.get()};
        if (clear_exception()) return result;
        for (const auto& face : faces)
        {
            jni::frame face_frame{32};
            random.setSeed.call(static_cast<jlong>(42));
            if (clear_exception()) return {};
            maps::List quads = model.getQuads.call(maps::BlockState(nullptr), face, random);
            if (clear_exception() || !quads) return {};
            auto objects = quads.toArray.call();
            if (clear_exception() || !objects) return {};
            const int count = objects.get_length();
            if (clear_exception() || count < 0 || count > 512) return {};
            for (int index = 0; index < count; ++index)
            {
                jni::frame quad_frame{32};
                maps::ItemBakedQuad baked(jni::get_env()->GetObjectArrayElement(
                    static_cast<jobjectArray>(objects.object_instance), index), false);
                if (clear_exception() || !baked) return {};
                auto packed = baked.getVertexData.call();
                if (clear_exception() || !packed) return {};
                const int length = packed.get_length();
                if (clear_exception() || length < 32 || length > 256 || length % 4) return {};
                const auto data = packed.to_vector();
                if (clear_exception()) return {};
                const int stride = length / 4;
                icon_quad quad;
                std::array<glm::vec3, 4> vertex{};
                bool valid = true;
                for (int point = 0; point < 4; ++point)
                {
                    const auto number = [&](int offset) { return std::bit_cast<float>(data[point * stride + offset]); };
                    vertex[point] = glm::vec3(transform * glm::vec4(number(0) - .5f, number(1) - .5f, number(2) - .5f, 1.f));
                    quad.position[point] = {.5f + vertex[point].x, .5f - vertex[point].y};
                    quad.uv[point] = {number(4), number(5)};
                    quad.depth += vertex[point].z * .25f;
                    valid &= std::isfinite(quad.position[point].x) && std::isfinite(quad.position[point].y)
                        && std::isfinite(vertex[point].z) && std::isfinite(quad.uv[point].x) && std::isfinite(quad.uv[point].y)
                        && std::abs(quad.position[point].x) < 8.f && std::abs(quad.position[point].y) < 8.f;
                }
                if (!valid) continue;
                const glm::vec3 normal = glm::cross(vertex[1] - vertex[0], vertex[2] - vertex[0]);
                // GUI projection looks along -Z; back faces/zero-area item edges are invisible.
                if (normal.z <= .000001f) continue;
                maps::ItemSprite sprite = baked.getSprite.call();
                if (clear_exception() || !sprite) return {};
                maps::Identifier atlas = sprite.getAtlasId.call();
                if (clear_exception() || !atlas) return {};
                maps::Texture texture = manager.getTexture.call(atlas);
                if (clear_exception() || !texture) return {};
                const int texture_id = texture.getGlId.call();
                if (clear_exception() || texture_id <= 0) return {};
                quad.texture = static_cast<std::uintptr_t>(texture_id);
                const int tint_index = baked.getColorIndex.call();
                if (clear_exception()) return {};
                int tint = -1;
                if (tint_index >= 0 && colors)
                {
                    tint = colors.getColor.call(stack, tint_index);
                    if (clear_exception()) return {};
                }
                const float light = side_lit ? .65f + .35f * std::clamp(glm::dot(glm::normalize(normal),
                    glm::normalize(glm::vec3(-.2f, 1.f, .7f))), 0.f, 1.f) : 1.f;
                quad.tint = IM_COL32(static_cast<int>(((tint >> 16) & 255) * light),
                    static_cast<int>(((tint >> 8) & 255) * light), static_cast<int>((tint & 255) * light), 255);
                result.push_back(quad);
                if (result.size() > 512) return {};
            }
        }
        std::stable_sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.depth < b.depth; });
        return result;
    }

    equipment read_equipment(const maps::ItemStack& stack, const services::game::client_context& context,
        const maps::PlayerEntity& player)
    {
        equipment result;
        if (platform::jni_context::clear_exception() || !stack) return result;
        const bool empty = stack.isEmpty.call() == JNI_TRUE;
        if (platform::jni_context::clear_exception()) return result;
        if (empty) { result.known = true; return result; }
        result.name = read_text(stack.getName.call());
        maps::Item item = stack.item.get();
        if (platform::jni_context::clear_exception() || !item) return {};
        maps::String key = item.getTranslationKey.call();
        if (platform::jni_context::clear_exception() || !key) return {};
        result.key = key.to_string();
        if (platform::jni_context::clear_exception()) return {};
        result.known = result.present = true;
        if (stack.getMaxDamage.is_valid() && stack.getDamage.is_valid())
        {
            const int maximum = stack.getMaxDamage.call();
            if (platform::jni_context::clear_exception()) return result;
            const int damage = stack.getDamage.call();
            if (!platform::jni_context::clear_exception() && maximum > 0)
            {
                result.maximum = maximum;
                result.damage = std::clamp(damage, 0, maximum);
            }
        }
        if (stack.hasEnchantments.is_valid())
        {
            const bool enchanted = stack.hasEnchantments.call() == JNI_TRUE;
            result.enchanted = !platform::jni_context::clear_exception() && enchanted;
        }
        result.icon = read_item_icon(stack, context, player, result.key);
        return result;
    }

    std::uintptr_t skin_texture(const maps::MinecraftClient& client, const maps::PlayerEntity& player)
    {
        const bool client_player = player.is_instance_of<maps::AbstractClientPlayerEntity>();
        if (platform::jni_context::clear_exception() || !client_player) return 0;
        maps::AbstractClientPlayerEntity visible(player.object_instance, false);
        maps::SkinTextures skin = visible.getSkinTextures.call();
        if (platform::jni_context::clear_exception() || !skin) return 0;
        maps::Identifier id = skin.texture.call();
        if (platform::jni_context::clear_exception() || !id) return 0;
        maps::TextureManager manager = client.getTextureManager.call();
        if (platform::jni_context::clear_exception() || !manager) return 0;
        maps::Texture texture = manager.getTexture.call(id);
        if (platform::jni_context::clear_exception() || !texture) return 0;
        const int value = texture.getGlId.call();
        return platform::jni_context::clear_exception() || value <= 0 ? 0 : static_cast<std::uintptr_t>(value);
    }

    std::string player_uuid(const maps::PlayerEntity& player)
    {
        maps::GameProfile profile = player.getGameProfile.call();
        if (platform::jni_context::clear_exception() || !profile) return {};
        maps::UUID uuid = profile.getId.call();
        if (platform::jni_context::clear_exception() || !uuid) return {};
        maps::String value = uuid.toString.call();
        if (platform::jni_context::clear_exception() || !value) return {};
        std::string result = value.to_string();
        return platform::jni_context::clear_exception() ? std::string{} : result;
    }

    ImU32 color(ImVec4 value, float opacity = 1.f)
    {
        value.w *= opacity;
        return ImGui::ColorConvertFloat4ToU32(value);
    }
}

struct modules::target_hud::runtime
{
    services::game::overlay_session session;
    target_info target;
    clock_type::time_point last_seen{};
    clock_type::time_point equipment_update{};
    unsigned generation = 0;
    float health_ratio = 0.f;
    bool preview = false;
};

modules::target_hud::target_hud()
    : module("target hud", "Visual", "target_hud"), runtime_(std::make_unique<runtime>()) {}
modules::target_hud::~target_hud() = default;

void modules::target_hud::on_enable(::cache&) { reset_generation_.fetch_add(1, std::memory_order_release); }
void modules::target_hud::on_disable(::cache&) { reset_generation_.fetch_add(1, std::memory_order_release); }

void modules::target_hud::refresh(bool preview)
{
    auto& state = *runtime_;
    const unsigned generation = reset_generation_.load(std::memory_order_acquire);
    if (state.generation != generation || state.preview != preview)
    {
        state.target = {};
        state.health_ratio = 0.f;
        state.last_seen = {};
        state.generation = generation;
        state.preview = preview;
    }
    if (!platform::jni_context::bind_current_thread()) { state.target = {}; return; }
    if (!enabled && !preview) { state.target = {}; state.session.reset(); return; }
    struct cleanup { ~cleanup() { platform::jni_context::clear_exception(); } } clear;
    jni::frame frame{192};
    try
    {
        services::game::client_context context;
        if (!services::game::capture_client_context(context, true, false))
        {
            state.target = {};
            state.session.reset();
            return;
        }
        if (state.session.observe({context.world, context.player}))
        {
            state.target = {};
            state.last_seen = {};
        }
        if (!preview && (gui::draw || gui::loader_visible || gui::should_unload || context.screen))
        { state.target = {}; return; }
        const HWND window = gui::get_window();
        if (!preview && (!window || GetForegroundWindow() != window)) { state.target = {}; return; }
        const bool local_alive = context.player.isAlive.call() == JNI_TRUE;
        if (platform::jni_context::clear_exception() || (!local_alive && !preview)) { state.target = {}; return; }

        const auto now = clock_type::now();
        maps::Entity candidate;
        bool aimed = false;
        if (preview)
            candidate = maps::Entity(context.player.object_instance, false);
        else
        {
            maps::HitResult hit = context.instance.crosshairTarget.get();
            if (platform::jni_context::clear_exception()) { state.target = {}; return; }
            if (hit && hit.is_instance_of<maps::EntityHitResult>())
            {
                maps::EntityHitResult entity_hit(hit.object_instance, false);
                candidate = entity_hit.entity.get();
                if (platform::jni_context::clear_exception()) { state.target = {}; return; }
                aimed = candidate && candidate.is_instance_of<maps::PlayerEntity>() && !candidate.is_same_object(context.player);
                if (platform::jni_context::clear_exception()) { state.target = {}; return; }
            }
            if (!aimed)
            {
                const float keep = settings_.read().keep_seconds;
                if (!state.target.valid || std::chrono::duration<float>(now - state.last_seen).count() > keep)
                { state.target = {}; return; }
                candidate = context.world.getEntityById.call(state.target.id);
            }
        }
        if (platform::jni_context::clear_exception() || !candidate || !candidate.is_instance_of<maps::PlayerEntity>())
        { state.target = {}; return; }
        maps::PlayerEntity player(candidate.object_instance, false);
        const bool alive = player.isAlive.call() == JNI_TRUE;
        if (platform::jni_context::clear_exception()) { state.target = {}; return; }
        const bool spectator = player.isSpectator.call() == JNI_TRUE;
        if (platform::jni_context::clear_exception() || (!preview && (!alive || spectator || services::anti_bot::is_bot(candidate))))
        { state.target = {}; return; }

        target_info next;
        next.id = player.getId.call();
        if (platform::jni_context::clear_exception()) { state.target = {}; return; }
        next.uuid = player_uuid(player);
        maps::String name = player.getEntityName.call();
        if (platform::jni_context::clear_exception() || !name || next.uuid.empty()) { state.target = {}; return; }
        next.name = name.to_string();
        if (platform::jni_context::clear_exception()) { state.target = {}; return; }
        next.health = player.getHealth.call();
        if (platform::jni_context::clear_exception()) { state.target = {}; return; }
        next.maximum_health = player.getMaxHealth.call();
        if (platform::jni_context::clear_exception()) { state.target = {}; return; }
        next.distance = preview ? 0.f : context.player.distanceTo.call(candidate);
        if (platform::jni_context::clear_exception() || next.name.empty() || !std::isfinite(next.health) ||
            !std::isfinite(next.maximum_health) || next.maximum_health <= 0.f ||
            !std::isfinite(next.distance) || next.distance < 0.f || (!preview && next.distance > 64.f))
        { state.target = {}; return; }
        if (!preview && !aimed && next.uuid != state.target.uuid) { state.target = {}; return; }
        next.health = std::clamp(next.health, 0.f, next.maximum_health);
        next.skin = skin_texture(context.instance, player);
        const bool same_target = state.target.valid && state.target.uuid == next.uuid;
        if (same_target && now < state.equipment_update)
        {
            next.armor = state.target.armor;
            next.hand = state.target.hand;
        }
        else
        {
            maps::PlayerInventory inventory = player.getInventory.call();
            if (platform::jni_context::clear_exception()) { state.target = {}; return; }
            if (inventory)
                for (int index = 0; index < 4; ++index)
                {
                    jni::frame slot_frame{32};
                    next.armor[index] = read_equipment(inventory.getArmorStack.call(3 - index), context, player);
                }
            next.hand = read_equipment(player.getMainHandStack.call(), context, player);
            state.equipment_update = now + std::chrono::milliseconds(50);
        }
        if (platform::jni_context::clear_exception()) { state.target = {}; return; }
        if (!same_target)
            state.health_ratio = next.health / next.maximum_health;
        if (generation != reset_generation_.load(std::memory_order_acquire)) { state.target = {}; return; }
        next.valid = true;
        if (aimed) state.last_seen = now;
        state.target = std::move(next);
    }
    catch (...) { state.target = {}; }
}

modules::target_hud::bounds modules::target_hud::get_bounds(float display_width, float display_height) const
{
    const auto settings = settings_.read();
    const float width = card_width * settings.scale, height = card_height * settings.scale;
    const auto position = gui::hud_editor_layout::clamp({settings.position_x, settings.position_y},
        {width, height}, {display_width, display_height});
    return {position.x, position.y, width, height};
}

void modules::target_hud::set_placement(float x, float y, float scale)
{
    settings_.update([&](auto& value) {
        value.position_x = x; value.position_y = y; value.scale = scale;
        return modules::settings::normalize(value, module_settings::target_hud::schema());
    });
}

void modules::target_hud::draw_card(ImDrawList* draw, float x, float y, bool preview)
{
    if (!draw) return;
    const auto settings = settings_.read();
    const auto& target = runtime_->target;
    if (!target.valid && !preview) return;
    const float scale = settings.scale;
    const auto point = [&](float px, float py) { return ImVec2(x + px * scale, y + py * scale); };
    const ImVec4 accent = gui::get_dynamic_color().Value;
    ImFont* font = gui::ui_font ? gui::ui_font : ImGui::GetFont();
    ImFont* bold = gui::ui_bold_font ? gui::ui_bold_font : font;
    const ImU32 primary = IM_COL32(240, 242, 249, 255), muted = IM_COL32(155, 161, 177, 255);
    auto text = [&](float px, float py, float size, float width, ImU32 tint, const char* value, bool strong = false) {
        ImFont* face = strong ? bold : font;
        const float pixels = size * scale;
        std::string fitted = value ? value : "";
        if (face->CalcTextSizeA(pixels, FLT_MAX, 0.f, fitted.c_str()).x > width * scale)
        {
            const float dots = face->CalcTextSizeA(pixels, FLT_MAX, 0.f, "...").x;
            const char* end = fitted.c_str();
            face->CalcTextSizeA(pixels, std::max(1.f, width * scale - dots), 0.f, fitted.c_str(), nullptr, &end);
            fitted.resize(static_cast<std::size_t>(end - fitted.c_str()));
            fitted += "...";
        }
        const ImVec2 position = point(px, py);
        const ImVec4 clip(position.x, position.y, position.x + width * scale, position.y + pixels + 3.f * scale);
        draw->AddText(face, pixels, position, tint, fitted.c_str(), nullptr, 0.f, &clip);
    };
    if (settings.glow)
        for (int layer = 7; layer > 0; --layer)
        {
            const float spread = static_cast<float>(layer);
            draw->AddRectFilled(point(-spread, -spread), point(card_width + spread, card_height + spread),
                color(accent, .018f), (6.f + spread) * scale);
        }
    draw->AddRectFilled(point(0.f, 0.f), point(card_width, card_height),
        color(ImVec4(.035f, .038f, .052f, settings.background_opacity)), 6.f * scale);
    draw->AddRect(point(0.f, 0.f), point(card_width, card_height), color(accent, .38f), 6.f * scale);
    draw->AddRectFilled(point(10.f, 10.f), point(56.f, 56.f), IM_COL32(31, 33, 43, 255), 3.f * scale);
    if (target.valid && target.skin)
    {
        // The front face and transparent hat layer both come from this player's skin.
        const ImTextureID texture = reinterpret_cast<ImTextureID>(target.skin);
        draw->AddImage(texture, point(10.f, 10.f), point(56.f, 56.f), ImVec2(8.f / 64.f, 8.f / 64.f), ImVec2(16.f / 64.f, 16.f / 64.f));
        draw->AddImage(texture, point(10.f, 10.f), point(56.f, 56.f), ImVec2(40.f / 64.f, 8.f / 64.f), ImVec2(48.f / 64.f, 16.f / 64.f));
    }
    else text(26.f, 24.f, 18.f, 20.f, muted, "?");
    const auto display_name=services::privacy::display_name(target.name);
    text(65.f, 10.f, 13.f, 157.f, primary, target.valid ? display_name.c_str() : gui::text("Target HUD", "Hedef HUD"), true);
    char value[96]{};
    if (target.valid) std::snprintf(value, sizeof(value), "%.1f / %.0f HP", target.health, target.maximum_health);
    else std::snprintf(value, sizeof(value), "-- / -- HP");
    text(65.f, 30.f, 10.f, 101.f, primary, value);
    if (target.valid) std::snprintf(value, sizeof(value), "%.1f m", target.distance);
    else std::snprintf(value, sizeof(value), "-- m");
    const float distance_width = font->CalcTextSizeA(10.f * scale, FLT_MAX, 0.f, value).x / scale;
    text(std::max(170.f, 222.f - distance_width), 30.f, 10.f, 52.f, muted, value);
    const float ratio = target.valid ? target.health / target.maximum_health : 0.f;
    const float dt = std::clamp(ImGui::GetIO().DeltaTime, 0.f, .05f);
    runtime_->health_ratio = gui::aegis::get_preferences().reduce_motion ? ratio
        : runtime_->health_ratio + (ratio - runtime_->health_ratio) * (1.f - std::exp(-14.f * dt));
    draw->AddRectFilled(point(65.f, 47.f), point(222.f, 51.f), IM_COL32(255, 255, 255, 18), 2.f * scale);
    const ImVec4 health_color = ratio < .25f ? ImVec4(.98f, .31f, .37f, 1.f) : accent;
    if (runtime_->health_ratio > .001f)
        draw->AddRectFilled(point(65.f, 47.f), point(65.f + 157.f * std::clamp(runtime_->health_ratio, 0.f, 1.f), 51.f),
            color(health_color), 2.f * scale);

    const std::array<const equipment*, 5> items = {&target.hand, &target.armor[0], &target.armor[1],
        &target.armor[2], &target.armor[3]};
    // One compact row: held item, helmet, chestplate, leggings and boots.
    for (int index = 0; index < static_cast<int>(items.size()); ++index)
    {
        const float left = 64.f + index * 32.f, top = 60.f;
        const auto& item = *items[index];
        draw->AddRectFilled(point(left, top), point(left + 26.f, top + 26.f),
            index == 0 ? color(accent, .09f) : IM_COL32(255, 255, 255, 8), 3.f * scale);
        draw->PushClipRect(point(left, top), point(left + 26.f, top + 26.f), true);
        for (const auto& quad : item.icon)
        {
            const auto vertex = [&](int i) { return point(left + 2.f + quad.position[i].x * 22.f,
                top + 2.f + quad.position[i].y * 22.f); };
            draw->AddImageQuad(reinterpret_cast<ImTextureID>(quad.texture), vertex(0), vertex(1), vertex(2), vertex(3),
                quad.uv[0], quad.uv[1], quad.uv[2], quad.uv[3], quad.tint);
        }
        draw->PopClipRect();
        if (item.icon.empty())
        {
            if (!item.known || item.present) text(left + 9.f, top + 6.f, 10.f, 10.f, muted, "?");
            else draw->AddLine(point(left + 10.f, top + 13.f), point(left + 16.f, top + 13.f),
                IM_COL32(155, 161, 177, 65), scale);
        }
        if (item.maximum > 0)
        {
            const float durability = static_cast<float>(item.maximum - item.damage) / item.maximum;
            draw->AddRectFilled(point(left + 3.f, top + 23.f), point(left + 23.f, top + 25.f), IM_COL32(9, 10, 13, 240));
            if (durability > 0.f) draw->AddRectFilled(point(left + 3.f, top + 23.f),
                point(left + 3.f + 20.f * durability, top + 25.f),
                ImColor::HSV(durability / 3.f, .85f, .95f));
        }
        if (item.enchanted) draw->AddRect(point(left, top), point(left + 26.f, top + 26.f),
            IM_COL32(170, 127, 241, 105), 3.f * scale);
    }
}

void modules::target_hud::render_overlay()
{
    if (gui::hud_editor_active) return;
    refresh(false);
    if (!enabled || gui::draw) return;
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    if (display.x < 1.f || display.y < 1.f) return;
    const auto area = get_bounds(display.x, display.y);
    const ImVec2 origin = ImGui::GetMainViewport()->Pos;
    draw_card(ImGui::GetBackgroundDrawList(), origin.x + area.x, origin.y + area.y, false);
}

void modules::target_hud::render_hud_preview(ImDrawList* draw, float x, float y)
{
    refresh(true);
    draw_card(draw, x, y, true);
}

void modules::target_hud::render_options()
{
    gui::settings::render(settings_, module_settings::target_hud::schema());
}
