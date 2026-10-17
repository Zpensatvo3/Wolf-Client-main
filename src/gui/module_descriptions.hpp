#pragma once

#include <cstring>

namespace gui
{
    inline const char* module_description(const char* name)
    {
        if (!name) return "";
        struct entry { const char* name; const char* english; };
        static constexpr entry descriptions[]{
            { "friends manager", "Manage your friends list" },
            { "click gui", "Menu controls and shortcuts" },
            { "panic", "Close the client" },
            { "notifications", "Show module changes and messages" },
            { "nameprotect", "Show an alias instead of your name" },
            { "anti bot", "Skip suspected fake players" },
            { "legit click aim assist", "Gently help you aim while clicking" },
            { "blatant click aim assist", "Track targets strongly while clicking" },
            { "aim assist", "Help you aim at a target" },
            { "triggerbot", "Attack the target you aim at" },
            { "mace swap", "Switch to a mace before hitting" },
            { "anchor macro", "Place, charge and explode an anchor" },
            { "safe anchor", "Use an anchor while standing still" },
            { "hit crystal", "Place and break crystals after you hit" },
            { "crystal aura", "Attack with a sword and crystals" },
            { "jump reset", "Jump after taking a hit" },
            { "shield breaker", "Switch to an axe against shields" },
            { "auto totem", "Put a totem in your hand" },
            { "pearl macro", "Throw a pearl and switch back" },
            { "chest stealer", "Move chest items into your inventory" },
            { "elytra swapper", "Swap your elytra and chestplate" },
            { "auto tool", "Choose the right tool to break a block" },
            { "auto armor", "Equip your best armor" },
            { "sprint", "Sprint automatically while moving" },
            { "no jump delay", "Jump again without waiting" },
            { "fast stop", "Stop sliding when you stop moving" },
            { "inv walk", "Move while your inventory is open" },
            { "bridge assist", "Sneak at block edges" },
            { "right clicker", "Right-click automatically" },
            { "left clicker", "Left-click automatically" },
            { "inventory clicker", "Click faster in your inventory" },
            { "fast place", "Place blocks and use potions faster" },
            { "fast exp", "Throw experience bottles faster" },
            { "pot refill", "Move healing potions to empty hotbar slots" },
            { "mouse tweaks", "Move items easily by dragging" },
            { "watermark", "Show the client logo" },
            { "arraylist", "List your enabled modules" },
            { "glow esp", "Highlight players with a glow" },
            { "chams", "Color player models" },
            { "china hat", "Wear a cone hat in third person" },
            { "wings", "Wear animated wings in third person" },
            { "cape", "Wear a cape that sways as you move" },
            { "sky", "Change the sky and horizon colors" },
            { "trails", "Leave particles and footprints as you move" },
            { "player esp", "Mark players with boxes and labels" },
            { "item esp", "Mark items on the ground" },
            { "storage esp", "Mark chests and other storage blocks" },
            { "block esp", "Mark the blocks you choose" },
            { "radar", "Show nearby players on a radar" },
            { "full bright", "Make dark areas brighter" },
            { "nametag", "Show more details above players" },
            { "trajectories", "Show where thrown items will go" },
            { "spotify hud", "Show your Spotify song and lyrics" },
            { "target hud", "Show your target's health and equipment" },
            { "vape aim assist", "Help you aim at a target" },
            { "vape left clicker", "Left-click automatically" },
            { "vape right clicker", "Right-click automatically" },
        };
        for (const auto& description : descriptions)
            if (std::strcmp(description.name, name) == 0)
                return description.english;
        return "Client module";
    }
}
