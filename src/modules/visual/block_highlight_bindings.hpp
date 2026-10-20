#pragma once
#include "../../mappings.hpp"
namespace block_highlight_bindings
{
    BEGIN_KLASS_DEF(List, "java/util/List")
        jni::method<jint, "size"> size{*this};
        jni::method<maps::Object, "get", jni::NOT_STATIC, jint> get{*this};
    END_KLASS_DEF()
#if ONESTEP_MINECRAFT_26_1
    BEGIN_KLASS_DEF(BlockView, "net/minecraft/world/level/BlockGetter") END_KLASS_DEF()
    BEGIN_KLASS_DEF(Shape, "net/minecraft/world/phys/shapes/VoxelShape")
        jni::method<List, "toAabbs"> boxes{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(State, "net/minecraft/world/level/block/state/BlockState")
        jni::method<Shape, "getShape", jni::NOT_STATIC, BlockView, maps::BlockPos> outline{*this};
    END_KLASS_DEF()
#else
    BEGIN_KLASS_DEF(BlockView, "net/minecraft/class_1922") END_KLASS_DEF()
    BEGIN_KLASS_DEF(Shape, "net/minecraft/class_265")
        jni::method<List, "method_1090"> boxes{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(State, "net/minecraft/class_2680")
        jni::method<Shape, "method_26218", jni::NOT_STATIC, BlockView, maps::BlockPos> outline{*this};
    END_KLASS_DEF()
#endif
}
