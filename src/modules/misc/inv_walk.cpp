#include "inv_walk.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../../gui/gui.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../services/game/client_context.hpp"
#include "../../services/game/client_bridge.hpp"
#include "../../services/input/input_service.hpp"
#include "../ui_redirect.hpp"
#include <optional>

namespace
{
    // Resolve the actual Minecraft binding, not hard-coded WASD/Space. Read
    // tracked hardware input so synthetic events from other modules don't move us.
    std::optional<bool> physical_binding(const maps::InputKey& key)
    {
        if (!key || !key.getCode.is_valid() || !key.getCategory.is_valid()) return std::nullopt;
        const int code = key.getCode.call();
        auto type = key.getCategory.call();
        auto symbols = maps::InputKeyType{}.KEYSYM.get();
        auto scans = maps::InputKeyType{}.SCANCODE.get();
        auto mouse = maps::InputKeyType{}.MOUSE.get();
        if (platform::jni_context::clear_exception() || !type || !symbols || !scans || !mouse) return std::nullopt;
        bool pressed = false;
        if (type.is_same_object(symbols))
        {
            const auto vk = services::input::windows_key_for_glfw(code);
            if (!vk) return std::nullopt;
            pressed = code == 257 ? services::input::physical_scan_down(0x1c) :
                code == 335 ? services::input::physical_scan_down(0x11c) : services::input::physical_key_down(vk);
        }
        else if (type.is_same_object(scans))
        {
            if (code <= 0 || code >= 512) return std::nullopt;
            pressed = services::input::physical_scan_down(static_cast<unsigned>(code));
        }
        else if (type.is_same_object(mouse))
        {
            constexpr unsigned buttons[]{VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2};
            if (code < 0 || code >= 5) return std::nullopt;
            pressed = services::input::physical_key_down(buttons[code]);
        }
        else return std::nullopt;
        return platform::jni_context::clear_exception() ? std::nullopt : std::optional<bool>{pressed};
    }

    bool focused_text(const maps::TextFieldWidget& field)
    {
        if (!field) return false;
        const bool focused = field.isFocused.call() == JNI_TRUE;
        const bool visible = field.isVisible.call() == JNI_TRUE;
        const bool active = field.isActive.call() == JNI_TRUE;
        if (platform::jni_context::clear_exception()) return true; // Fail closed.
        return focused && visible && active;
    }

    bool text_in_tree(const maps::Object& object, int depth, int& remaining)
    {
        if (!object) return false;
        if (depth > 12 || --remaining < 0) return true;
        if (object.is_instance_of<maps::TextFieldWidget>())
            return focused_text(maps::TextFieldWidget(object.object_instance, false));
        if (platform::jni_context::clear_exception()) return true;
        if (!object.is_instance_of<maps::ParentElement>())
            return platform::jni_context::clear_exception();
        maps::ParentElement parent(object.object_instance, false);
        auto children = parent.children.call();
        auto array = children ? children.toArray.call() : jni::array<maps::Object>{nullptr};
        if (platform::jni_context::clear_exception() || !array) return true;
        const int count = array.get_length();
        if (platform::jni_context::clear_exception() || count > remaining) return true;
        for (int index = 0; index < count; ++index)
        {
            jni::frame frame{16};
            maps::Object child(jni::get_env()->GetObjectArrayElement(static_cast<jobjectArray>(array.object_instance), index), false);
            if (platform::jni_context::clear_exception() || text_in_tree(child, depth + 1, remaining)) return true;
        }
        return false;
    }

    bool screen_is_typing(const maps::Screen& screen)
    {
        int remaining = 512;
        if (text_in_tree(maps::Object(screen.object_instance, false), 0, remaining)) return true;
        if (screen.is_instance_of<maps::CreativeInventoryScreen>())
        {
            auto field = maps::CreativeInventoryScreen(screen.object_instance, false).searchBox.get();
            if (platform::jni_context::clear_exception() || focused_text(field)) return true;
        }
        auto book = maps::recipe_book(screen);
        if (platform::jni_context::clear_exception()) return true;
        if (book)
        {
            const bool open = book.isOpen.call() == JNI_TRUE;
            auto field = book.searchField.get();
            if (platform::jni_context::clear_exception() || (open && focused_text(field))) return true;
        }
        return platform::jni_context::clear_exception();
    }
}

