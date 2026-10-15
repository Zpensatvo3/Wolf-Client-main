#pragma once
#include "../../mappings.hpp"

// Fabric 1.21.1 bindings used by the retained Aim Assist adapter.
namespace vape_bindings
{
    BEGIN_KLASS_DEF_EX(Player, "net/minecraft/class_1657", maps::PlayerEntity)
        jni::method<jint, "method_6096"> getArmor{*this};
        jni::method<jboolean, "method_6057", jni::NOT_STATIC, maps::Entity> canSee{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF_EX(Manager, "net/minecraft/class_636", maps::ClientPlayerInteractionManager)
        jni::method<jboolean, "method_2923"> breaking{*this};
    END_KLASS_DEF()
}
