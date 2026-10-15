#pragma once
#include "java_types.hpp"

// Fabric 1.21.1 intermediary bindings; original member definitions preserved.
namespace maps
{
        BEGIN_KLASS_DEF(Vec3d, "net/minecraft/class_243")
        jni::constructor<jdouble, jdouble, jdouble> constructor{ *this };
        jni::field<jdouble, "field_1352"> x{ *this };
    jni::field<jdouble, "field_1351"> y{ *this };
    jni::field<jdouble, "field_1350"> z{ *this };

    maths::vector3d to_vector3d()
    {
        return { x.get(), y.get(), z.get() };
    }
    END_KLASS_DEF()

	KLASS_DECLARATION(BlockHitResult, "net/minecraft/class_3965")
	KLASS_DECLARATION(RaycastContext, "net/minecraft/class_3959")

	BEGIN_KLASS_DEF(RaycastContext$ShapeType, "net/minecraft/class_3959$class_3960")
		jni::field<RaycastContext$ShapeType, "field_17558", jni::STATIC> COLLIDER{ *this };
	END_KLASS_DEF()

	BEGIN_KLASS_DEF(RaycastContext$FluidHandling, "net/minecraft/class_3959$class_242")
		jni::field<RaycastContext$FluidHandling, "field_1348", jni::STATIC> NONE{ *this };
	END_KLASS_DEF()

}