bool modules::inv_walk::release_key(owned_key& owned, bool vanilla)
{
    if (!owned.key) return true;
    if (!owned.key.isPressed.is_valid() || !owned.key.setPressed.is_valid()) return false;
    const bool pressed = owned.key.isPressed.call() == JNI_TRUE;
    if (platform::jni_context::clear_exception()) return false;
    bool physical = false;
    if (vanilla)
    {
        auto binding = owned.key.boundKey.get();
        if (platform::jni_context::clear_exception()) return false;
        // Rebinding while active must not preserve the old physical key's hold.
        const auto state = physical_binding(binding);
        physical = state.value_or(false);
    }
    if (inv_walk_engine::should_release(true, pressed, vanilla, physical))
    {
        owned.key.setPressed.call(JNI_FALSE);
        if (platform::jni_context::clear_exception()) return false;
    }
    owned.key.clear_ref(); owned.binding.clear_ref();
    return true;
}

bool modules::inv_walk::release_owned(bool vanilla)
{
    bool released = true;
    for (auto& key : owned_) if (!release_key(key, vanilla)) released = false;
    if (released) { world_.clear_ref(); player_.clear_ref(); }
    return released;
}

void modules::inv_walk::update_key(unsigned index, const maps::KeyBinding& key, bool desired)
{
    auto& owned = owned_[index];
    auto binding = key ? key.boundKey.get() : maps::InputKey{};
    if (platform::jni_context::clear_exception() || !key || !binding) { release_key(owned, false); return; }
    if (owned.key && (!owned.key.is_same_object(key) || !owned.binding.is_same_object(binding)))
        if (!release_key(owned, false)) return;
    if (platform::jni_context::clear_exception()) return;
    if (!desired) { release_key(owned, false); return; }
    if (!key.isPressed.is_valid() || !key.setPressed.is_valid()) return;
    const bool pressed = key.isPressed.call() == JNI_TRUE;
    if (platform::jni_context::clear_exception()) return;
    if (pressed) return; // Never claim a press made by vanilla or another module.
    owned.key = key; owned.binding = binding;
    if (platform::jni_context::clear_exception()) return;
    key.setPressed.call(JNI_TRUE);
    platform::jni_context::clear_exception(); // Keep ownership if the write failed; retry cleanup.
}

void modules::inv_walk::cancel_on_client_thread()
{
    std::lock_guard lock(runtime_mutex);
    reset_requested.store(true);
    if (!platform::jni_context::bind_current_thread()) return;
    jni::frame frame{64};
    try { release_owned(false); services::game::client_bridge::release_walk(false); }
    catch (...) { platform::jni_context::clear_exception(); }
    platform::jni_context::clear_exception();
}

bool modules::inv_walk::gate_inventory_input(UINT message, WPARAM key, LPARAM data)
{
    if (!enabled || gui::draw || gui::loader_visible || gui::should_unload) return false;
    int kind=2,code=0;
    switch(message) {
    case WM_KEYDOWN: case WM_SYSKEYDOWN:
        if ((key==VK_INSERT && ((data>>16)&0x1ff)==0x152) || key==VK_END || key==VK_LWIN || key==VK_RWIN || key==VK_MENU ||
            (message==WM_SYSKEYDOWN && key==VK_F4)) return false;
        kind=0;code=static_cast<int>((data>>16)&0x1ff);break;
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN: case WM_XBUTTONDOWN:
    case WM_LBUTTONDBLCLK: case WM_RBUTTONDBLCLK: case WM_MBUTTONDBLCLK: case WM_XBUTTONDBLCLK:
    case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL: break;
    case WM_INPUT: {
        RAWINPUT raw{};UINT size=sizeof(raw);
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(data),RID_INPUT,&raw,&size,sizeof(RAWINPUTHEADER))!=sizeof(raw) ||
            raw.header.dwType!=RIM_TYPEMOUSE || !(raw.data.mouse.usButtonFlags &
            (RI_MOUSE_LEFT_BUTTON_DOWN|RI_MOUSE_RIGHT_BUTTON_DOWN|RI_MOUSE_MIDDLE_BUTTON_DOWN|
             RI_MOUSE_BUTTON_4_DOWN|RI_MOUSE_BUTTON_5_DOWN|RI_MOUSE_WHEEL|RI_MOUSE_HWHEEL))) return false;
        break;
    }
    default: return false;
    }
    return !services::game::client_bridge::prepare_walk_action(kind,code);
}

