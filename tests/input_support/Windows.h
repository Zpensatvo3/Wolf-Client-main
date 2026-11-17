#pragma once
// Input-service test double only. No OS calls or real input injection.
#include <cstdint>
using HWND = void*;
using UINT = unsigned;
using WORD = unsigned short;
using DWORD = unsigned long;
using ULONG_PTR = std::uintptr_t;
constexpr UINT VK_LBUTTON=1, VK_RBUTTON=2, VK_SHIFT=0x10, VK_CONTROL=0x11, VK_MENU=0x12;
constexpr UINT VK_LCONTROL=0xa2, VK_RCONTROL=0xa3, VK_LMENU=0xa4, VK_RMENU=0xa5;
constexpr UINT MAPVK_VSC_TO_VK_EX=3, MAPVK_VK_TO_VSC=0;
constexpr DWORD INPUT_MOUSE=0, INPUT_KEYBOARD=1, MOUSEEVENTF_MOVE=1;
constexpr DWORD MOUSEEVENTF_LEFTDOWN=2, MOUSEEVENTF_LEFTUP=4, MOUSEEVENTF_RIGHTDOWN=8, MOUSEEVENTF_RIGHTUP=16;
constexpr DWORD KEYEVENTF_KEYUP=2;
struct INPUT {
    DWORD type{};
    struct { long dx{},dy{}; DWORD dwFlags{}; ULONG_PTR dwExtraInfo{}; } mi;
    struct { WORD wVk{},wScan{}; DWORD dwFlags{}; ULONG_PTR dwExtraInfo{}; } ki;
};
inline unsigned input_test_send_calls=0;
inline UINT SendInput(UINT, INPUT*, int) { ++input_test_send_calls; return 0; }
inline UINT MapVirtualKeyW(UINT key, UINT mode) {
    if (mode==MAPVK_VSC_TO_VK_EX) return key==0x36 ? 0xa1 : key==0x2a ? 0xa0 : 0;
    return key;
}
