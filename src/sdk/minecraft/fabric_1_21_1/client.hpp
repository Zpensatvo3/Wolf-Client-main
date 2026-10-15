#pragma once
#include "input.hpp"

// Fabric 1.21.1 intermediary bindings; original member definitions preserved.
namespace maps
{
	BEGIN_KLASS_DEF(ActionResult, "net/minecraft/class_1269")
        jni::method<jboolean, "method_23665"> isAccepted{ *this };
	END_KLASS_DEF()

        BEGIN_KLASS_DEF_EX(EntityHitResult, "net/minecraft/class_3966", HitResult)
        jni::field<Entity, "field_17592"> entity{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_MEMBERS(BlockHitResult)
        jni::constructor<Vec3d, Direction, BlockPos, jboolean> constructor{ *this };
        jni::method<Direction, "method_17780", jni::NOT_STATIC> getSide{ *this };
    jni::method<BlockPos, "method_17777", jni::NOT_STATIC> getBlockPos{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(ClientPlayerInteractionManager, "net/minecraft/class_636")
        jni::method<void, "method_2918", jni::NOT_STATIC, PlayerEntity, Entity> attackEntity{ *this };
    jni::method<jboolean, "method_2910", jni::NOT_STATIC, BlockPos, Direction> attackBlock{ *this };
    jni::method<ActionResult, "method_2896", jni::NOT_STATIC, ClientPlayerEntity, Hand, BlockHitResult> interactBlock{ *this };
    jni::method<void, "method_2906", jni::NOT_STATIC, jint, jint, jint, SlotActionType, PlayerEntity> clickSlot{ *this };
    jni::method<void, "method_2911", jni::NOT_STATIC> syncSelectedSlot{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(PlayerInteractBlockC2SPacket, "net/minecraft/class_2885")
        jni::constructor<Hand, BlockHitResult, jint> constructor{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(MatrixStack, "net/minecraft/class_4587")
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(Window, "net/minecraft/class_1041")
    jni::method<jlong, "method_4490"> getHandle{ *this };
        jni::method<jint, "method_4486"> getScaledWidth{ *this };
    jni::method<jint, "method_4502"> getScaledHeight{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(TextRenderer, "net/minecraft/class_327")
        jni::method<jint, "method_30880", jni::NOT_STATIC, String> getWidth{ *this };
    jni::method<jint, "method_30881", jni::NOT_STATIC, MatrixStack, String, jfloat, jfloat, jint> drawWithShadow{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(Camera, "net/minecraft/class_4184")
    jni::method<jfloat, "method_19329"> getPitch{ *this };
    jni::method<jfloat, "method_19330"> getYaw{ *this };
    jni::method<Vec3d, "method_19326"> getPos{ *this };
	jni::method<jboolean, "method_19333"> isThirdPerson{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(EntityRenderDispatcher, "net/minecraft/class_898")
    jni::field<Camera, "field_4686"> camera{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(GameRenderer, "net/minecraft/class_757")
        // Read-only vanilla ray query, rather than last frame's crosshair cache.
        jni::method<HitResult, "method_56153", jni::NOT_STATIC, Entity, jdouble, jdouble, jfloat> findCrosshairTarget{ *this };
		// The value used by the world renderer after zoom/FOV multipliers.
		jni::method<jdouble, "method_3196", jni::NOT_STATIC, Camera, jfloat, jboolean> getFov{ *this };
		jni::multi_method<Camera, "method_19418", "method_19349"> getCamera{ *this };
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(ServerInfo, "net/minecraft/class_642")
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(Session, "net/minecraft/class_320")
        jni::method<String, "method_1676", jni::NOT_STATIC> getUsername{ *this };
    jni::method<UUID, "method_44717", jni::NOT_STATIC> getUuidOrNull{ *this };
    jni::method<String, "method_1675", jni::NOT_STATIC> getSessionId{ *this };
    jni::method<String, "method_1674", jni::NOT_STATIC> getAccessToken{ *this };
    jni::method<String, "method_38740", jni::NOT_STATIC> getClientId{ *this };
    jni::method<String, "method_38741", jni::NOT_STATIC> getXuid{ *this };
    jni::method<String, "method_35718", jni::NOT_STATIC> getAccountType{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(MinecraftClient, "net/minecraft/class_310")
        jni::field<MinecraftClient, "field_1700", jni::STATIC> instance{ *this };
    jni::field<ClientWorld, "field_1687"> world{ *this };
    jni::field<ClientPlayerEntity, "field_1724"> player{ *this };
    jni::field<HitResult, "field_1765"> crosshairTarget{ *this };
    jni::field<ClientPlayerInteractionManager, "field_1761"> interactionManager{ *this };
    jni::field<Mouse, "field_1729"> mouse{ *this };
    jni::field<GameOptions, "field_1690"> options{ *this };
    jni::field<RenderTickCounter, "field_1728"> renderTickCounter{ *this };
    jni::field<Screen, "field_1755"> currentScreen{ *this };
    jni::method<ClientPlayNetworkHandler, "method_1562"> getNetworkHandler{ *this };
    jni::method<Window, "method_22683"> getWindow{ *this };
    jni::method<TextureManager, "method_1531"> getTextureManager{ *this };
    jni::field<TextRenderer, "field_1772"> textRenderer{ *this };
    jni::field<EntityRenderDispatcher, "field_1731"> entityRenderDispatcher{ *this };
    jni::field<GameRenderer, "field_1773"> gameRenderer{ *this };
    jni::field<Session, "field_1726"> session{ *this };
    jni::method<GameProfile, "method_53462"> getGameProfile{ *this };
    jni::method<ServerInfo, "method_1558"> getCurrentServerEntry{ *this };
		jni::method<Entity, "method_1560"> getCameraEntity{ *this };
		jni::method<void, "method_1504", jni::NOT_STATIC, Entity> setCameraEntity{ *this };
    jni::method<jboolean, "method_1536", jni::NOT_STATIC> doAttack{ *this };
    jni::method<void, "method_1583", jni::NOT_STATIC> doItemUse{ *this };
    jni::field<jint, "field_1752"> itemUseCooldown{ *this };
    END_KLASS_DEF()
}