void modules::inv_walk::render_overlay()
{
    std::lock_guard lock(runtime_mutex);
    if (!platform::jni_context::bind_current_thread()) { reset_requested.store(true); return; }
    struct clear_on_exit { ~clear_on_exit() { platform::jni_context::clear_exception(); } } cleanup;
    jni::frame frame{160};
    try
    {
        if (reset_requested.exchange(false)) {
            if (!release_owned(false) || !services::game::client_bridge::release_walk(false)) { reset_requested.store(true); return; }
        }
        const HWND window = gui::get_window();
        inv_walk_engine::observation state;
        state.enabled = enabled;
        state.focused = window && GetForegroundWindow() == window;
        state.client_blocked = gui::draw || gui::loader_visible || gui::should_unload || services::game::client_bridge::is_inventory_busy();
        if (!state.enabled || !state.focused || state.client_blocked) { release_owned(false); services::game::client_bridge::release_walk(false); return; }
        namespace bridge = services::game::client_bridge;
        bridge::walk_snapshot observed;
        const auto backend = bridge::capture_walk(observed);
        if (backend != services::game::hybrid::result::native_fallback) {
            if (backend != services::game::hybrid::result::ready) { bridge::release_walk(false); return; }
            state.live = observed.alive; state.typing = observed.typing;
            state.screen = static_cast<inv_walk_engine::screen_kind>(observed.screen);
            const auto config = settings_.read();
            const auto mode = inv_walk_engine::choose_mode(config, state);
            if (mode != inv_walk_engine::mode::inventory) { bridge::release_walk(mode == inv_walk_engine::mode::vanilla); return; }
            if (!bridge::apply_walk(observed, inv_walk_engine::desired_keys(config, observed.physical))) bridge::release_walk(false);
            return;
        }
        services::game::client_context context;
        if (!services::game::capture_client_context(context, true, false)) { release_owned(false); return; }
        if (world_ && (!world_.is_same_object(context.world) || !player_.is_same_object(context.player)))
            if (!release_owned(false)) return;
        if (platform::jni_context::clear_exception()) { release_owned(false); return; }
        state.live = context.player.isAlive.call() == JNI_TRUE;
        if (platform::jni_context::clear_exception() || !state.live) { release_owned(false); return; }
        if (!context.screen) state.screen = inv_walk_engine::screen_kind::gameplay;
        else if (context.screen.is_instance_of<maps::HandledScreen>())
        {
            const bool inventory = context.screen.is_instance_of<maps::InventoryScreen>() ||
                context.screen.is_instance_of<maps::CreativeInventoryScreen>();
            state.screen = inventory ? inv_walk_engine::screen_kind::inventory : inv_walk_engine::screen_kind::container;
            state.typing = screen_is_typing(context.screen);
        }
        if (platform::jni_context::clear_exception()) { release_owned(false); return; }
        const auto config = settings_.read();
        const auto mode = inv_walk_engine::choose_mode(config, state);
        if (mode != inv_walk_engine::mode::inventory)
        {
            release_owned(mode == inv_walk_engine::mode::vanilla); return;
        }
        if (context.player && context.player.isSprinting.call() == JNI_TRUE && !platform::jni_context::clear_exception())
            context.player.setSprinting.call(JNI_FALSE);
        platform::jni_context::clear_exception();
        auto options = context.instance.options.get();
        if (platform::jni_context::clear_exception() || !options) { release_owned(false); return; }
        const std::array<maps::KeyBinding, inv_walk_engine::key_count> keys{
            options.forwardKey.get(), options.backKey.get(), options.leftKey.get(), options.rightKey.get(),
            options.jumpKey.get(), options.sneakKey.get()};
        if (platform::jni_context::clear_exception()) { release_owned(false); return; }
        std::array<bool, inv_walk_engine::key_count> physical{};
        for (unsigned index = 0; index < keys.size(); ++index)
        {
            auto binding = keys[index] ? keys[index].boundKey.get() : maps::InputKey{};
            auto held = physical_binding(binding);
            if (platform::jni_context::clear_exception()) { release_owned(false); return; }
            // An unbound/unsupported optional key must not disable WASD.
            // Unknown bindings remain released, never guessed or hard-coded.
            physical[index] = held.value_or(false);
        }
        const bool mouse_interacting = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 || (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        const auto desired = mouse_interacting ? std::array<bool, inv_walk_engine::key_count>{} : inv_walk_engine::desired_keys(config, physical);
        world_ = context.world; player_ = context.player;
        if (platform::jni_context::clear_exception()) { release_owned(false); return; }
        for (unsigned index = 0; index < keys.size(); ++index) update_key(index, keys[index], desired[index]);
    }
    catch (...) { platform::jni_context::clear_exception(); release_owned(false); services::game::client_bridge::release_walk(false); }
}

void modules::inv_walk::render_options()
{
    gui::settings::render(settings_, module_settings::inv_walk::schema());
    ImGui::TextWrapped("%s", "Pauses while typing and briefly before inventory clicks, hotkeys or closing. Walking resumes after the action. Server compatibility varies.");
}
