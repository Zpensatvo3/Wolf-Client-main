#pragma once
#include "../../mappings.hpp"

// Private, verified Yarn 1.21.1+build.3 bindings. No world generation or render hooks.
// https://maven.fabricmc.net/docs/yarn-1.21.1+build.3/net/minecraft/world/biome/BiomeEffects.html
namespace sky_bindings
{
    BEGIN_KLASS_DEF(Effects, "net/minecraft/class_4763")
        jni::field<jint,"field_26418"> skyColor{*this};
        jni::field<jint,"field_22067"> fogColor{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Biome, "net/minecraft/class_1959")
        jni::method<Effects,"method_24377"> getEffects{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Key, "net/minecraft/class_5321")
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Keys, "net/minecraft/class_7924")
        jni::field<Key,"field_41236",jni::STATIC> BIOME{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Registry, "net/minecraft/class_2378")
        jni::method<maps::Iterator,"iterator"> iterator{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Manager, "net/minecraft/class_5455")
        jni::method<Registry,"method_30530",jni::NOT_STATIC,Key> get{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Dimension, "net/minecraft/class_2874")
        jni::method<jboolean,"comp_642"> hasSkyLight{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Submersion, "net/minecraft/class_5636")
        jni::field<Submersion,"field_27888",jni::STATIC> NONE{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Camera, "net/minecraft/class_4184")
        jni::method<Submersion,"method_19334"> getSubmersionType{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(World, "net/minecraft/class_1937")
        jni::method<Manager,"method_30349"> getRegistryManager{*this};
        jni::method<Dimension,"method_8597"> getDimension{*this};
    END_KLASS_DEF()
}
