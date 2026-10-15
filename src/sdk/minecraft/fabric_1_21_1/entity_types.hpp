#pragma once
#include "inventory_network.hpp"

// Fabric 1.21.1 intermediary bindings; original member definitions preserved.
namespace maps
{
        BEGIN_KLASS_DEF(Box, "net/minecraft/class_238")
        jni::field<jdouble, "field_1323"> minX{ *this };
    jni::field<jdouble, "field_1322"> minY{ *this };
    jni::field<jdouble, "field_1321"> minZ{ *this };
    jni::field<jdouble, "field_1320"> maxX{ *this };
    jni::field<jdouble, "field_1325"> maxY{ *this };
    jni::field<jdouble, "field_1324"> maxZ{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(EntityPose, "net/minecraft/class_4050")
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(EntityDimensions, "net/minecraft/class_4048")
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(StatusEffect, "net/minecraft/class_1291")
        END_KLASS_DEF()

	BEGIN_KLASS_DEF(StatusEffects, "net/minecraft/class_1294")
		jni::field<RegistryEntry, "field_5919", jni::STATIC> BLINDNESS{ *this };
		jni::field<StatusEffect, "field_5912", jni::STATIC> GLOWING{ *this };
    jni::field<StatusEffect, "field_5925", jni::STATIC> NIGHT_VISION{ *this };
    jni::field<StatusEffect, "field_5918", jni::STATIC> FIRE_RESISTANCE{ *this };
    jni::field<StatusEffect, "field_5924", jni::STATIC> REGENERATION{ *this };
    jni::field<StatusEffect, "field_5917", jni::STATIC> HASTE{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(StatusEffectInstance, "net/minecraft/class_1293")
        jni::constructor<StatusEffect, jint, jint> constructor{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(PlayerAbilities, "net/minecraft/class_1656")
        jni::field<jboolean, "field_7477"> creativeMode{ *this };
    jni::field<jboolean, "field_7478"> allowFlying{ *this };
    jni::field<jboolean, "field_7479"> flying{ *this };
    jni::field<jfloat, "field_7481"> flySpeed{ *this };
    jni::field<jfloat, "field_7482"> walkSpeed{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(OrderedText, "net/minecraft/class_5481")
        END_KLASS_DEF()

        BEGIN_KLASS_MEMBERS(Text)
        jni::method<Text, "method_30163", jni::STATIC, String> of{ *this };
        jni::method<String, "getString"> getString{ *this };
        jni::method<String, "method_54160"> getLiteralString{ *this };
    jni::method<OrderedText, "method_30937", jni::NOT_STATIC> asOrderedText{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(Hand, "net/minecraft/class_1268")
        jni::field<Hand, "field_5808", jni::STATIC> MAIN_HAND{ *this };
    jni::field<Hand, "field_5810", jni::STATIC> OFF_HAND{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(PlayerInteractItemC2SPacket, "net/minecraft/class_2886")
        jni::constructor<maps::Hand, jint> constructor{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(HitResult$Type, "net/minecraft/class_239$class_240")
        jni::field<HitResult$Type, "field_1331", jni::STATIC> ENTITY{ *this };
    jni::field<HitResult$Type, "field_1332", jni::STATIC> BLOCK{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(HitResult, "net/minecraft/class_239")
        jni::method<HitResult$Type, "method_17783"> getType{ *this };
    jni::method<Vec3d, "method_17784"> getPos{ *this };
    END_KLASS_DEF()

}
