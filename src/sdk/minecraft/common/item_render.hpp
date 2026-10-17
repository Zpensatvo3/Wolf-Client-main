#pragma once
// Shared SDK layout; target-specific JNI differences are selected at compile time.
#include "client_entities.hpp"
#include <bit>

// Read-only model/atlas access. Yarn 1.21.1+build.3: no Java render calls or GL state changes.
namespace maps
{
    BEGIN_KLASS_DEF(ModelVector3f, "org/joml/Vector3f")
        jni::field<jfloat, "x"> x{*this};
        jni::field<jfloat, "y"> y{*this};
        jni::field<jfloat, "z"> z{*this};
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(ModelVector3fc, "org/joml/Vector3fc")
        jni::method<jfloat, "x"> x{*this};
        jni::method<jfloat, "y"> y{*this};
        jni::method<jfloat, "z"> z{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(ItemTransformation, ONESTEP_MC_NAME("net/minecraft/class_804"))
#if ONESTEP_MINECRAFT_VERSION >= 12108
        jni::method<ModelVector3fc, ONESTEP_MC_NAME("comp_3747")> rotation{*this};
        jni::method<ModelVector3fc, ONESTEP_MC_NAME("comp_3748")> translation{*this};
        jni::method<ModelVector3fc, ONESTEP_MC_NAME("comp_3749")> scale{*this};
#else
        jni::field<ModelVector3f, ONESTEP_MC_NAME("field_4287")> rotation{*this};
        jni::field<ModelVector3f, ONESTEP_MC_NAME("field_4286")> translation{*this};
        jni::field<ModelVector3f, ONESTEP_MC_NAME("field_4285")> scale{*this};
#endif
        std::array<float, 3> rotation_components() const;
        std::array<float, 3> translation_components() const;
        std::array<float, 3> scale_components() const;
    END_KLASS_DEF()

    inline std::array<float, 3> ItemTransformation_members::rotation_components() const
    {
#if ONESTEP_MINECRAFT_VERSION >= 12108
        auto v = rotation.call(); return v ? std::array<float, 3>{v.x.call(), v.y.call(), v.z.call()} : std::array<float, 3>{};
#else
        auto v = rotation.get(); return v ? std::array<float, 3>{v.x.get(), v.y.get(), v.z.get()} : std::array<float, 3>{};
#endif
    }
    inline std::array<float, 3> ItemTransformation_members::translation_components() const
    {
#if ONESTEP_MINECRAFT_VERSION >= 12108
        auto v = translation.call(); return v ? std::array<float, 3>{v.x.call(), v.y.call(), v.z.call()} : std::array<float, 3>{};
#else
        auto v = translation.get(); return v ? std::array<float, 3>{v.x.get(), v.y.get(), v.z.get()} : std::array<float, 3>{};
#endif
    }
    inline std::array<float, 3> ItemTransformation_members::scale_components() const
    {
#if ONESTEP_MINECRAFT_VERSION >= 12108
        auto v = scale.call(); return v ? std::array<float, 3>{v.x.call(), v.y.call(), v.z.call()} : std::array<float, 3>{};
#else
        auto v = scale.get(); return v ? std::array<float, 3>{v.x.get(), v.y.get(), v.z.get()} : std::array<float, 3>{};
#endif
    }

    BEGIN_KLASS_DEF(ItemModelTransformation, ONESTEP_MC_NAME("net/minecraft/class_809"))
#if ONESTEP_MINECRAFT_VERSION == 12101
        jni::field<ItemTransformation, ONESTEP_MC_NAME("field_4300")> gui{*this};
#endif
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(ModelRandom, ONESTEP_MC_NAME("net/minecraft/class_5819"))
        jni::method<ModelRandom, ONESTEP_MC_NAME("method_43049"), jni::STATIC, jlong> createSeeded{*this};
        jni::method<void, ONESTEP_MC_NAME("method_43052"), jni::NOT_STATIC, jlong> setSeed{*this};
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(ItemSprite, ONESTEP_MC_NAME("net/minecraft/class_1058"))
        jni::method<Identifier, ONESTEP_MC_NAME("method_45852")> getAtlasId{*this};
    END_KLASS_DEF()

#if ONESTEP_MINECRAFT_VERSION == 26100
    BEGIN_KLASS_DEF(QuadMaterialInfo, "net/minecraft/client/resources/model/geometry/BakedQuad$MaterialInfo")
        jni::method<jint, "tintIndex"> getColorIndex{*this};
        jni::method<ItemSprite, "sprite"> getSprite{*this};
    END_KLASS_DEF()
#endif
    BEGIN_KLASS_DEF(ItemBakedQuad, ONESTEP_MC_NAME("net/minecraft/class_777"))
#if ONESTEP_MINECRAFT_VERSION >= 12111
        jni::method<ModelVector3fc, ONESTEP_MC_NAME("method_76648"), jni::NOT_STATIC, jint> getPosition{*this};
        jni::method<jlong, ONESTEP_MC_NAME("method_76649"), jni::NOT_STATIC, jint> getTexcoords{*this};
#elif ONESTEP_MINECRAFT_VERSION >= 12108
        jni::method<jni::array<jint>, ONESTEP_MC_NAME("comp_3721")> getVertexData{*this};
#else
        jni::method<jni::array<jint>, ONESTEP_MC_NAME("method_3357")> getVertexData{*this};
#endif
#if ONESTEP_MINECRAFT_VERSION == 26100
        jni::method<QuadMaterialInfo, "materialInfo"> getMaterial{*this};
#elif ONESTEP_MINECRAFT_VERSION >= 12108
        jni::method<jint, ONESTEP_MC_NAME("comp_3722")> getColorIndex{*this};
        jni::method<ItemSprite, ONESTEP_MC_NAME("comp_3724")> getSprite{*this};
#else
        jni::method<jint, ONESTEP_MC_NAME("method_3359")> getColorIndex{*this};
        jni::method<ItemSprite, ONESTEP_MC_NAME("method_35788")> getSprite{*this};
#endif
        std::vector<jint> vertex_data() const;
    END_KLASS_DEF()

#if ONESTEP_MINECRAFT_VERSION >= 12111
    BEGIN_KLASS_DEF(PackedUv, ONESTEP_MC_NAME("net/minecraft/class_5611"))
        jni::method<jfloat, ONESTEP_MC_NAME("method_76641"), jni::STATIC, jlong> getX{*this};
        jni::method<jfloat, ONESTEP_MC_NAME("method_76642"), jni::STATIC, jlong> getY{*this};
    END_KLASS_DEF()
#endif
    inline std::vector<jint> ItemBakedQuad_members::vertex_data() const
    {
#if ONESTEP_MINECRAFT_VERSION >= 12111
        std::vector<jint> result(32);
        PackedUv uv;
        for (int index = 0; index < 4; ++index)
        {
            auto position = getPosition.call(index);
            if (!position) return {};
            const jlong packed = getTexcoords.call(index);
            result[index * 8] = std::bit_cast<jint>(position.x.call());
            result[index * 8 + 1] = std::bit_cast<jint>(position.y.call());
            result[index * 8 + 2] = std::bit_cast<jint>(position.z.call());
            result[index * 8 + 4] = std::bit_cast<jint>(uv.getX.call(packed));
            result[index * 8 + 5] = std::bit_cast<jint>(uv.getY.call(packed));
        }
        return result;
#else
        auto packed = getVertexData.call();
        if (!packed || packed.get_length() < 32 || packed.get_length() > 256) return {};
        return packed.to_vector();
#endif
    }

    BEGIN_KLASS_DEF(ItemBakedModel, ONESTEP_MC_NAME("net/minecraft/class_1087"))
#if ONESTEP_MINECRAFT_VERSION <= 12104
        jni::method<List, ONESTEP_MC_NAME("method_4707"), jni::NOT_STATIC, BlockState, Direction, ModelRandom> getQuads{*this};
        jni::method<ItemModelTransformation, ONESTEP_MC_NAME("method_4709")> getTransformation{*this};
        jni::method<jboolean, ONESTEP_MC_NAME("method_24304")> isSideLit{*this};
#endif
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(ItemColors, ONESTEP_MC_NAME("net/minecraft/class_325"))
#if ONESTEP_MINECRAFT_VERSION == 12101
        jni::method<jint, ONESTEP_MC_NAME("method_1704"), jni::NOT_STATIC, ItemStack, jint> getColor{*this};
#endif
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(ItemModels, ONESTEP_MC_NAME("net/minecraft/class_763"))
#if ONESTEP_MINECRAFT_VERSION == 12101
        jni::method<ItemBakedModel, ONESTEP_MC_NAME("method_3308"), jni::NOT_STATIC, ItemStack> getModel{*this};
#endif
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(ItemRenderer, ONESTEP_MC_NAME("net/minecraft/class_918"))
#if ONESTEP_MINECRAFT_VERSION == 12101
        jni::method<ItemBakedModel, ONESTEP_MC_NAME("method_4019"), jni::NOT_STATIC, ItemStack, World, LivingEntity, jint> getModel{*this};
        jni::method<ItemModels, ONESTEP_MC_NAME("method_4012")> getModels{*this};
        jni::field<ItemColors, ONESTEP_MC_NAME("field_4733")> colors{*this};
#endif
    END_KLASS_DEF()

#if ONESTEP_MINECRAFT_VERSION >= 12104
    BEGIN_KLASS_DEF(ItemDisplayContext, ONESTEP_MC_NAME("net/minecraft/class_811"))
        jni::field<ItemDisplayContext, ONESTEP_MC_NAME("field_4317"), jni::STATIC> GUI{*this};
    END_KLASS_DEF()
#if ONESTEP_MINECRAFT_VERSION == 26100
    BEGIN_KLASS_DEF(ItemLocalTransform, "org/joml/Matrix4f")
        jni::method<jni::array<jfloat>, "get", jni::NOT_STATIC, jni::array<jfloat>> get{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(ItemTintList, "it/unimi/dsi/fastutil/ints/IntList")
        jni::method<jint, "size"> size{*this};
        jni::method<jint, "getInt", jni::NOT_STATIC, jint> getInt{*this};
    END_KLASS_DEF()
#endif
    BEGIN_KLASS_DEF(ItemRenderLayer, ONESTEP_MC_NAME("net/minecraft/class_10444$class_10446"))
#if ONESTEP_MINECRAFT_VERSION == 26100
        jni::field<ItemTintList, ONESTEP_MC_NAME("field_55349")> tints{*this};
        jni::field<ItemLocalTransform, "localTransform"> localTransform{*this};
#else
        jni::field<jni::array<jint>, ONESTEP_MC_NAME("field_55349")> tints{*this};
#endif
#if ONESTEP_MINECRAFT_VERSION >= 12108
        jni::field<ItemTransformation, ONESTEP_MC_NAME("field_56967")> transform{*this};
        jni::field<jboolean, ONESTEP_MC_NAME("field_56965")> useLight{*this};
        jni::method<List, ONESTEP_MC_NAME("method_67997")> getQuads{*this};
#else
        jni::field<ItemBakedModel, ONESTEP_MC_NAME("field_55346")> model{*this};
        jni::method<ItemTransformation, ONESTEP_MC_NAME("method_65619")> getTransformation{*this};
        jni::method<jboolean, ONESTEP_MC_NAME("method_65621")> isSideLit{*this};
#endif
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(ItemRenderState, ONESTEP_MC_NAME("net/minecraft/class_10444"))
        jni::constructor<> constructor{*this};
        jni::field<jint, ONESTEP_MC_NAME("field_55339")> layerCount{*this};
        jni::field<jni::array<ItemRenderLayer>, ONESTEP_MC_NAME("field_55340")> layers{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(ItemModelManager, ONESTEP_MC_NAME("net/minecraft/class_10442"))
#if ONESTEP_MINECRAFT_VERSION >= 12108
        jni::method<void, ONESTEP_MC_NAME("method_65597"), jni::NOT_STATIC, ItemRenderState, ItemStack, ItemDisplayContext, LivingEntity> update{*this};
#else
        jni::method<void, ONESTEP_MC_NAME("method_65597"), jni::NOT_STATIC, ItemRenderState, ItemStack, ItemDisplayContext, jboolean, LivingEntity> update{*this};
#endif
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(ItemModelClient, ONESTEP_MC_NAME("net/minecraft/class_310"))
        jni::method<ItemModelManager, ONESTEP_MC_NAME("method_65386")> getItemModelManager{*this};
    END_KLASS_DEF()
#endif
}
