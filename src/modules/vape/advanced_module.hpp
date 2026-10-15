#pragma once
#include "../module_support.hpp"
#include "../../mappings.hpp"
#include <array>
#include <span>

namespace vape_advanced
{
    enum class kind { velocity, knockback, jump, wtap, shield, mace, swap, select, block,
        reach, release, flick, aura, crystal, count };
    enum class control_type { toggle, number, integer, choice };
    struct control {
        const char* key; const char* en; const char* tr; control_type type;
        double initial, low, high; const char* choices = nullptr;
    };
    struct configuration { std::array<double,24> values{}; bool operator==(const configuration&) const = default; };
    std::span<const control> controls(kind);
    const char* id(kind);
    const char* name(kind);
    double value(kind, const configuration&, const char*);
    void publish(kind, bool, const configuration&, std::uint64_t);
    void retire(kind);
    bool owns_attack();
    bool reach_active();
    bool confirm_reach_target(const maps::Entity&);
    std::string runtime_error();
}
namespace modules
{
    class vape_advanced_module : public module
    {
    public:
        explicit vape_advanced_module(vape_advanced::kind);
        ~vape_advanced_module() override;
        void run(::cache&) override {}
        void on_enable(::cache&) override { revision.fetch_add(1); }
        void on_disable(::cache&) override { revision.fetch_add(1); }
        void render_overlay() override;
        bool render_when_disabled() const override { return true; }
        void render_options() override;
        float get_options_height() override;
        void visit_config(config_visitor&) override;
    private:
        vape_advanced::kind type;
        vape_advanced::configuration settings;
        std::mutex mutex;
        std::atomic<std::uint64_t> revision{1};
    };
    module* create_vape_velocity(); module* create_vape_knockback(); module* create_vape_jump();
    module* create_vape_wtap(); module* create_vape_shield(); module* create_vape_mace();
    module* create_vape_swap(); module* create_vape_select(); module* create_vape_block();
    module* create_vape_reach(); module* create_vape_release(); module* create_vape_flick();
    module* create_vape_aura(); module* create_vape_crystal();
}
