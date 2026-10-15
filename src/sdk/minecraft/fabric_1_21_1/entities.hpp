#pragma once
#include "entity_types.hpp"

// Fabric 1.21.1 intermediary bindings; original member definitions preserved.
namespace maps
{
		BEGIN_KLASS_DEF(Entity, "net/minecraft/class_1297")
		jni::method<jfloat, "method_5739", jni::NOT_STATIC, Entity> distanceTo{ *this };
		jni::method<jint, "method_5628"> getId{ *this };
        jni::method<World, "method_37908"> getWorld{ *this };
    jni::field<jint, "field_6012"> age{ *this };
    jni::field<Vec3d, "field_22467"> pos{ *this };
    jni::field<jfloat, "field_6031"> yaw{ *this };
    jni::field<jfloat, "field_5965"> pitch{ *this };
    jni::field<jfloat, "field_5982"> prevYaw{ *this };
    jni::field<jfloat, "field_6004"> prevPitch{ *this };
    jni::field<Box, "field_6005"> boundingBox{ *this };
    jni::method<Box, "method_5829"> getBoundingBox{ *this };
    jni::method<Vec3d, "method_19538"> getPos{ *this };
    jni::method<jdouble, "method_23320"> getEyeY{ *this };
    jni::method<jfloat, "method_5751"> getStandingEyeHeight{ *this };
    jni::field<jfloat, "field_6017"> fallDistance{ *this };
    jni::field<jboolean, "field_5952"> onGround{ *this };
    jni::method<jboolean, "method_24828"> isOnGround{ *this };
    jni::method<jboolean, "method_5809"> isOnFire{ *this };
    jni::method<jboolean, "method_5799"> isTouchingWater{ *this };
    jni::method<jboolean, "method_5771"> isInLava{ *this };
    jni::method<jboolean, "method_5757"> isInsideWall{ *this };
    jni::field<jdouble, "field_6038"> lastRenderX{ *this };
    jni::field<jdouble, "field_5971"> lastRenderY{ *this };
    jni::field<jdouble, "field_5989"> lastRenderZ{ *this };
    jni::method<jboolean, "method_5767"> isInvisible{ *this };

    maths::vector3d get_prev_position() const
    {
        return { lastRenderX.get(), lastRenderY.get(), lastRenderZ.get() };
    }

    maps::Box get_bounding_box() const
    {
        if (boundingBox.is_valid())
        {
            maps::Box b = boundingBox.get();
            if (b) return b;
        }
        if (getBoundingBox.is_valid())
        {
            return getBoundingBox.call();
        }
        return maps::Box{ nullptr };
    }

    maths::vector3d get_position() const
    {
        if (pos.is_valid())
        {
            maps::Vec3d p = pos.get();
            if (p) return p.to_vector3d();
        }
        if (getPos.is_valid())
        {
            maps::Vec3d p = getPos.call();
            if (p) return p.to_vector3d();
        }
        return { 0, 0, 0 };
    }

    maths::angles get_prev_angles() const
    {
        return { prevYaw.get(), prevPitch.get() };
    }

    maths::angles get_angles() const
    {
        return { yaw.get(), pitch.get() };
    }

