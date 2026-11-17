#include "attack_service.hpp"
#include "anti_bot_service.hpp"

#include "../../platform/jni/jni_context.hpp"
#include <cmath>

float services::attack::cooldown(const maps::ClientPlayerEntity& player, float tick_delta)
{
    float value = 0;
    return read_cooldown(player, value, tick_delta) ? value : 0.f;
}

bool services::attack::read_cooldown(const maps::ClientPlayerEntity& player, float& output, float tick_delta)
{
    output = 0;
    if (!player) return false;
    const float value = player.getAttackCooldownProgress.call(tick_delta);
    if (platform::jni_context::clear_exception() || !std::isfinite(value)) return false;
    output = value; return true;
}

bool services::attack::swing_main_hand(const maps::ClientPlayerEntity& player)
{

    if (!player)
        return false;
    maps::Hand hand = maps::Hand{}.MAIN_HAND.get();
    if (!hand || platform::jni_context::clear_exception())
        return false;
    player.swingHand.call(hand);
    return !platform::jni_context::clear_exception();
}

bool services::attack::attack_entity(const maps::ClientPlayerInteractionManager& manager,
    const maps::ClientPlayerEntity& player,
    const maps::Entity& target,
    bool swing)
{

    if (!manager || !player || !target)
        return false;
    if (services::anti_bot::is_bot(target)) return false;
    manager.attackEntity.call(player.toPlayerEntity(), target);
    if (platform::jni_context::clear_exception())
        return false;
    return !swing || swing_main_hand(player);
}
