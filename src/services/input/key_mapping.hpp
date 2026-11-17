#pragma once

namespace services::input
{
    // GLFW symbolic key codes -> Windows virtual keys. Unsupported keys fail
    // closed; scancode bindings are handled separately using recorded scancodes.
    inline unsigned windows_key_for_glfw(int key)
    {
        if ((key >= 48 && key <= 57) || (key >= 65 && key <= 90) || key == 32) return static_cast<unsigned>(key);
        if (key >= 290 && key <= 313) return static_cast<unsigned>(0x70 + key - 290);
        if (key >= 320 && key <= 329) return static_cast<unsigned>(0x60 + key - 320);
        switch (key) {
        case 256: return 0x1b; case 257: return 0x0d; case 258: return 0x09; case 259: return 0x08;
        case 260: return 0x2d; case 261: return 0x2e; case 262: return 0x27; case 263: return 0x25;
        case 264: return 0x28; case 265: return 0x26; case 266: return 0x21; case 267: return 0x22;
        case 268: return 0x24; case 269: return 0x23; case 280: return 0x14; case 281: return 0x91;
        case 282: return 0x90; case 283: return 0x2c; case 284: return 0x13;
        case 330: return 0x6e; case 331: return 0x6f; case 332: return 0x6a; case 333: return 0x6d;
        case 334: return 0x6b; case 335: return 0x0d;
        case 340: return 0xa0; case 341: return 0xa2; case 342: return 0xa4; case 343: return 0x5b;
        case 344: return 0xa1; case 345: return 0xa3; case 346: return 0xa5; case 347: return 0x5c;
        case 39: return 0xde; case 44: return 0xbc; case 45: return 0xbd; case 46: return 0xbe;
        case 47: return 0xbf; case 59: return 0xba; case 61: return 0xbb; case 91: return 0xdb;
        case 92: return 0xdc; case 93: return 0xdd; case 96: return 0xc0;
        default: return 0;
        }
    }
}
