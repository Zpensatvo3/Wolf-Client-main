#include "../src/modules/combat/crystal_aura_engine.hpp"
#include <cassert>
#include <iostream>
#include <limits>
#include <vector>

int main()
{
    using namespace modules::crystal_aura_logic;
    using namespace std::chrono_literals;
    std::vector<candidate> players{{1,4,true},{2,1,false},{3,9,true},{4,INFINITY,true},{5,-1,true}};
    assert(choose_target(players,-1) == 1); // Friend/bot/dead/spectator entries are ineligible.
    assert(choose_target(players,3) == 3); // Do not switch away from a live locked target.
    players[2].eligible = false;
    assert(choose_target(players,3) == 1); // Target died/was removed/added as friend.
    players[0].eligible = false;
    assert(choose_target(players,1) == -1);
    assert(choose_target({},-1) == -1);

    target_state player{target_kind::player,false,true,false,false,false};
    target_state golem{target_kind::mob,false,true,false,false,false};
    target_state armor_stand{target_kind::other,false,true,false,false,false};
    for (int mode = -1; mode <= 3; ++mode)
    {
        assert(target_allowed(mode,player) == (mode == 0 || mode == 1));
        assert(target_allowed(mode,golem) == (mode == 1 || mode == 2));
        assert(!target_allowed(mode,armor_stand)); // Also excludes crystals, dropped items and projectiles.
        for (const auto flag : {&target_state::self,&target_state::spectator})
        {
            auto blocked = golem; blocked.*flag = true;
            assert(!target_allowed(mode,blocked));
            blocked = player; blocked.*flag = true;
            assert(!target_allowed(mode,blocked));
        }
    }
    for (const auto flag : {&target_state::friendly,&target_state::bot})
    {
        auto blocked = player; blocked.*flag = true;
        assert(!target_allowed(0,blocked) && !target_allowed(1,blocked));
        auto mob = golem; mob.*flag = true;
        assert(target_allowed(1,mob) && target_allowed(2,mob)); // Player-only filters never classify mobs as fake players.
    }
    auto dead = golem; dead.alive = false;
    assert(!target_allowed(1,dead) && !target_allowed(2,dead));
    std::vector<candidate> mixed{{10,9,target_allowed(2,player)},{20,4,target_allowed(2,golem)},
        {30,1,target_allowed(2,dead)}};
    assert(choose_target(mixed,-1) == 20);
    mixed[1].eligible = false;
    mixed.push_back({40,16,target_allowed(2,golem)});
    assert(choose_target(mixed,20) == 40); // Dead golem hands off to the next mob, not a nearby player.

    const cell base{-4,64,8};
    assert(valid(base_volume(base)) && valid(crystal_volume(base)));
    assert(crystal_volume(base).min.y == 65 && crystal_volume(base).max.y == 67);
    assert(intersects(crystal_volume(base),{{-4,65,8},{-3,66.8,9}}));
    assert(!intersects(crystal_volume(base),{{-3,65,8},{-2,66.8,9}})); // Touching edges are not overlap.
    assert(!valid({{0,0,0},{0,1,1}}));
    const auto q = closest({10,2,-1},{{0,0,0},{1,3,1}});
    assert(q.x == 1 && q.y == 2 && q.z == 0);

    module_settings::crystal_aura config;
    engine engine;
    auto now = clock::time_point{} + 10s;
    observation seen;
    seen.valid = seen.materials = seen.can_melee = true;
    seen.target_id = 7; seen.session = 1;
    auto sword = engine.next(seen,now,config);
    assert(sword && sword->kind == action_kind::sword && sword->target_id == 7);
    assert(!engine.next(seen,now+1ms,config)); // No second submission before commit.
    assert(engine.commit(*sword,false,now,config));
    assert(engine.state() == phase::melee);
    assert(!engine.next(seen,now+79ms,config));
    now += 80ms;
    sword = engine.next(seen,now,config);
    assert(sword && engine.commit(*sword,true,now,config));
    assert(engine.state() == phase::launch);
    assert(!engine.next(seen,now+100ms,config)); // No crystal phase before observed airborne state.
    seen.airborne = true;
    assert(!engine.next(seen,now+110ms,config));
    assert(engine.state() == phase::base);
    seen.site_available = seen.space_clear = true;
    now += 120ms;
    auto place_base = engine.next(seen,now,config);
    assert(place_base && place_base->kind == action_kind::place_obsidian);
    assert(engine.commit(*place_base,true,now,config));
    assert(engine.state() == phase::wait_base);
    for (int frame = 1; frame <= 10; ++frame) assert(!engine.next(seen,now+std::chrono::milliseconds(frame),config));
    assert(engine.state() == phase::wait_base); // Packet submission is not proof of placement.
    seen.base_ready = true;
    assert(!engine.next(seen,now+20ms,config));
    assert(engine.state() == phase::crystal);
    now += 80ms;
    auto place = engine.next(seen,now,config);
    assert(place && place->kind == action_kind::place_crystal);
    assert(engine.commit(*place,true,now,config));
    assert(engine.state() == phase::wait_crystal);
    assert(!engine.next(seen,now+100ms,config));
    seen.crystal_id = 99; seen.can_break = true;
    assert(!engine.next(seen,now+110ms,config));
    assert(engine.state() == phase::break_crystal && engine.crystal() == 99);
    now += 120ms;
    auto attack = engine.next(seen,now,config);
    assert(attack && attack->kind == action_kind::break_crystal && attack->crystal_id == 99);
    assert(engine.commit(*attack,true,now,config));
    assert(engine.state() == phase::wait_break);
    for (int frame = 1; frame <= 20; ++frame) assert(!engine.next(seen,now+std::chrono::milliseconds(frame),config));
    seen.crystal_id = -1;
    assert(!engine.next(seen,now+30ms,config));
    assert(engine.state() == phase::melee); // Resume sword, not an unbounded crystal packet burst.

    // Every wait has a bounded timeout and returns to melee with backoff.
    engine.reset(); now += 5s; seen.crystal_id = -1; seen.base_ready = false; seen.airborne = false;
    sword = engine.next(seen,now,config);
    assert(sword && engine.commit(*sword,true,now,config));
    assert(!engine.next(seen,now+900ms,config));
    assert(engine.state() == phase::melee && !engine.next(seen,now+979ms,config));
    now += 980ms;
    sword = engine.next(seen,now,config);
    assert(sword && engine.commit(*sword,true,now,config));
    seen.airborne = true;
    assert(!engine.next(seen,now+1ms,config));
    now += 80ms;
    place_base = engine.next(seen,now,config);
    assert(place_base && engine.commit(*place_base,true,now,config));
    assert(!engine.next(seen,now+1200ms,config));
    assert(engine.state() == phase::melee);

    // Stale queued actions cannot execute against a replacement target/session.
    engine.reset(); now += 5s;
    auto stale = engine.next(seen,now,config);
    assert(stale);
    seen.target_id = 8;
    auto replacement = engine.next(seen,now+1ms,config);
    assert(replacement && replacement->target_id == 8);
    assert(!engine.commit(*stale,true,now+2ms,config));
    seen.session = 2;
    auto rejoined = engine.next(seen,now+3ms,config);
    assert(rejoined && rejoined->serial != replacement->serial);
    assert(!engine.commit(*replacement,true,now+4ms,config));
    engine.reset();
    assert(!engine.commit(*rejoined,true,now+5ms,config));
    seen.valid = false;
    assert(!engine.next(seen,now+6ms,config) && engine.target() == -1);
    seen.valid = true; seen.materials = false;
    assert(!engine.next(seen,now+7ms,config));
    seen.materials = true; seen.can_melee = false;
    assert(!engine.next(seen,now+8ms,config)); // Cooldown/range/visibility blocked.
    assert(engine.target() == 8);

    // Existing obsidian skips base placement; every later wait is bounded too.
    for (int timeout_stage = 0; timeout_stage < 4; ++timeout_stage)
    {
        engine.reset(); now += 10s;
        seen = {}; seen.valid = seen.materials = seen.can_melee = true;
        seen.session = 3; seen.target_id = 8;
        sword = engine.next(seen,now,config);
        assert(sword && engine.commit(*sword,true,now,config));
        seen.airborne = true;
        assert(!engine.next(seen,now+1ms,config));
        if (timeout_stage == 0)
        {
            // No support/space/visible face found: retry sword, not air-place.
            assert(!engine.next(seen,now+1201ms,config) && engine.state() == phase::melee);
            continue;
        }
        seen.base_ready = seen.site_available = seen.space_clear = true;
        assert(!engine.next(seen,now+2ms,config));
        assert(engine.state() == phase::crystal);
        now += 100ms;
        place = engine.next(seen,now,config);
        assert(place && place->kind == action_kind::place_crystal && engine.commit(*place,true,now,config));
        if (timeout_stage == 1)
        {
            assert(!engine.next(seen,now+1200ms,config) && engine.state() == phase::melee);
            continue;
        }
        seen.crystal_id = 88;
        assert(!engine.next(seen,now+1ms,config));
        if (timeout_stage == 2)
        {
            // Crystal is behind a block/outside normal entity reach.
            seen.can_break = false;
            assert(!engine.next(seen,now+80ms,config));
            assert(!engine.next(seen,now+1201ms,config) && engine.state() == phase::melee);
            continue;
        }
        seen.can_break = true;
        attack = engine.next(seen,now+80ms,config);
        assert(attack && engine.commit(*attack,true,now+80ms,config));
        assert(engine.finishing_crystal()); // Can finish even after the last crystal item is consumed.
        assert(!engine.next(seen,now+1280ms,config) && engine.state() == phase::melee);
        assert(!engine.finishing_crystal());
    }
    for (bool mob : {false,true})
        for (bool ground_test : {false,true})
        {
            engine.reset(); now += 10s;
            seen = {}; seen.valid = seen.materials = seen.can_melee = true;
            seen.session = 4; seen.target_id = 20; seen.target_is_mob = mob;
            config.mob_ground_test = ground_test;
            sword = engine.next(seen,now,config);
            assert(sword && engine.commit(*sword,true,now,config));
            assert(engine.state() == phase::launch);
            assert(!engine.next(seen,now+1ms,config));
            if (!mob || !ground_test)
            {
                assert(engine.state() == phase::launch); // Players still require actual airborne state.
                seen.airborne = true;
                assert(!engine.next(seen,now+2ms,config));
                assert(engine.state() == phase::base);
                continue;
            }
            assert(engine.state() == phase::base && !seen.airborne); // Knockback-resistant golem.
            seen.site_available = seen.space_clear = true;
            place_base = engine.next(seen,now+80ms,config);
            assert(place_base && engine.commit(*place_base,true,now+80ms,config));
            seen.base_ready = true;
            assert(!engine.next(seen,now+90ms,config));
            place = engine.next(seen,now+160ms,config);
            assert(place && engine.commit(*place,true,now+160ms,config));
            seen.crystal_id = 90; seen.can_break = true;
            assert(!engine.next(seen,now+170ms,config));
            attack = engine.next(seen,now+240ms,config);
            assert(attack && attack->kind == action_kind::break_crystal && engine.commit(*attack,true,now+240ms,config));
            seen.crystal_id = -1;
            assert(!engine.next(seen,now+250ms,config) && engine.state() == phase::melee);
        }
    std::cout << "Crystal Aura: player/mob modes, golem ground tests, target handoff, action flow and cancellation passed\n";
}
