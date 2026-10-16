#pragma once
#include "client_entities.hpp"

// Read-only model/atlas access. Yarn 1.21.1+build.3: no Java render calls or GL state changes.
namespace maps
{
    BEGIN_KLASS_DEF(ModelVector3f, "org/joml/Vector3f")
        jni::field<jfloat, "x"> x{*this};
        jni::field<jfloat, "y"> y{*this};
        jni::field<jfloat, "z"> z{*this};
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(ItemTransformation, "net/minecraft/class_804")
        jni::field<ModelVector3f, "field_4287"> rotation{*this};
        jni::field<ModelVector3f, "field_4286"> translation{*this};
        jni::field<ModelVector3f, "field_4285"> scale{*this};
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(ItemModelTransformation, "net/minecraft/class_809")
        jni::field<ItemTransformation, "field_4300"> gui{*this};
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(ModelRandom, "net/minecraft/class_5819")
        jni::method<ModelRandom, "method_43049", jni::STATIC, jlong> createSeeded{*this};
        jni::method<void, "method_43052", jni::NOT_STATIC, jlong> setSeed{*this};
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(ItemSprite, "net/minecraft/class_1058")
        jni::method<Identifier, "method_45852"> getAtlasId{*this};
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(ItemBakedQuad, "net/minecraft/class_777")
        jni::method<jni::array<jint>, "method_3357"> getVertexData{*this};
        jni::method<jint, "method_3359"> getColorIndex{*this};
        jni::method<ItemSprite, "method_35788"> getSprite{*this};
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(ItemBakedModel, "net/minecraft/class_1087")
        jni::method<List, "method_4707", jni::NOT_STATIC, BlockState, Direction, ModelRandom> getQuads{*this};
        jni::method<ItemModelTransformation, "method_4709"> getTransformation{*this};
        jni::method<jboolean, "method_24304"> isSideLit{*this};
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(ItemColors, "net/minecraft/class_325")
        jni::method<jint, "method_1704", jni::NOT_STATIC, ItemStack, jint> getColor{*this};
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(ItemModels, "net/minecraft/class_763")
        jni::method<ItemBakedModel, "method_3308", jni::NOT_STATIC, ItemStack> getModel{*this};
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(ItemRenderer, "net/minecraft/class_918")
        jni::method<ItemBakedModel, "method_4019", jni::NOT_STATIC, ItemStack, World, LivingEntity, jint> getModel{*this};
        jni::method<ItemModels, "method_4012"> getModels{*this};
        jni::field<ItemColors, "field_4733"> colors{*this};
    END_KLASS_DEF()
}
