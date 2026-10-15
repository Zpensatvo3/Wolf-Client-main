#include "ProcessDiscovery.hpp"

#ifdef Q_OS_WIN
#include <Windows.h>
#include <TlHelp32.h>
#endif

#include <QFileInfo>
#include <iterator>

namespace
{
#ifdef Q_OS_WIN
struct WindowSearch { DWORD pid{}; HWND window{}; };

BOOL CALLBACK findWindow(HWND window, LPARAM parameter)
{
    auto* search = reinterpret_cast<WindowSearch*>(parameter);
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    if (pid == search->pid && IsWindowVisible(window) && GetWindow(window, GW_OWNER) == nullptr)
    {
        wchar_t title[512]{};
        GetWindowTextW(window, title, static_cast<int>(std::size(title)));
        if (wcslen(title) > 0)
        {
            search->window = window;
            return FALSE;
        }
    }
    return TRUE;
}

bool hasJvmModule(DWORD pid)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) return false;
    MODULEENTRY32W module{ sizeof(module) };
    bool found = false;
    if (Module32FirstW(snapshot, &module))
    {
        do { if (_wcsicmp(module.szModule, L"jvm.dll") == 0) { found = true; break; } }
        while (Module32NextW(snapshot, &module));
    }
    CloseHandle(snapshot);
    return found;
}
#endif
}

QVariantList thedorius::launcher::ProcessDiscovery::minecraftProcesses() const
{
    QVariantList result;
#ifdef Q_OS_WIN
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return result;
    PROCESSENTRY32W process{ sizeof(process) };
    if (Process32FirstW(snapshot, &process))
    {
        do
        {
            const QString executable = QString::fromWCharArray(process.szExeFile).toLower();
            if (executable != QStringLiteral("javaw.exe") && executable != QStringLiteral("java.exe")) continue;
            if (!hasJvmModule(process.th32ProcessID)) continue;
            const HWND window = reinterpret_cast<HWND>(mainWindowForProcess(process.th32ProcessID));
            if (!window) continue;
            wchar_t title[512]{};
            GetWindowTextW(window, title, static_cast<int>(std::size(title)));
            const QString windowTitle = QString::fromWCharArray(title);
            if (!windowTitle.contains(QStringLiteral("Minecraft"), Qt::CaseInsensitive)) continue;
            result.push_back(QVariantMap{
                { "pid", process.th32ProcessID },
                { "title", windowTitle },
                { "compatible", windowTitle.contains(QStringLiteral("1.21.1"), Qt::CaseInsensitive) }
            });
        } while (Process32NextW(snapshot, &process));
    }
    CloseHandle(snapshot);
#endif
    return result;
}

quintptr thedorius::launcher::ProcessDiscovery::mainWindowForProcess(quint32 processId)
{
#ifdef Q_OS_WIN
    WindowSearch search{ static_cast<DWORD>(processId), nullptr };
    EnumWindows(findWindow, reinterpret_cast<LPARAM>(&search));
    return reinterpret_cast<quintptr>(search.window);
#else
    Q_UNUSED(processId);
    return 0;
#endif
}
