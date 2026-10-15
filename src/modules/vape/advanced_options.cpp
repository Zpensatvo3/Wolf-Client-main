#include "advanced_module.hpp"
#include "../../java_hook/java_hook.hpp"
#include "../../gui/gui.hpp"
#include "../../gui/widgets.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace vape_advanced
{
    using T = control_type;
    std::span<const control> controls(kind k)
    {
        static const control velocity[]{
            {"horizontal","Horizontal","Yatay",T::number,90,0,200},{"vertical","Vertical","Dikey",T::number,100,0,200},
            {"chance","Chance","Şans",T::number,100,0,100},{"water","Liquid Check","Sıvı kontrolü",T::toggle,1,0,1},
            {"kite","Kite Mode","Uzaklaşma modu",T::toggle,0,0,1},
            {"kite_horizontal","Kite Horizontal","Uzaklaşma yatay",T::number,120,0,200},
            {"kite_vertical","Kite Vertical","Uzaklaşma dikey",T::number,100,0,200}};
        static const control knockback[]{
            {"chance","Chance","Şans",T::number,80,0,100},{"ground_min","Ground Min (ms)","Yerde alt (ms)",T::integer,180,0,1000},
            {"ground_max","Ground Max (ms)","Yerde üst (ms)",T::integer,250,0,1000},
            {"air_min","Air Min (ms)","Havada alt (ms)",T::integer,40,0,1000},
            {"air_max","Air Max (ms)","Havada üst (ms)",T::integer,80,0,1000},
            {"water","Liquid Check","Sıvı kontrolü",T::toggle,1,0,1}};
        static const control jump[]{
            {"chance","Chance","Şans",T::number,100,0,100},{"accuracy","Accuracy","İsabet",T::number,90,0,100},
            {"delay","Delay (ticks)","Gecikme (tik)",T::integer,0,0,10},
            {"water","Liquid Check","Sıvı kontrolü",T::toggle,1,0,1},
            {"minimum","Minimum Velocity","En düşük hız",T::number,.05,0,1}};
        static const control wtap[]{{"mode","Mode","Mod",T::choice,0,0,1,"Packet|Keyboard"}};
        static const control shield[]{
            {"delay","Swap Delay","Geçiş gecikmesi",T::integer,0,0,5},{"restore","Restore Delay","Dönüş gecikmesi",T::integer,1,0,5},
            {"double","Double Hit","Çift vuruş",T::toggle,1,0,1},{"range","Range","Menzil",T::number,4,1,6}};
        static const control mace[]{
            {"mode","Mace","Topuz",T::choice,2,0,2,"Density|Breach|Auto"},
            {"unequip","Unequip Elytra","Elytra çıkar",T::toggle,1,0,1},{"reequip","Re-equip Elytra","Elytra geri giy",T::toggle,1,0,1},
            {"stun","Stun Slam","Kalkan kombosu",T::toggle,1,0,1},{"chance","Stun Chance","Kombo şansı",T::number,100,0,100},
            {"smash_only","Smash Only","Sadece ezme",T::toggle,1,0,1},{"fall","Minimum Fall","En az düşüş",T::number,1.5,.1,10},
            {"range","Range","Menzil",T::number,6,1,6}};
        static const control swap[]{
            {"axes","Swap Axes","Balta değişimi",T::toggle,1,0,1},{"maces","Swap Maces","Topuz değişimi",T::toggle,1,0,1},
            {"stun","Stun Slam","Kalkan kombosu",T::toggle,1,0,1},{"restore","Restore Slot","Slotu geri al",T::toggle,1,0,1}};
        static const control select[]{
            {"mode","Mode","Mod",T::choice,1,0,1,"Passive|Active"},{"chance","Chance","Şans",T::number,90,0,100}};
        static const control block[]{
            {"mode","Mode","Mod",T::choice,1,0,3,"Manual|Predict|Auto|Lag"},
            {"mouse","On Click Only","Sadece tıklarken",T::toggle,1,0,1},
            {"range","Range","Menzil",T::number,3.8,1,6},{"fov","FOV","FOV",T::number,90,10,360},
            {"hurt","Prediction Window (ms)","Tahmin aralığı (ms)",T::number,50,0,250},
            {"ping","Include Ping","Ping dahil",T::toggle,1,0,1},{"hold","Hold (ticks)","Tutma (tik)",T::integer,2,1,10}};
        static const control reach[]{
            {"min","Min Range","Alt menzil",T::number,3,3,6},{"max","Max Range","Üst menzil",T::number,3.25,3,6},
            {"chance","Chance","Şans",T::number,100,0,100},{"sprint","Sprinting Only","Sadece sprintte",T::toggle,0,0,1},
            {"water","Liquid Check","Sıvı kontrolü",T::toggle,1,0,1},{"vertical","Vertical Check","Dikey kontrol",T::toggle,1,0,1},
            {"height","Max Height","En fazla yükseklik",T::number,2.2,.1,6},{"misplace","Misplace","Misplace",T::toggle,0,0,1}};
        static const control release[]{
            {"gui","Only in GUI","Sadece menüde",T::toggle,1,0,1},{"bows","Bows","Yaylar",T::toggle,1,0,1},
            {"shields","Shields","Kalkanlar",T::toggle,1,0,1},{"food","Food / Potions","Yiyecek / iksir",T::toggle,1,0,1}};
        static const control flick[]{
            {"angle","Max Angle","En fazla açı",T::number,75,1,180},{"range","Range","Menzil",T::number,3.6,1,6},
            {"chance","Chance","Şans",T::number,100,0,100},{"blink","Blink","Blink",T::toggle,1,0,1},
            {"select","Select Hits","Vuruş seçimi",T::toggle,1,0,1},{"random","Random Offset","Rastgele ofset",T::toggle,1,0,1},
            {"offset","Offset","Ofset",T::number,.15,0,.5},{"invert","Invert Strafe","Strafe tersle",T::toggle,0,0,1}};
        static const control aura[]{
            {"range","Range","Menzil",T::number,3.8,1,6},{"extra","Pre-Aim Range","Ön nişan mesafesi",T::number,1,0,3},
            {"fov","FOV","FOV",T::number,360,10,360},{"priority","Priority","Öncelik",T::choice,0,0,3,"Distance|Health|Armor|Threat"},
            {"area","Target Area","Hedef alanı",T::choice,0,0,3,"Closest|Center|Box|Ring"},
            {"speed","Aim Speed","Nişan hızı",T::number,7,1,10},{"cooldown","Cooldown","Dolum",T::number,.95,.5,1},
            {"perfect","Perfect Swing","Tam dolum",T::toggle,1,0,1},{"shield","Shield Check","Kalkan kontrolü",T::toggle,1,0,1},
            {"mouse","On Click Only","Sadece tıklarken",T::toggle,0,0,1},{"jitter","Human Jitter","Nişan titreşimi",T::toggle,1,0,1}};
        static const control crystal[]{
            {"place","Place Range","Yerleştirme menzili",T::number,4.5,1,6},{"break","Break Range","Kırma menzili",T::number,4.5,1,6},
            {"damage","Min Damage","En az hasar",T::number,6,0,30},{"self","Max Self Damage","En fazla öz hasar",T::number,4,0,20},
            {"suicide","Prevent Suicide","Ölümü önle",T::toggle,1,0,1},{"obsidian","Auto Obsidian","Otomatik obsidyen",T::toggle,1,0,1}};
        switch(k) {
        case kind::velocity:return velocity; case kind::knockback:return knockback; case kind::jump:return jump;
        case kind::wtap:return wtap; case kind::shield:return shield; case kind::mace:return mace;
        case kind::swap:return swap; case kind::select:return select; case kind::block:return block;
        case kind::reach:return reach; case kind::release:return release; case kind::flick:return flick;
        case kind::aura:return aura; case kind::crystal:return crystal; default:return {};
        }
    }
    const char* id(kind k)
    {
        static const char* ids[]{"vape_velocity","vape_knockback_delay","vape_jump_reset","vape_wtap",
            "vape_shield_breaker","vape_auto_mace","vape_hit_swap","vape_hit_select","vape_block_hit",
            "vape_reach","vape_no_item_release","vape_hit_flick","vape_silent_aura","vape_crystal_aura"};
        return ids[static_cast<int>(k)];
    }
    const char* name(kind k)
    {
        static const char* names[]{"vape velocity","vape knockback delay","vape jump reset","vape wtap",
            "vape shield breaker","vape auto mace","vape hit swap","vape hit select","vape block hit",
            "vape reach","vape no item release","vape hit flick","vape silent aura","vape crystal aura"};
        return names[static_cast<int>(k)];
    }
    double value(kind k,const configuration& config,const char* key)
    {
        const auto schema = controls(k);
        for (std::size_t i=0;i<schema.size();++i) if (std::strcmp(schema[i].key,key)==0) return config.values[i];
        return 0;
    }
}
modules::vape_advanced_module::vape_advanced_module(vape_advanced::kind k)
    :module(vape_advanced::name(k),"Vape",vape_advanced::id(k)),type(k)
{
    auto schema=vape_advanced::controls(k);
    for (std::size_t i=0;i<schema.size();++i) settings.values[i]=schema[i].initial;
}
modules::vape_advanced_module::~vape_advanced_module() { vape_advanced::retire(type); }
float modules::vape_advanced_module::get_options_height() { return 50.f+36.f*vape_advanced::controls(type).size(); }
void modules::vape_advanced_module::render_overlay()
{
    vape_advanced::configuration snapshot;
    { std::lock_guard lock(mutex); snapshot=settings; }
    vape_advanced::publish(type,enabled,snapshot,revision.load());
}
void modules::vape_advanced_module::render_options()
{
    using namespace vape_advanced;
    std::lock_guard lock(mutex);
    const auto before=settings;
    const auto schema=controls(type);
    for (std::size_t i=0;i<schema.size();++i) {
        const auto& item=schema[i]; auto& number=settings.values[i]; const auto* label=gui::text(item.en,item.tr);
        if (item.type==control_type::toggle) { bool v=number!=0; gui::option_toggle(label,&v); number=v?1:0; }
        else if (item.type==control_type::choice) {
            std::string choices=item.choices; std::vector<std::string> strings;
            std::size_t start=0;
            for (std::size_t end=0;end<=choices.size();++end) if (end==choices.size() || choices[end]=='|') {
                strings.push_back(choices.substr(start,end-start));start=end+1;
            }
            std::vector<const char*> pointers; for (const auto& entry:strings) pointers.push_back(entry.c_str());
            int v=static_cast<int>(number); gui::option_combo(label,&v,pointers.data(),static_cast<int>(pointers.size()));number=v;
        } else if (item.type==control_type::integer) {
            int v=static_cast<int>(number);gui::option_slider(label,&v,static_cast<int>(item.low),static_cast<int>(item.high),"%d");number=v;
        } else { float v=static_cast<float>(number);gui::option_slider(label,&v,static_cast<float>(item.low),static_cast<float>(item.high),"%.2f");number=v; }
    }
    if(type==kind::reach&&settings.values[0]>settings.values[1])std::swap(settings.values[0],settings.values[1]);
    if(type==kind::knockback) {
        if(settings.values[1]>settings.values[2])std::swap(settings.values[1],settings.values[2]);
        if(settings.values[3]>settings.values[4])std::swap(settings.values[3],settings.values[4]);
    }
    if (!(before==settings)) revision.fetch_add(1);
    if (enabled) {
        auto error=java_hook::vape_error();
        if(error.empty())error=runtime_error();
        if(!error.empty())ImGui::TextWrapped("%s",error.c_str());
    }
}
void modules::vape_advanced_module::visit_config(config_visitor& v)
{
    using namespace vape_advanced;
    std::lock_guard lock(mutex);
    auto candidate=settings;
    if (v.importing()) v.stage_values(true);
    const auto schema=controls(type);
    for (std::size_t i=0;i<schema.size();++i) {
        const auto& field=schema[i];auto& number=candidate.values[i];
        if (field.type==control_type::toggle) { bool b=number!=0;v.field(field.key,b);number=b?1:0; }
        else if (field.type==control_type::choice || field.type==control_type::integer) {
            int n=static_cast<int>(number);
            if (field.type==control_type::choice) v.bounded_enum(field.key,n,static_cast<int>(field.low),static_cast<int>(field.high));
            else v.field(field.key,n);
            number=n;
        } else v.field(field.key,number);
        if (v.importing() && !std::isfinite(number)) v.reject("Vape: non-finite settings.");
        if (std::isfinite(number)) number=std::clamp(number,field.low,field.high);
    }
    // Range normalization changes neither the timing formula nor random source.
    auto order=[&](const char* a,const char* b) {
        int first=-1,second=-1;
        for (int i=0;i<static_cast<int>(schema.size());++i) {
            if (std::strcmp(schema[i].key,a)==0) first=i;
            if (std::strcmp(schema[i].key,b)==0) second=i;
        }
        if(first>=0&&second>=0&&candidate.values[first]>candidate.values[second])
            std::swap(candidate.values[first],candidate.values[second]);
    };
    order("ground_min","ground_max");order("air_min","air_max");order("min","max");
    if(v.importing()) { v.stage_values(false);if(v.reading()&&v.valid()) {settings=candidate;revision.fetch_add(1);} }
}
#define VAPE_FACTORY(suffix,k) modules::module* modules::create_vape_##suffix(){return new vape_advanced_module(vape_advanced::kind::k);}
VAPE_FACTORY(velocity,velocity) VAPE_FACTORY(knockback,knockback) VAPE_FACTORY(jump,jump)
VAPE_FACTORY(wtap,wtap) VAPE_FACTORY(shield,shield) VAPE_FACTORY(mace,mace)
VAPE_FACTORY(swap,swap) VAPE_FACTORY(select,select) VAPE_FACTORY(block,block)
VAPE_FACTORY(reach,reach) VAPE_FACTORY(release,release) VAPE_FACTORY(flick,flick)
VAPE_FACTORY(aura,aura) VAPE_FACTORY(crystal,crystal)
#undef VAPE_FACTORY
