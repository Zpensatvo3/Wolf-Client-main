#pragma once
#include "entities.hpp"

// Fabric 1.21.1 intermediary bindings; original member definitions preserved.
namespace maps
{
		BEGIN_KLASS_DEF(Input, "net/minecraft/class_744")
		jni::field<jboolean, "field_3904"> jumping{ *this };
	END_KLASS_DEF()

        BEGIN_KLASS_DEF(Identifier, "net/minecraft/class_2960")
        jni::method<String, "method_12832"> getPath{ *this };
    jni::method<String, "method_12836"> getNamespace{ *this };
    jni::method<String, "toString", jni::NOT_STATIC> toString{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(Texture, "net/minecraft/class_1044")
        jni::method<jint, "method_4624"> getGlId{ *this };
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(SkinTextures, "net/minecraft/class_8685")
        jni::method<Identifier, "comp_1626"> texture{ *this };
        END_KLASS_DEF()

	BEGIN_KLASS_DEF_EX(Registry, "net/minecraft/class_2378", Iterable)
		jni::method<Identifier, "method_10221", jni::NOT_STATIC, Object> getId{ *this };
	END_KLASS_DEF()

	BEGIN_KLASS_DEF_EX(DefaultedRegistry, "net/minecraft/class_7922", Registry)
	END_KLASS_DEF()

	BEGIN_KLASS_DEF(Registries, "net/minecraft/class_7923")
		jni::field<DefaultedRegistry, "field_41175", jni::STATIC> BLOCK{ *this };
	END_KLASS_DEF()

        BEGIN_KLASS_DEF(TextureManager, "net/minecraft/class_1060")
        jni::method<Texture, "method_4619", jni::NOT_STATIC, Identifier> getTexture{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF_EX(AbstractClientPlayerEntity, "net/minecraft/class_742", PlayerEntity)
        jni::method<SkinTextures, "method_52814"> getSkinTextures{ *this };
    END_KLASS_DEF()

		BEGIN_KLASS_DEF_EX(ClientPlayerEntity, "net/minecraft/class_746", AbstractClientPlayerEntity)
		jni::field<Input, "field_3913"> input{ *this };
		/* ClientPlayerEntity.canStartSprinting in Yarn/Fabric 1.21.1. */
		jni::method<jboolean, "method_48300", jni::NOT_STATIC> canStartSprinting{ *this };
		maps::PlayerEntity toPlayerEntity() const
    {
        return maps::PlayerEntity(this->object_instance, this->is_global());
    }
    END_KLASS_DEF()

        BEGIN_KLASS_DEF_EX(MobEntity, "net/minecraft/class_1308", LivingEntity)
        END_KLASS_DEF()

		BEGIN_KLASS_DEF_EX(ZombieEntity, "net/minecraft/class_1642", MobEntity)
		END_KLASS_DEF()

        BEGIN_KLASS_DEF_EX(AnimalEntity, "net/minecraft/class_1429", MobEntity)
        END_KLASS_DEF()

        BEGIN_KLASS_DEF_EX(ClientWorld, "net/minecraft/class_638", World)
        jni::field<List, "field_18226"> players{ *this };
    jni::method<List, "method_18456"> getPlayers{ *this };
    jni::field<WorldProperties, "field_24430"> clientWorldProperties{ *this };
    jni::method<Iterable, "method_18112"> getEntities{ *this };
    END_KLASS_DEF()

		BEGIN_KLASS_DEF_EX(OtherClientPlayerEntity, "net/minecraft/class_745", AbstractClientPlayerEntity)
		jni::constructor<ClientWorld, GameProfile> constructor{ *this };
		END_KLASS_DEF()

}
