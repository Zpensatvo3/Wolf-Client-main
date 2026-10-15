#pragma once
#include "../../mappings.hpp"

// Private Yarn 1.21.1 mappings for an independent model, not a render hook.
namespace chams_model
{
    BEGIN_KLASS_DEF(Cuboid, "net/minecraft/class_630$class_628")
        jni::field<jfloat, "field_3645"> minX{*this};
        jni::field<jfloat, "field_3644"> minY{*this};
        jni::field<jfloat, "field_3643"> minZ{*this};
        jni::field<jfloat, "field_3648"> maxX{*this};
        jni::field<jfloat, "field_3647"> maxY{*this};
        jni::field<jfloat, "field_3646"> maxZ{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Part, "net/minecraft/class_630")
        jni::method<Part, "method_32086", jni::NOT_STATIC, maps::String> getChild{*this};
        jni::field<maps::List, "field_3663"> cuboids{*this};
        jni::field<jfloat, "field_3657"> pivotX{*this};
        jni::field<jfloat, "field_3656"> pivotY{*this};
        jni::field<jfloat, "field_3655"> pivotZ{*this};
        jni::field<jfloat, "field_3654"> pitch{*this};
        jni::field<jfloat, "field_3675"> yaw{*this};
        jni::field<jfloat, "field_3674"> roll{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Layer, "net/minecraft/class_5601")
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Layers, "net/minecraft/class_5602")
        jni::field<Layer, "field_27577", jni::STATIC> PLAYER{*this};
        jni::field<Layer, "field_27581", jni::STATIC> PLAYER_SLIM{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Map, "java/util/Map")
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Loader, "net/minecraft/class_5599")
        jni::method<Part, "method_32072", jni::NOT_STATIC, Layer> getModelPart{*this};
        jni::field<Map, "field_27542"> modelParts{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Client, "net/minecraft/class_310")
        jni::method<Loader, "method_31974"> getEntityModelLoader{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Model, "net/minecraft/class_591")
        jni::constructor<Part, jboolean> constructor{*this};
        jni::method<void, "method_17087", jni::NOT_STATIC, maps::LivingEntity, jfloat, jfloat, jfloat, jfloat, jfloat> setAngles{*this};
        jni::method<void, "method_17086", jni::NOT_STATIC, maps::LivingEntity, jfloat, jfloat, jfloat> animateModel{*this};
        jni::field<jfloat, "field_3447"> handSwingProgress{*this};
        jni::field<jboolean, "field_3448"> child{*this};
        jni::field<jboolean, "field_3449"> riding{*this};
        jni::field<jboolean, "field_3400"> sneaking{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Limb, "net/minecraft/class_8080")
        jni::method<jfloat, "method_48572", jni::NOT_STATIC, jfloat> getPos{*this};
        jni::method<jfloat, "method_48570", jni::NOT_STATIC, jfloat> getSpeed{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Living, "net/minecraft/class_1309")
        jni::field<Limb, "field_42108"> limbAnimator{*this};
        jni::field<jfloat, "field_6283"> bodyYaw{*this};
        jni::field<jfloat, "field_6220"> prevBodyYaw{*this};
        jni::field<jfloat, "field_6241"> headYaw{*this};
        jni::field<jfloat, "field_6259"> prevHeadYaw{*this};
        jni::method<jfloat, "method_6055", jni::NOT_STATIC, jfloat> getHandSwingProgress{*this};
        jni::method<jboolean, "method_6057", jni::NOT_STATIC, maps::Entity> canSee{*this};
        jni::method<jboolean, "method_18276"> isInSneakingPose{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(SkinModel, "net/minecraft/class_8685$class_7920")
        jni::method<maps::String, "name"> name{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Skin, "net/minecraft/class_8685")
        jni::method<SkinModel, "comp_1629"> model{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(PlayerSkin, "net/minecraft/class_742")
        jni::method<Skin, "method_52814"> getSkinTextures{*this};
    END_KLASS_DEF()
}
