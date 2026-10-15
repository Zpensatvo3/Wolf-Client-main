#include <Windows.h>
#include <thread>
#include <jni.h>
#include <unordered_map>
#include "gui/gui.hpp"
#include "modules/module_lifecycle.hpp"
#include "modules/core/module_registry.hpp"
#include "modules/combat/jump_reset.hpp"
#include "services/input/input_service.hpp"
#include "java_hook/java_hook.hpp"
#include "cache/cache.hpp"
#include "meta_jni.hpp"
#include "jvmti/jvmti.hpp"
#include "loader/loader_ui.hpp"

#include <iostream>
#include <string>
#include <algorithm>
#include <chrono>
#include "auth/auth.hpp"
#include "configs/configs.hpp"
#include "utils/version_manager.hpp"
#include "platform/adapter/game_adapter.hpp"

JavaVM* jvm = nullptr;
JNIEnv* env = nullptr;
jvmti* jvmti_env = nullptr;

typedef jint(JNICALL* JNI_GetCreatedJavaVMs_t)(JavaVM**, jsize, jsize*);

void main_thread(HMODULE hModule)
{
	// The license gate is intentionally bypassed in this temporary build.  Keep
	// the former flow compiled but unreachable so it can be restored without
	// losing the operator UI.
	if constexpr (false)
	{
	AllocConsole();

	FILE* dummy_file = nullptr;
	freopen_s(&dummy_file, "CONIN$", "r", stdin);
	freopen_s(&dummy_file, "CONOUT$", "w", stdout);
	freopen_s(&dummy_file, "CONOUT$", "w", stderr);

	HANDLE hCon = GetStdHandle(STD_OUTPUT_HANDLE);
	HWND hConsoleWindow = GetConsoleWindow();

	constexpr SHORT CONSOLE_WIDTH = 88;
	constexpr SHORT CONSOLE_HEIGHT = 30;
	constexpr int CONTENT_WIDTH = 78;

	// Configure the console before drawing. Buffer and window use the same size,
	// preventing scrollbars and keeping the layout stable.
	COORD bufferSize = { CONSOLE_WIDTH, CONSOLE_HEIGHT };
	SetConsoleScreenBufferSize(hCon, bufferSize);

	SMALL_RECT windowSize = { 0, 0, CONSOLE_WIDTH - 1, CONSOLE_HEIGHT - 1 };
	SetConsoleWindowInfo(hCon, TRUE, &windowSize);
	SetConsoleTitleA("onestep | Secure Authentication");

	if (hConsoleWindow)
	{
		// Keep the small authentication window centered and prevent accidental resizing
		// from breaking the fixed console layout.
		const int windowWidth = 860;
		const int windowHeight = 520;
		const int x = (GetSystemMetrics(SM_CXSCREEN) - windowWidth) / 2;
		const int y = (GetSystemMetrics(SM_CYSCREEN) - windowHeight) / 2;
		MoveWindow(hConsoleWindow, x, y, windowWidth, windowHeight, TRUE);

		HMENU systemMenu = GetSystemMenu(hConsoleWindow, FALSE);
		if (systemMenu)
		{
			DeleteMenu(systemMenu, SC_SIZE, MF_BYCOMMAND);
			DeleteMenu(systemMenu, SC_MAXIMIZE, MF_BYCOMMAND);
		}
	}

	CONSOLE_FONT_INFOEX fontInfo{};
	fontInfo.cbSize = sizeof(fontInfo);
	fontInfo.dwFontSize.X = 0;
	fontInfo.dwFontSize.Y = 18;
	fontInfo.FontFamily = FF_DONTCARE;
	fontInfo.FontWeight = FW_NORMAL;
	wcscpy_s(fontInfo.FaceName, L"Consolas");
	SetCurrentConsoleFontEx(hCon, FALSE, &fontInfo);

	// Win32 console colors. Kept intentionally small and consistent.
	const WORD C_MUTED   = FOREGROUND_INTENSITY;
	const WORD C_BORDER  = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
	const WORD C_TEXT    = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
	const WORD C_WHITE   = C_TEXT | FOREGROUND_INTENSITY;
	const WORD C_ACCENT  = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
	const WORD C_INFO    = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
	const WORD C_SUCCESS = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
	const WORD C_WARNING = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
	const WORD C_ERROR   = FOREGROUND_RED | FOREGROUND_INTENSITY;

	auto set_color = [&](WORD color)
	{
		SetConsoleTextAttribute(hCon, color);
	};

	auto set_cursor_visible = [&](bool visible)
	{
		CONSOLE_CURSOR_INFO cursorInfo{};
		if (GetConsoleCursorInfo(hCon, &cursorInfo))
		{
			cursorInfo.bVisible = visible ? TRUE : FALSE;
			cursorInfo.dwSize = 20;
			SetConsoleCursorInfo(hCon, &cursorInfo);
		}
	};

	auto clear_console = [&]()
	{
		CONSOLE_SCREEN_BUFFER_INFO csbi{};
		if (!GetConsoleScreenBufferInfo(hCon, &csbi))
			return;

		const DWORD cells = static_cast<DWORD>(csbi.dwSize.X) * static_cast<DWORD>(csbi.dwSize.Y);
		DWORD written = 0;
		const COORD origin = { 0, 0 };

		FillConsoleOutputCharacterA(hCon, ' ', cells, origin, &written);
		FillConsoleOutputAttribute(hCon, C_TEXT, cells, origin, &written);
		SetConsoleCursorPosition(hCon, origin);
	};

	auto fit_text = [](const std::string& value, std::size_t width)
	{
		if (value.size() <= width)
			return value + std::string(width - value.size(), ' ');

		if (width <= 3)
			return value.substr(0, width);

		return value.substr(0, width - 3) + "...";
	};

	auto mask_value = [](const std::string& value, std::size_t head, std::size_t tail)
	{
		if (value.empty())
			return std::string("Not available");

		if (value.size() <= head + tail + 3)
			return value;

		return value.substr(0, head) + "..." + value.substr(value.size() - tail);
	};

	auto print_rule = [&](WORD color)
	{
		set_color(color);
		std::cout << "  " << std::string(CONTENT_WIDTH, '-') << "\n";
	};

	auto print_row = [&](const std::string& left, WORD leftColor, const std::string& right = "", WORD rightColor = 0)
	{
		if (rightColor == 0)
			rightColor = C_TEXT;
		const std::size_t rightWidth = right.empty() ? 0 : std::min<std::size_t>(right.size(), 25);
		const std::size_t gap = right.empty() ? 0 : 4;
		const std::size_t leftWidth = CONTENT_WIDTH - rightWidth - gap;

		std::cout << "  ";
		set_color(leftColor);
		std::cout << fit_text(left, leftWidth);

		if (!right.empty())
		{
			set_color(C_MUTED);
			std::cout << std::string(gap, ' ');
			set_color(rightColor);
			std::cout << fit_text(right, rightWidth);
		}

		std::cout << "\n";
	};

	auto print_section_title = [&](const std::string& title)
	{
		std::cout << "\n  ";
		set_color(C_ACCENT);
		std::cout << "[ ";
		set_color(C_WHITE);
		std::cout << title;
		set_color(C_ACCENT);
		std::cout << " ]\n";
		print_rule(C_BORDER);
	};

	auto render_header = [&]()
	{
		clear_console();
		set_cursor_visible(false);

		std::cout << "\n\n  ";
		set_color(C_WHITE);
		std::cout << "one";
		set_color(C_ACCENT);
		std::cout << "step";
		set_color(C_MUTED);
		std::cout << "   /   SECURE ACCESS";
		std::cout << "\n  ";
		set_color(C_MUTED);
		std::cout << "Native runtime  |  Fabric " << mc::get_version_name() << "  |  Protected session\n";
		print_rule(C_BORDER);
	};

	auto print_progress_bar = [&](int percent, WORD fillColor)
	{
		constexpr int BAR_WIDTH = 54;
		const int clamped = std::clamp(percent, 0, 100);
		const int filled = (clamped * BAR_WIDTH) / 100;

		std::cout << "  ";
		set_color(C_MUTED);
		std::cout << "Preparing client   ";
		set_color(C_BORDER);
		std::cout << "[";
		for (int i = 0; i < BAR_WIDTH; ++i)
		{
			if (i < filled)
			{
				set_color(fillColor);
				std::cout << '#';
			}
			else
			{
				set_color(C_MUTED);
				std::cout << '.';
			}
		}

		set_color(C_BORDER);
		std::cout << "] ";
		set_color(C_WHITE);
		std::cout << (clamped < 100 ? " " : "") << (clamped < 10 ? " " : "") << clamped << "%";
		std::cout << std::string(4, ' ') << "\r" << std::flush;
	};

	const std::string endpoint = "wolf-auth-api.gogogamesas.workers.dev";
	const std::string hwid_str = auth::generate_hwid();
	const std::string masked_hwid = mask_value(hwid_str, 10, 6);

	// -------------------------------------------------------------------------
	// Authentication landing screen
	// -------------------------------------------------------------------------
	render_header();

	print_section_title("DEVICE");
	print_row("Fingerprint", C_MUTED, masked_hwid, C_WHITE);
	print_row("Connection", C_MUTED, "HTTPS / TLS", C_INFO);
	print_row("Status", C_MUTED, "READY FOR KEY", C_WARNING);

	print_section_title("LICENSE KEY");
	print_row("Paste the key assigned to this device.", C_MUTED);
	std::cout << "\n  ";
	set_color(C_MUTED);
	std::cout << "KEY  ";
	set_color(C_ACCENT);
	std::cout << "> ";
	set_color(C_WHITE);
	set_cursor_visible(true);

	std::string key_input;
	std::getline(std::cin >> std::ws, key_input);
	set_cursor_visible(false);

	std::cout << "\n";
	print_rule(C_BORDER);

	// -------------------------------------------------------------------------
	// Authentication progress
	// -------------------------------------------------------------------------
	print_section_title("VERIFYING LICENSE");

	const char spinner[] = { '|', '/', '-', '\\' };
	for (int i = 0; i < 10; ++i)
	{
		std::cout << "\r  ";
		set_color(C_INFO);
		std::cout << '[' << spinner[i % 4] << "] ";
		set_color(C_WHITE);
		std::cout << "Creating encrypted request";
		std::cout << std::string(i % 4, '.');
		std::cout << std::string(3 - (i % 4), ' ');
		std::cout << std::string(42, ' ') << std::flush;
		std::this_thread::sleep_for(std::chrono::milliseconds(65));
	}

	std::cout << "\r  ";
	set_color(C_INFO);
	std::cout << "[>] ";
	set_color(C_WHITE);
	std::cout << "Contacting secure authentication edge";
	std::cout << std::string(37, ' ') << "\n";

	auth::auth_result res = auth::authenticate(key_input, endpoint);

	if (!res.success)
	{
		render_header();

		print_section_title("ACCESS DENIED");
		print_row("This license could not be verified.", C_ERROR);
		print_row("Response", C_MUTED, fit_text(res.message, 25), C_ERROR);
		print_row("Client state", C_MUTED, "LOCKED", C_ERROR);

		print_section_title("CLOSING SESSION");

		for (int second = 4; second >= 0; --second)
		{
			constexpr int CLOSE_BAR_WIDTH = 48;
			const int percent = ((4 - second) * 100) / 4;
			const int filled = (percent * CLOSE_BAR_WIDTH) / 100;

			std::cout << "\r  ";
			set_color(C_MUTED);
			std::cout << "Closing securely  ";
			set_color(C_BORDER);
			std::cout << "[";
			for (int i = 0; i < CLOSE_BAR_WIDTH; ++i)
			{
				if (i < filled)
				{
					set_color(C_ERROR);
					std::cout << '#';
				}
				else
				{
					set_color(C_MUTED);
					std::cout << '.';
				}
			}

			set_color(C_BORDER);
			std::cout << "]  ";
			set_color(C_MUTED);
			std::cout << "exit ";
			set_color(C_WHITE);
			std::cout << second << "s";
			std::cout << std::string(13, ' ') << std::flush;

			std::this_thread::sleep_for(std::chrono::seconds(1));
		}

		std::cout << "\n\n";
		set_color(C_TEXT);
		set_cursor_visible(true);
		SecureZeroMemory(key_input.data(), key_input.size());
		key_input.clear();

		FreeConsole();
		FreeLibraryAndExitThread(hModule, 0);
		return;
	}

	// Heartbeat starts only after the client modules are fully initialized.

	// -------------------------------------------------------------------------
	// Success dashboard
	// -------------------------------------------------------------------------
	render_header();

	print_section_title("ACCESS GRANTED");
	print_row("License verified successfully.", C_SUCCESS);
	print_row("License", C_MUTED, mask_value(key_input, 9, 5), C_WHITE);
	print_row("Expiration", C_MUTED, res.expires_at, C_INFO);
	print_row("Session", C_MUTED, "SECURE", C_SUCCESS);

	print_section_title("STARTING CLIENT");

	for (int percent = 0; percent <= 100; percent += 4)
	{
		print_progress_bar(percent, percent == 100 ? C_SUCCESS : C_INFO);
		std::this_thread::sleep_for(std::chrono::milliseconds(24));
	}

	std::cout << "\n";
	print_row("onestep engine initialized.", C_SUCCESS, "READY", C_SUCCESS);

	std::cout << "\n";
	set_color(C_MUTED);
	std::cout << "  The loader will continue automatically.\n";
	set_color(C_TEXT);
	std::this_thread::sleep_for(std::chrono::milliseconds(650));

	SecureZeroMemory(key_input.data(), key_input.size());
	key_input.clear();
	set_cursor_visible(true);
	FreeConsole();
	}

	const std::string endpoint = "wolf-auth-api.gogogamesas.workers.dev";
	auth::configure_config_service(endpoint);

	loader_ui::initialize();
	loader_ui::stage(0, "Locating Minecraft JVM runtime");

    while (!GetModuleHandleA("jvm.dll"))
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

	loader_ui::stage(0, "Minecraft runtime detected");
    HMODULE hJvm = GetModuleHandleA("jvm.dll");
    JNI_GetCreatedJavaVMs_t get_vms = (JNI_GetCreatedJavaVMs_t)GetProcAddress(hJvm, "JNI_GetCreatedJavaVMs");
	if (!get_vms)
	{
		loader_ui::fail("JNI_GetCreatedJavaVMs export not found");
		auth::controlled_shutdown();
		std::this_thread::sleep_for(std::chrono::milliseconds(1500));
		FreeLibraryAndExitThread(hModule, 0);
		return;
	}
    
    jsize count = 0;
    get_vms(&jvm, 1, &count);
    if (count == 0) 
    {
		loader_ui::fail("Minecraft JVM instance was not available");
		auth::controlled_shutdown();
		std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        FreeLibraryAndExitThread(hModule, 0);
        return;
    }

	loader_ui::stage(1, "Attaching native worker thread");
    if (jvm->AttachCurrentThread((void**)&env, nullptr) != JNI_OK)
    {
		loader_ui::fail("Failed to attach the native JVM thread");
		auth::controlled_shutdown();
		std::this_thread::sleep_for(std::chrono::milliseconds(1500));
		FreeLibraryAndExitThread(hModule, 0);
		return;
    }
    
	loader_ui::stage(1, "Resolving JNI metadata and mappings");
    jni::init();
    jni::set_thread_env(env);
    
	loader_ui::stage(2, "Building JVMTI class cache");
    jvmti_env = new jvmti(jvm);
    jni::set_custom_find_class([](const char* name) -> jclass {
        return jvmti_env->find_loaded_class(name);
    });

	mc::detect(env, jvmti_env);
	platform::adapter::initialize(mc::get_version());
	const std::string ver_stage = std::string("Detected runtime: Fabric ") + mc::get_version_name();
	loader_ui::stage(2, ver_stage.c_str());

	loader_ui::stage(2, "Synchronizing Minecraft client state");
    cache global_cache;

	loader_ui::stage(3, "Installing OpenGL render hook");
    if (!gui::init()) 
    {
		loader_ui::fail("OpenGL renderer hook could not be installed");
		auth::controlled_shutdown();
		std::this_thread::sleep_for(std::chrono::milliseconds(1500));
		delete jvmti_env;
		jni::shutdown();
		jvm->DetachCurrentThread();
		FreeLibraryAndExitThread(hModule, 0);
        return;
    }

	if (!java_hook::init())
	{
		loader_ui::fail("Java hook initialization failed");
		auth::controlled_shutdown();
		std::this_thread::sleep_for(std::chrono::milliseconds(1500));
		gui::draw = false;
		gui::shutdown();
		delete jvmti_env;
		jni::shutdown();
		jvm->DetachCurrentThread();
		FreeLibraryAndExitThread(hModule, 0);
		return;
	}
	loader_ui::stage(4, "Registering client modules");
    modules::init();
	configs::initialize();

	loader_ui::ready();
	std::unordered_map<modules::module*, bool> module_key_states{};

    while (!GetAsyncKeyState(VK_END) && !gui::should_unload)
    {
        global_cache.update();
        
		bool gameplay_input_allowed = !gui::draw && !gui::loader_visible && global_cache.is_valid();
		const HWND minecraft_window = gui::get_window();
		gameplay_input_allowed = gameplay_input_allowed && minecraft_window &&
			GetForegroundWindow() == minecraft_window;
		if (gameplay_input_allowed)
		{
			try
			{
				maps::Screen current_screen = global_cache.instance.currentScreen.get();
				if (env->ExceptionCheck() == JNI_TRUE)
				{
					env->ExceptionClear();
					gameplay_input_allowed = false;
				}
				else if (current_screen)
				{
					// Chat, inventory, pause and every other Minecraft screen block binds.
					gameplay_input_allowed = false;
				}
			}
			catch (...)
			{
				if (env->ExceptionCheck() == JNI_TRUE)
					env->ExceptionClear();
				gameplay_input_allowed = false;
			}
		}

		// Each module owns its edge state. Suppressed input is still sampled so a
		// key held while typing or alt-tabbed cannot toggle on focus return.
        for (modules::module* mod : modules::get_modules())
        {
			int bind = mod->get_keybind();
			bool is_pressed = bind > 0 && bind != gui::menu_key &&
				(GetAsyncKeyState(bind) & 0x8000) != 0;
			if (bind == VK_SPACE)
				is_pressed = bind != gui::menu_key && services::input::physical_key_down(VK_SPACE);
			bool& was_pressed = module_key_states[mod];
			if (bind <= 0 || !gameplay_input_allowed)
            {
				was_pressed = is_pressed;
			}
			else
			{
				if (is_pressed && !was_pressed)
					mod->enabled = !mod->enabled;
				was_pressed = is_pressed;
            }

            // Call lifecycle hooks (on_enable / on_disable)
            if (mod->enabled != mod->prev_enabled)
            {
                if (mod->enabled)
                {
                    mod->on_enable(global_cache);
                }
                else
                {
                    mod->on_disable(global_cache);
                }
                mod->prev_enabled = mod->enabled;
            }

            if (mod->enabled)
                mod->run(global_cache);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

	configs::shutdown();
	auth::controlled_shutdown();

	loader_ui::closing();

    // Stop the render callback first. It iterates the module list every frame;
    // deleting modules before the hook has returned causes a use-after-free.
    gui::draw = false;
    java_hook::shutdown();
    gui::shutdown();

    modules::shutdown(global_cache);
    
    delete jvmti_env;
    jni::shutdown();
    jvm->DetachCurrentThread();

    FreeLibraryAndExitThread(hModule, 0);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        if (auto hThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)main_thread, hModule, 0, nullptr))
            CloseHandle(hThread);
        break;
    }
    return TRUE;
}
