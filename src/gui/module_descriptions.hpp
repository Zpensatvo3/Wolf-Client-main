#pragma once

#include "language.hpp"
#include <cstring>

namespace gui
{
    inline const char* module_description(const char* name)
    {
        if (!name) return "";
        struct entry { const char* name; const char* english; const char* turkish; };
        static constexpr entry descriptions[]{
            { "friends manager", "Manage your friends list", "Arkadaş listeni düzenler" },
            { "click gui", "Choose the menu language", "Menünün dilini değiştirir" },
            { "panic", "Close the client", "İstemciyi kapatır" },
            { "notifications", "Show module changes and messages", "Modül değişikliklerini ve bildirimleri gösterir" },
            { "nameprotect", "Show an alias instead of your name", "Kendi adının yerine takma ad gösterir" },
            { "anti bot", "Skip suspected fake players", "Bot olduğu düşünülen oyuncuları hedeflemez" },
            { "legit click aim assist", "Gently help you aim while clicking", "Tıklarken nişanını hafifçe hedefe yaklaştırır" },
            { "blatant click aim assist", "Track targets strongly while clicking", "Tıklarken hedefi daha güçlü takip eder" },
            { "aim assist", "Help you aim at a target", "Hedefe nişan almanı kolaylaştırır" },
            { "triggerbot", "Attack the target you aim at", "Nişan aldığın hedefe otomatik vurur" },
            { "mace swap", "Switch to a mace before hitting", "Vurmadan önce topuza geçer" },
            { "anchor macro", "Place, charge and explode an anchor", "Çapa koyar, doldurur ve patlatır" },
            { "safe anchor", "Use an anchor while standing still", "Durduğun yerde çapa kurup patlatır" },
            { "hit crystal", "Place and break crystals after you hit", "Vuruşundan sonra kristal koyup kırar" },
            { "crystal aura", "Attack with a sword and crystals", "Kılıç ve kristalle otomatik saldırır" },
            { "jump reset", "Jump after taking a hit", "Darbe alınca zıplar" },
            { "shield breaker", "Switch to an axe against shields", "Kalkan kırmak için baltaya geçer" },
            { "auto totem", "Put a totem in your hand", "Eline otomatik totem alır" },
            { "pearl macro", "Throw a pearl and switch back", "İnci atıp önceki eşyana döner" },
            { "chest stealer", "Move chest items into your inventory", "Sandıktaki eşyaları envanterine alır" },
            { "elytra swapper", "Swap your elytra and chestplate", "Elytra ile göğüslüğünü değiştirir" },
            { "auto tool", "Choose the right tool to break a block", "Blok kırmak için uygun aleti seçer" },
            { "auto armor", "Equip your best armor", "En iyi zırhlarını otomatik giyer" },
            { "sprint", "Sprint automatically while moving", "Hareket ederken otomatik koşar" },
            { "no jump delay", "Jump again without waiting", "Zıplamalar arasındaki beklemeyi kaldırır" },
            { "fast stop", "Stop sliding when you stop moving", "Yürümeyi bırakınca kaymanı durdurur" },
            { "inv walk", "Move while your inventory is open", "Envanter açıkken hareket etmeni sağlar" },
            { "bridge assist", "Sneak at block edges", "Blok kenarında otomatik eğilir" },
            { "right clicker", "Right-click automatically", "Otomatik sağ tıklar" },
            { "left clicker", "Left-click automatically", "Otomatik sol tıklar" },
            { "inventory clicker", "Click faster in your inventory", "Envanterde daha hızlı tıklar" },
            { "fast place", "Place blocks and use potions faster", "Blok koymayı ve iksir kullanmayı hızlandırır" },
            { "fast exp", "Throw experience bottles faster", "Tecrübe şişelerini daha hızlı atar" },
            { "pot refill", "Move healing potions to empty hotbar slots", "Boş hızlı erişim yuvalarına can iksiri taşır" },
            { "mouse tweaks", "Move items easily by dragging", "Eşyaları sürükleyerek taşımayı kolaylaştırır" },
            { "watermark", "Show the client logo", "İstemcinin logosunu gösterir" },
            { "arraylist", "List your enabled modules", "Açık modülleri ekranda listeler" },
            { "glow esp", "Highlight players with a glow", "Oyuncuların çevresini parlatır" },
            { "chams", "Color player models", "Oyuncuların görünümünü renklendirir" },
            { "china hat", "Wear a cone hat in third person", "Üçüncü şahısta karakterine koni şapka ekler" },
            { "wings", "Wear animated wings in third person", "Üçüncü şahısta karakterine hareketli kanatlar ekler" },
            { "cape", "Wear a cape that sways as you move", "Hareket ettikçe dalgalanan bir pelerin ekler" },
            { "sky", "Change the sky and horizon colors", "Gökyüzünün ve ufkun renklerini değiştirir" },
            { "trails", "Leave particles and footprints as you move", "Hareket ederken parçacık ve ayak izi bırakır" },
            { "player esp", "Mark players with boxes and labels", "Oyuncuları kutu ve etiketlerle gösterir" },
            { "item esp", "Mark items on the ground", "Yerdeki eşyaları işaretler" },
            { "storage esp", "Mark chests and other storage blocks", "Sandık ve diğer depoları işaretler" },
            { "block esp", "Mark the blocks you choose", "Seçtiğin blokları işaretler" },
            { "radar", "Show nearby players on a radar", "Yakındaki oyuncuları radarda gösterir" },
            { "full bright", "Make dark areas brighter", "Karanlık yerleri aydınlatır" },
            { "nametag", "Show more details above players", "Oyuncuların üstünde ek bilgi gösterir" },
            { "trajectories", "Show where thrown items will go", "Atacağın eşyanın gideceği yolu gösterir" },
            { "spotify hud", "Show your Spotify song and lyrics", "Spotify şarkını ve sözlerini gösterir" },
            { "target hud", "Show your target's health and equipment", "Hedefinin canını ve eşyalarını gösterir" },
            { "vape aim assist", "Help you aim at a target", "Hedefe nişan almanı kolaylaştırır" },
            { "vape left clicker", "Left-click automatically", "Otomatik sol tıklar" },
            { "vape right clicker", "Right-click automatically", "Otomatik sağ tıklar" },
        };
        for (const auto& description : descriptions)
            if (std::strcmp(description.name, name) == 0)
                return gui::text(description.english, description.turkish);
        return gui::text("Client module", "İstemci modülü");
    }
}
