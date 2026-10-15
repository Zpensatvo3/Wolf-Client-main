#pragma once
#include "geometry.hpp"

// Fabric 1.21.1 intermediary bindings; original member definitions preserved.
namespace maps
{
    KLASS_DECLARATION(Text, "net/minecraft/class_2561")
        BEGIN_KLASS_DEF(Item, "net/minecraft/class_1792")
        jni::method<String, "method_7876"> getTranslationKey{ *this };
        END_KLASS_DEF()

        BEGIN_KLASS_DEF_EX(SwordItem, "net/minecraft/class_1829", Item)
        END_KLASS_DEF()

        BEGIN_KLASS_DEF_EX(AxeItem, "net/minecraft/class_1743", Item)
        END_KLASS_DEF()

	BEGIN_KLASS_DEF_EX(BlockItem, "net/minecraft/class_1747", Item)
	END_KLASS_DEF()

	BEGIN_KLASS_DEF_EX(MaceItem, "net/minecraft/class_9362", Item)
	END_KLASS_DEF()

        BEGIN_KLASS_DEF_EX(ShieldItem, "net/minecraft/class_1819", Item)
        END_KLASS_DEF()


        BEGIN_KLASS_DEF(Items, "net/minecraft/class_1802")
        jni::field<Item, "field_8288", jni::STATIC> TOTEM_OF_UNDYING{ *this };
    jni::field<Item, "field_8436", jni::STATIC> SPLASH_POTION{ *this };
    jni::field<Item, "field_8574", jni::STATIC> POTION{ *this };
    jni::field<Item, "field_8255", jni::STATIC> SHIELD{ *this };
    jni::field<Item, "field_8406", jni::STATIC> WOODEN_AXE{ *this };
    jni::field<Item, "field_8062", jni::STATIC> STONE_AXE{ *this };
    jni::field<Item, "field_8475", jni::STATIC> IRON_AXE{ *this };
    jni::field<Item, "field_8825", jni::STATIC> GOLDEN_AXE{ *this };
    jni::field<Item, "field_8556", jni::STATIC> DIAMOND_AXE{ *this };
    jni::field<Item, "field_22025", jni::STATIC> NETHERITE_AXE{ *this };
    jni::field<Item, "field_8639", jni::STATIC> FIREWORK_ROCKET{ *this };
    jni::field<Item, "field_8287", jni::STATIC> EXPERIENCE_BOTTLE{ *this };
    jni::field<Item, "field_8281", jni::STATIC> OBSIDIAN{ *this };
    jni::field<Item, "field_8801", jni::STATIC> GLOWSTONE{ *this };
    jni::field<Item, "field_8301", jni::STATIC> END_CRYSTAL{ *this };
    jni::field<Item, "field_8634", jni::STATIC> ENDER_PEARL{ *this };
	jni::field<Item, "field_8102", jni::STATIC> BOW{ *this };
	jni::field<Item, "field_8399", jni::STATIC> CROSSBOW{ *this };
	jni::field<Item, "field_8547", jni::STATIC> TRIDENT{ *this };
	jni::field<Item, "field_8543", jni::STATIC> SNOWBALL{ *this };
	jni::field<Item, "field_8803", jni::STATIC> EGG{ *this };
	jni::field<Item, "field_8150", jni::STATIC> LINGERING_POTION{ *this };
    jni::field<Item, "field_8598", jni::STATIC> ENCHANTED_BOOK{ *this };
    jni::field<Item, "field_8674", jni::STATIC> WRITABLE_BOOK{ *this };
    jni::field<Item, "field_8360", jni::STATIC> WRITTEN_BOOK{ *this };
    jni::field<Item, "field_8529", jni::STATIC> BOOK{ *this };
    jni::field<Item, "field_8361", jni::STATIC> KNOWLEDGE_BOOK{ *this };
    jni::field<Item, "field_23141", jni::STATIC> RESPAWN_ANCHOR{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(BlockPos, "net/minecraft/class_2338")
        jni::constructor<jint, jint, jint> constructor{ *this };
        jni::field<BlockPos, "field_10980", jni::STATIC> ORIGIN{ *this };
    jni::method<jint, "method_10263", jni::NOT_STATIC> getX{ *this };
    jni::method<jint, "method_10264", jni::NOT_STATIC> getY{ *this };
    jni::method<jint, "method_10260", jni::NOT_STATIC> getZ{ *this };
    jni::method<BlockPos, "method_10087", jni::NOT_STATIC> down{ *this };
    jni::method<BlockPos, "method_10084", jni::NOT_STATIC> up{ *this };
    jni::method<BlockPos, "method_10088", jni::NOT_STATIC, jint, jint, jint> add{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(Property, "net/minecraft/class_2769")
        END_KLASS_DEF()

        BEGIN_KLASS_DEF_EX(IntProperty, "net/minecraft/class_2758", Property)
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(RespawnAnchorBlock, "net/minecraft/class_4969")
        jni::field<IntProperty, "field_23153", jni::STATIC> CHARGES{ *this };
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(Block, "net/minecraft/class_2248")
		jni::method<Item, "method_8389"> asItem{ *this };
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(Blocks, "net/minecraft/class_2246")
        jni::field<Block, "field_10343", jni::STATIC> COBWEB{ *this };
    jni::field<Block, "field_23152", jni::STATIC> RESPAWN_ANCHOR{ *this };
    jni::field<Block, "field_10540", jni::STATIC> OBSIDIAN{ *this };
    jni::field<Block, "field_9987", jni::STATIC> BEDROCK{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(BlockState, "net/minecraft/class_2680")
        jni::method<jboolean, "method_27852", jni::NOT_STATIC, Block> isOf{ *this };
    jni::method<jboolean, "method_26215", jni::NOT_STATIC> isAir{ *this };
    jni::method<jboolean, "method_26225"> isOpaque{ *this };
		jni::method<Block, "method_26204"> getBlock{ *this };
    jni::method<Comparable, "method_11654", jni::NOT_STATIC, Property> get{ *this };
    END_KLASS_DEF()

	BEGIN_KLASS_DEF(TeamVisibilityRule, "net/minecraft/class_270$class_272")
		jni::field<TeamVisibilityRule, "field_1443", jni::STATIC> NEVER{ *this };
	END_KLASS_DEF()

	BEGIN_KLASS_DEF(Team, "net/minecraft/class_268")
		jni::method<String, "method_1197"> getName{ *this };
		jni::method<void, "method_1149", jni::NOT_STATIC, TeamVisibilityRule> setNameTagVisibilityRule{ *this };
	END_KLASS_DEF()

	BEGIN_KLASS_DEF(Scoreboard, "net/minecraft/class_269")
		jni::method<Team, "method_1153", jni::NOT_STATIC, String> getTeam{ *this };
		jni::method<Team, "method_1164", jni::NOT_STATIC, String> getScoreHolderTeam{ *this };
		jni::method<Team, "method_1171", jni::NOT_STATIC, String> addTeam{ *this };
		jni::method<void, "method_1191", jni::NOT_STATIC, Team> removeTeam{ *this };
		jni::method<jboolean, "method_1172", jni::NOT_STATIC, String, Team> addScoreHolderToTeam{ *this };
		jni::method<void, "method_1157", jni::NOT_STATIC, String, Team> removeScoreHolderFromTeam{ *this };
	END_KLASS_DEF()

    // Verified against Yarn 1.21.1+build.3. Read only the client's already
    // loaded chunk array; ESP must never request/generate missing chunks.
    BEGIN_KLASS_DEF(ChunkPos, "net/minecraft/class_1923")
        jni::field<jint, "field_9181"> x{ *this };
        jni::field<jint, "field_9180"> z{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(Palette, "net/minecraft/class_2837")
        jni::method<Object, "method_12288", jni::NOT_STATIC, jint> get{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(PaletteStorage, "net/minecraft/class_6490")
        jni::method<void, "method_39892", jni::NOT_STATIC, jni::array<jint>> writePaletteIndices{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(PaletteData, "net/minecraft/class_2841$class_6561")
        jni::method<Palette, "comp_119"> palette{ *this };
        jni::method<PaletteStorage, "comp_118"> storage{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(PalettedContainer, "net/minecraft/class_2841")
        jni::field<PaletteData, "field_34560"> data{ *this };
        jni::method<PalettedContainer, "method_39957"> copy{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(ChunkSection, "net/minecraft/class_2826")
        jni::method<jboolean, "method_38292"> isEmpty{ *this };
        jni::method<PalettedContainer, "method_12265"> getBlockStateContainer{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(Chunk, "net/minecraft/class_2791")
        jni::method<ChunkPos, "method_12004"> getPos{ *this };
        jni::method<jni::array<ChunkSection>, "method_12006"> getSectionArray{ *this };
        jni::method<jint, "method_31607"> getBottomY{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF_EX(WorldChunk, "net/minecraft/class_2818", Chunk)
        jni::method<Map, "method_12214"> getBlockEntities{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(BlockEntity, "net/minecraft/class_2586")
        jni::method<BlockPos, "method_11016"> getPos{ *this };
        jni::method<BlockState, "method_11010"> getCachedState{ *this };
        jni::method<jboolean, "method_11015"> isRemoved{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(ClientChunkMap, "net/minecraft/class_631$class_3681")
        jni::field<AtomicReferenceArray, "field_16251"> chunks{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(ClientChunkManager, "net/minecraft/class_631")
        jni::field<ClientChunkMap, "field_16246"> chunks{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(World, "net/minecraft/class_1937")
        jni::method<BlockState, "method_8320", jni::NOT_STATIC, BlockPos> getBlockState{ *this };
		jni::method<BlockHitResult, "method_17742", jni::NOT_STATIC, RaycastContext> raycast{ *this };
		jni::method<Scoreboard, "method_8428"> getScoreboard{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(WorldProperties, "net/minecraft/class_638$class_5271")
        jni::method<jlong, "method_188"> getTime{ *this };
    jni::method<jlong, "method_217"> getTimeOfDay{ *this };
    jni::method<jboolean, "method_203"> isThundering{ *this };
    jni::method<jboolean, "method_156"> isRaining{ *this };
    jni::method<jint, "method_207"> getDifficulty{ *this };
    jni::method<void, "method_157", jni::NOT_STATIC, jboolean> setRaining{ *this };
    jni::method<BlockPos, "method_56126"> getSpawnPos{ *this };
    jni::method<void, "method_29035", jni::NOT_STATIC, jlong> setTimeOfDay{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(Direction, "net/minecraft/class_2350")
        jni::field<Direction, "field_11033", jni::STATIC> DOWN{ *this };
    jni::field<Direction, "field_11036", jni::STATIC> UP{ *this };
    jni::field<Direction, "field_11043", jni::STATIC> NORTH{ *this };
    jni::field<Direction, "field_11035", jni::STATIC> SOUTH{ *this };
    jni::field<Direction, "field_11039", jni::STATIC> WEST{ *this };
    jni::field<Direction, "field_11034", jni::STATIC> EAST{ *this };
    jni::method<jint, "method_10148", jni::NOT_STATIC> getOffsetX{ *this };
    jni::method<jint, "method_10164", jni::NOT_STATIC> getOffsetY{ *this };
    jni::method<jint, "method_10165", jni::NOT_STATIC> getOffsetZ{ *this };
    END_KLASS_DEF()

}
