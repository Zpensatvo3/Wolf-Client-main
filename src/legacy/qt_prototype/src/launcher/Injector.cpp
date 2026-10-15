#include "Injector.hpp"

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

using namespace thedorius::launcher;

InjectionResult thedorius::launcher::injectLibrary(quint32 processId, const QString& absoluteDllPath)
{
#ifndef Q_OS_WIN
    Q_UNUSED(processId); Q_UNUSED(absoluteDllPath);
    return { false, QStringLiteral("Injection is only supported on Windows") };
#else
    HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, processId);
    if (!process) return { false, QStringLiteral("Minecraft process could not be opened (%1)").arg(GetLastError()) };

    const std::wstring path = absoluteDllPath.toStdWString();
    const SIZE_T bytes = (path.size() + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote)
    {
        CloseHandle(process);
        return { false, QStringLiteral("Remote memory allocation failed (%1)").arg(GetLastError()) };
    }
    SIZE_T written = 0;
    const bool copied = WriteProcessMemory(process, remote, path.c_str(), bytes, &written) && written == bytes;
    HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
    auto loadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(kernel, "LoadLibraryW"));
    HANDLE thread = copied && loadLibrary
        ? CreateRemoteThread(process, nullptr, 0, loadLibrary, remote, 0, nullptr) : nullptr;
    bool success = false;
    if (thread)
    {
        if (WaitForSingleObject(thread, 15000) == WAIT_OBJECT_0)
        {
            DWORD moduleHandle = 0;
            success = GetExitCodeThread(thread, &moduleHandle) && moduleHandle != 0;
        }
        CloseHandle(thread);
    }
    VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    CloseHandle(process);
    return success ? InjectionResult{ true, QStringLiteral("Client attached") }
                   : InjectionResult{ false, QStringLiteral("Client injection failed") };
#endif
}
