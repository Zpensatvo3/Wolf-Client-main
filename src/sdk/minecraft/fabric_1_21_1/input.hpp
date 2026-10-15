#pragma once
#include "client_entities.hpp"

// Fabric 1.21.1 intermediary bindings; original member definitions preserved.
namespace maps
{
        BEGIN_KLASS_DEF(SimpleOption, "net/minecraft/class_7172")
        jni::field<Object, "field_37868"> value{ *this };
    jni::method<Object, "method_41753"> getValue{ *this };
	jni::method<void, "method_41748", jni::NOT_STATIC, Object> setValue{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(InputKeyType, "net/minecraft/class_3675$class_307")
        jni::field<InputKeyType, "field_1668", jni::STATIC> KEYSYM{ *this };
        jni::field<InputKeyType, "field_1671", jni::STATIC> SCANCODE{ *this };
        jni::field<InputKeyType, "field_1672", jni::STATIC> MOUSE{ *this };
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(InputKey, "net/minecraft/class_3675$class_306")
        jni::method<jint, "method_1444"> getCode{ *this };
        jni::method<InputKeyType, "method_1442"> getCategory{ *this };
    END_KLASS_DEF()
        BEGIN_KLASS_DEF(KeyBinding, "net/minecraft/class_304")
    jni::field<InputKey, "field_1655"> boundKey{ *this };
        jni::method<jboolean, "method_1434"> isPressed{ *this };
    jni::method<void, "method_23481", jni::NOT_STATIC, jboolean> setPressed{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(GameOptions, "net/minecraft/class_315")
        jni::field<SimpleOption, "field_1843"> mouseSensitivity{ *this };
    jni::field<SimpleOption, "field_1840"> gammaSetting{ *this };
    jni::field<KeyBinding, "field_1886"> attackKey{ *this };
    jni::field<KeyBinding, "field_1904"> useKey{ *this };
    jni::field<KeyBinding, "field_1903"> jumpKey{ *this };
	jni::field<KeyBinding, "field_1894"> forwardKey{ *this };
	jni::field<KeyBinding, "field_1881"> backKey{ *this };
	jni::field<KeyBinding, "field_1913"> leftKey{ *this };
	jni::field<KeyBinding, "field_1849"> rightKey{ *this };
	jni::field<KeyBinding, "field_1832"> sneakKey{ *this };
    jni::field<SimpleOption, "field_1826"> fov{ *this };
    jni::method<SimpleOption, "method_42495"> getMouseSensitivity{ *this };
    jni::method<SimpleOption, "method_42473"> getGamma{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(Mouse, "net/minecraft/class_312")
    jni::method<void, "method_1601", jni::NOT_STATIC, jlong, jint, jint, jint> onMouseButton{ *this };
    jni::field<jboolean, "field_1791"> leftButtonClicked{ *this };
    jni::field<jboolean, "field_1788"> rightButtonClicked{ *this };
        jni::field<jdouble, "field_1789"> cursorDeltaX{ *this };
    jni::field<jdouble, "field_1787"> cursorDeltaY{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(RenderTickCounter, "net/minecraft/class_9779$class_9781")
        jni::field<jfloat, "field_51959"> tickDelta{ *this };
        jni::method<jfloat, "method_60644", jni::NOT_STATIC, jboolean> getTickDelta{ *this };

        float get_tick_delta()
        {
            JNIEnv* env = jni::get_env();
            if (getTickDelta.is_valid())
            {
                float d = getTickDelta.call(JNI_FALSE);
                bool exc = env && env->ExceptionCheck();
                if (exc) env->ExceptionClear();
                if (!exc && std::isfinite(d) && d >= 0.0f && d <= 2.0f)
                    return d;
            }
            if (tickDelta.is_valid())
            {
                float d = tickDelta.get();
                bool exc = env && env->ExceptionCheck();
                if (exc) env->ExceptionClear();
                if (!exc && std::isfinite(d) && d >= 0.0f && d <= 2.0f)
                    return d;
            }
            return 1.0f;
        }
    END_KLASS_DEF()

}
