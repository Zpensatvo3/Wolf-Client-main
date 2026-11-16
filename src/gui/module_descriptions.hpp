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
            { "self destruct", "Close the client" },
            { "notifications", "Show module changes and messages" },
            { "nameprotect", "Show an alias instead of your name" },
            { "anti bot", "Skip suspected fake players" },
            { "Click Aim Assist", "Aim while clicking with selectable Legit and Blatant modes" },
            { "Aim Assist V2", "Help you aim at a target" },
            { "triggerbot", "Attack the target you aim at" },
            { "mace swap", "Switch to a mace before hitting" },
            { "anchor macro", "Place, charge and explode an anchor" },
            { "safe anchor", "Place cover, charge and detonate an anchor while standing still" },
            { "hit crystal", "Hold right-click to place and break crystals" },
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
            { "hit particles", "Show particles when your target takes a hit" },
            { "jump circles", "Show rings when you jump or land" },
            { "block highlight", "Outline the block you look at" },
            { "view model", "Adjust the look of items in your hands" },
            { "player esp", "Mark players with boxes and labels" },
            { "item esp", "Mark items on the ground" },
            { "storage esp", "Mark chests and other storage blocks" },
            { "block esp", "Highlight selected blocks with 3D boxes" },
            { "radar", "Show nearby players on a radar" },
            { "full bright", "Make dark areas brighter" },
            { "nametag", "Show more details above players" },
            { "trajectories", "Show where thrown items will go" },
            { "spotify hud", "Show your Spotify song and lyrics" },
            { "target hud", "Show your target's health and equipment" },
            { "vape aim assist", "Help you aim at a target" },
        };
        for (const auto& description : descriptions)
            if (std::strcmp(description.name, name) == 0)
                return description.english;
        return "Client module";
    }
}
