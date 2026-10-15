#pragma once

namespace gui
{
    inline int active_language = 0; // 0: English, 1: Turkish
    inline bool is_turkish() { return active_language == 1; }
    inline const char* text(const char* english, const char* turkish)
    {
        return is_turkish() ? turkish : english;
    }
}