    jni::method<EntityPose, "method_18376"> getPose{ *this };
    jni::method<EntityDimensions, "method_18377", jni::NOT_STATIC, EntityPose> getDimensions{ *this };
    jni::method<jfloat, "method_18381", jni::NOT_STATIC, EntityPose> getEyeHeight{ *this };
    jni::method<Text, "method_23315"> getDefaultName{ *this };
    jni::method<String, "method_5820"> getEntityName{ *this };
    jni::method<jboolean, "method_5805"> isAlive{ *this };
    jni::method<jboolean, "method_7325"> isSpectator{ *this };
    jni::method<HitResult, "method_5745", jni::NOT_STATIC, jdouble, jfloat, jboolean> raycast{ *this };
		jni::method<jboolean, "method_5624", jni::NOT_STATIC> isSprinting{ *this };
		jni::method<jboolean, "method_5765", jni::NOT_STATIC> hasVehicle{ *this };
    jni::method<void, "method_5728", jni::NOT_STATIC, jboolean> setSprinting{ *this };
    jni::method<void, "method_5660", jni::NOT_STATIC, jboolean> setSneaking{ *this };
    jni::method<void, "method_5834", jni::NOT_STATIC, jboolean> setGlowing{ *this };
    jni::method<void, "method_5729", jni::NOT_STATIC, jint, jboolean> setFlag{ *this };
    jni::method<jboolean, "method_5681"> isSwimming{ *this };
    jni::method<Vec3d, "method_18798"> getVelocity{ *this };
		jni::method<void, "method_18800", jni::NOT_STATIC, jdouble, jdouble, jdouble> setVelocity{ *this };
		jni::method<void, "method_5814", jni::NOT_STATIC, jdouble, jdouble, jdouble> setPosition{ *this };
		jni::method<void, "method_5808", jni::NOT_STATIC, jdouble, jdouble, jdouble, jfloat, jfloat> refreshPositionAndAngles{ *this };
    jni::method<jfloat, "method_17681"> getWidth{ *this };
    jni::method<jfloat, "method_17682"> getHeight{ *this };
    jni::method<Text, "method_5797"> getCustomName{ *this };
    jni::method<jboolean, "method_5807"> isCustomNameVisible{ *this };
    jni::method<void, "method_5880", jni::NOT_STATIC, jboolean> setCustomNameVisible{ *this };
        jni::method<Vec3d, "method_30950", jni::NOT_STATIC, jfloat> getLerpedPos{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF_EX(ItemEntity, "net/minecraft/class_1542", Entity)
        jni::method<ItemStack, "method_6983"> getStack{ *this };
    END_KLASS_DEF()

	BEGIN_KLASS_MEMBERS(RaycastContext)
		jni::constructor<Vec3d, Vec3d, RaycastContext$ShapeType,
			RaycastContext$FluidHandling, Entity> constructor{ *this };
	END_KLASS_MEMBERS()

	BEGIN_KLASS_DEF_EX(ArmorStandEntity, "net/minecraft/class_1531", Entity)
	END_KLASS_DEF()

	BEGIN_KLASS_DEF_EX(TextDisplayEntity, "net/minecraft/class_8113$class_8123", Entity)
		// These are private in Java, but remain callable through JNI. Keeping the
		// original Text object lets the client restore server holograms exactly.
		jni::method<Text, "method_48915"> getText{ *this };
		jni::method<void, "method_48911", jni::NOT_STATIC, Text> setText{ *this };
	END_KLASS_DEF()

	BEGIN_KLASS_DEF_EX(EndCrystalEntity, "net/minecraft/class_1511", Entity)
	END_KLASS_DEF()

        BEGIN_KLASS_DEF(DamageSource, "net/minecraft/class_1282")
        jni::method<Entity, "method_5529"> getAttacker{ *this };
    END_KLASS_DEF()

		BEGIN_KLASS_DEF_EX(LivingEntity, "net/minecraft/class_1309", Entity)
    jni::field<jint, "field_6273"> lastAttackedTicks{ *this };
    jni::field<jint, "field_6235"> hurtTime{ *this };
    jni::field<jint, "field_6254"> maxHurtTime{ *this };
    jni::field<jint, "field_6228"> jumpingCooldown{ *this };
    // Yarn 1.21.1 intermediary mappings.
    jni::method<jfloat, "method_6032"> getHealth{ *this };
    jni::method<jfloat, "method_6063"> getMaxHealth{ *this };
    jni::method<jboolean, "method_6128"> isFallFlying{ *this };
    jni::method<jboolean, "method_6113"> isSleeping{ *this };
    jni::method<jboolean, "method_6115"> isUsingItem{ *this };
	jni::method<jint, "method_6048"> getItemUseTime{ *this };
	jni::method<ItemStack, "method_6030"> getActiveItem{ *this };
		jni::method<jboolean, "method_6039"> isBlocking{ *this };
		jni::method<jboolean, "method_6101"> isClimbing{ *this };
    jni::method<Entity, "method_6065"> getAttacker{ *this };
    jni::method<DamageSource, "method_6081"> getRecentDamageSource{ *this };
    jni::method<void, "method_6043"> jump{ *this };
    jni::method<jboolean, "method_24518", jni::NOT_STATIC, Item> isHolding{ *this };
    jni::method<ItemStack, "method_6047"> getMainHandStack{ *this };
    jni::method<jboolean, "method_6092", jni::NOT_STATIC, StatusEffectInstance> addStatusEffect{ *this };
    jni::method<jboolean, "method_6016", jni::NOT_STATIC, StatusEffect> removeStatusEffect{ *this };
		jni::method<jboolean, "method_6059", jni::NOT_STATIC, StatusEffect> hasStatusEffect{ *this };
		jni::method<jboolean, "method_6059", jni::NOT_STATIC, RegistryEntry> hasStatusEffectEntry{ *this };
    jni::method<void, "method_6122", jni::NOT_STATIC, Hand, ItemStack> setStackInHand{ *this };
    jni::method<void, "method_6104", jni::NOT_STATIC, Hand> swingHand{ *this };
    END_KLASS_DEF()

		BEGIN_KLASS_DEF_EX(PlayerEntity, "net/minecraft/class_1657", LivingEntity)
        jni::method<jfloat, "method_7261", jni::NOT_STATIC, jfloat> getAttackCooldownProgress{ *this };
    jni::method<void, "method_7350", jni::NOT_STATIC> resetLastAttackedTicks{ *this };
    // Vanilla 1.21.1 interaction ranges; delivery checks never extend reach.
    jni::method<jdouble, "method_55754"> getBlockInteractionRange{ *this };
    jni::method<jdouble, "method_55755"> getEntityInteractionRange{ *this };
    jni::method<GameProfile, "method_7334"> getGameProfile{ *this };
    jni::method<ItemStack, "method_6079", jni::NOT_STATIC> getOffHandStack{ *this };
    jni::method<PlayerInventory, "method_31548"> getInventory{ *this };
    jni::method<PlayerAbilities, "method_31549"> getAbilities{ *this };
    jni::field<ScreenHandler, "field_7512"> currentScreenHandler{ *this };
    jni::method<jdouble, "method_5858", jni::NOT_STATIC, Entity> squaredDistanceTo{ *this };
    END_KLASS_DEF()

}
