#pragma once

namespace modules::auto_tool_logic
{
    struct mining_score { float speed = 1.0f; bool suitable = false; };
    // Extracted unchanged from Auto Tool; shared by native and Java I/O paths.
    inline bool is_better_score(const mining_score& candidate, const mining_score& current)
    {
        if (candidate.suitable != current.suitable) return candidate.suitable;
        return candidate.speed > current.speed + 0.01f;
    }
}
