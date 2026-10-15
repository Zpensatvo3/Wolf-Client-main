#pragma once
#include <string>
#include <cmath>
#include "meta_jni.hpp"
#include "maths/maths.hpp"

namespace maps
{
    BEGIN_KLASS_DEF(Object, "java/lang/Object")
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(Comparable, "java/lang/Comparable")
        END_KLASS_DEF()

        BEGIN_KLASS_DEF_EX(Integer, "java/lang/Integer", Comparable)
        jni::method<jint, "intValue", jni::NOT_STATIC> intValue{ *this };
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(Class, "java/lang/Class")
        operator jclass() const
    {
        return (jclass)object_instance;
    }
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(String, "java/lang/String")
        static String create(const char* str)
    {
        return String(jni::get_env()->NewStringUTF(str));
    }

    std::string to_string() const
    {
        if (!object_instance) return std::string();
        jstring str_obj = (jstring)object_instance;
        jsize utf8_size = jni::get_env()->GetStringUTFLength(str_obj);
        jsize size = jni::get_env()->GetStringLength(str_obj);

        std::string str(utf8_size, '\0');
        jni::get_env()->GetStringUTFRegion(str_obj, 0, size, str.data());
        return str;
    }
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(Collection, "java/util/Collection")
        jni::method<jni::array<Object>, "toArray"> toArray{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF_EX(List, "java/util/List", Collection)
        jni::method<Object, "get", jni::NOT_STATIC, jint> get{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF_EX(DefaultedList, "net/minecraft/class_2371", List)
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(Iterator, "java/util/Iterator")
        jni::method<jboolean, "hasNext"> hasNext{ *this };
        jni::method<Object, "next"> next{ *this };
        END_KLASS_DEF()

	BEGIN_KLASS_DEF(Iterable, "java/lang/Iterable")
		jni::method<Iterator, "iterator"> iterator{ *this };
	END_KLASS_DEF()

        BEGIN_KLASS_DEF(URL, "java/net/URL")
        jni::constructor<String> constructor{ *this };
    jni::method<String, "toString"> toString{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(ClassLoader, "java/lang/ClassLoader")
        jni::method<Class, "findClass", jni::NOT_STATIC, String> findClass{ *this };
    jni::method<Class, "loadClass", jni::NOT_STATIC, String> loadClass{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF_EX(URLClassLoader, "java/net/URLClassLoader", ClassLoader)
        jni::constructor<jni::array<URL>> constructor{ *this };
    jni::constructor<jni::array<URL>, ClassLoader> constructor2{ *this };
    jni::method<void, "addURL", jni::NOT_STATIC, URL> addURL{ *this };
    jni::method<jni::array<URL>, "getURLs", jni::NOT_STATIC> getURLs{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(Double, "java/lang/Double")
        jni::method<jdouble, "doubleValue"> doubleValue{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(PrintStream, "java/io/PrintStream")
        jni::method<void, "println", jni::NOT_STATIC, String> println{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(System, "java/lang/System")
        jni::field<PrintStream, "out", jni::STATIC> out{ *this };
    END_KLASS_DEF()

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
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(BlockState, "net/minecraft/class_2680")
        jni::method<jboolean, "method_27852", jni::NOT_STATIC, Block> isOf{ *this };
    jni::method<jboolean, "method_26215", jni::NOT_STATIC> isAir{ *this };
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

        BEGIN_KLASS_DEF(Packet, "net/minecraft/class_2596")
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(GameProfile, "com/mojang/authlib/GameProfile")
        jni::method<String, "getName", jni::NOT_STATIC> getName{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(PlayerListEntry, "net/minecraft/class_640")
        jni::method<GameProfile, "method_2966"> getProfile{ *this };
        jni::method<jint, "method_2959"> getLatency{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(ClientPlayNetworkHandler, "net/minecraft/class_634")
        jni::method<void, "method_2883", jni::NOT_STATIC, Packet> sendPacket{ *this };
        jni::method<Collection, "method_2880"> getPlayerList{ *this };
        jni::method<PlayerListEntry, "method_2874", jni::NOT_STATIC, String> getPlayerListEntryByName{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(PlayerActionC2SPacket$Action, "net/minecraft/class_2846$class_2847")
        jni::field<PlayerActionC2SPacket$Action, "field_12969", jni::STATIC> SWAP_ITEM_WITH_OFFHAND{ *this };
    jni::field<PlayerActionC2SPacket$Action, "field_12974", jni::STATIC> RELEASE_USE_ITEM{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(PlayerActionC2SPacket, "net/minecraft/class_2846")
        jni::constructor<PlayerActionC2SPacket$Action, BlockPos, Direction> constructor{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(PickFromInventoryC2SPacket, "net/minecraft/class_2838")
        jni::constructor<jint> constructor{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(ClickSlotC2SPacket, "net/minecraft/class_2813")
        jni::constructor<jint> constructor{ *this };
    END_KLASS_DEF()

	BEGIN_KLASS_DEF(ComponentType, "net/minecraft/class_9331")
	END_KLASS_DEF()

	BEGIN_KLASS_DEF(ChargedProjectilesComponent, "net/minecraft/class_9278")
		jni::method<jboolean, "method_57442"> isEmpty{ *this };
		jni::method<jboolean, "method_57438", jni::NOT_STATIC, Item> contains{ *this };
	END_KLASS_DEF()

	BEGIN_KLASS_DEF(RegistryEntry, "net/minecraft/class_6880")
	END_KLASS_DEF()

        BEGIN_KLASS_DEF(ItemStack, "net/minecraft/class_1799")
        jni::field<Item, "field_8038"> item{ *this };
        jni::method<jboolean, "method_7960"> isEmpty{ *this };
        jni::method<ItemStack, "method_7971", jni::NOT_STATIC, jint> split{ *this };
        jni::method<jboolean, "method_31574", jni::NOT_STATIC, Item> isOf{ *this };
        jni::method<ItemStack, "method_7972", jni::NOT_STATIC> copy{ *this };
        jni::method<jint, "method_7947"> getCount{ *this };
        jni::method<jint, "method_7914"> getMaxCount{ *this };
		jni::method<jfloat, "method_7924", jni::NOT_STATIC, BlockState> getMiningSpeedMultiplier{ *this };
		jni::method<jboolean, "method_7951", jni::NOT_STATIC, BlockState> isSuitableFor{ *this };
		jni::method<Object, "method_57824", jni::NOT_STATIC, ComponentType> getComponent{ *this };
        jni::method<jboolean, "method_31577", jni::STATIC, ItemStack, ItemStack> areItemsAndComponentsEqual{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(ScreenHandler, "net/minecraft/class_1703")
        jni::field<jint, "field_7763"> syncId{ *this };
        jni::field<DefaultedList, "field_7761"> slots{ *this };
        jni::method<ItemStack, "method_34255"> getCursorStack{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(UpdateSelectedSlotC2SPacket, "net/minecraft/class_2868")
        jni::constructor<jint> constructor{ *this };
    jni::method<jint, "method_12442"> getSelectedSlot{ *this };
    jni::field<jint, "field_13052"> selectedSlot{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(NbtCompound, "net/minecraft/class_2487")
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(DataComponentTypes, "net/minecraft/class_9334")
		jni::field<ComponentType, "field_50075", jni::STATIC> FOOD{ *this };
		jni::field<ComponentType, "field_49651", jni::STATIC> POTION_CONTENTS{ *this };
		jni::field<ComponentType, "field_49649", jni::STATIC> CHARGED_PROJECTILES{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(Screen, "net/minecraft/class_437")
		jni::field<jint, "field_22789"> width{ *this };
		jni::field<jint, "field_22790"> height{ *this };
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(Inventory, "net/minecraft/class_1263")
        jni::method<ItemStack, "method_5438", jni::NOT_STATIC, jint> getStack{ *this };
    jni::method<jint, "method_5439"> size{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(Slot, "net/minecraft/class_1735")
        jni::field<Inventory, "field_7871"> inventory{ *this };
        jni::field<jint, "field_7875"> index{ *this };
        jni::field<jint, "field_7874"> id{ *this };
        jni::method<jboolean, "method_7680", jni::NOT_STATIC, ItemStack> canInsert{ *this };
        jni::method<ItemStack, "method_7677"> getStack{ *this };
        jni::method<jboolean, "method_7681"> hasStack{ *this };
        jni::method<jint, "method_7676", jni::NOT_STATIC, ItemStack> getMaxItemCount{ *this };
        jni::method<jboolean, "method_7682"> isEnabled{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF_EX(HandledScreen, "net/minecraft/class_465", Screen)
        jni::field<Slot, "field_2787"> focusedSlot{ *this };
        jni::field<ScreenHandler, "field_2797"> handler{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF_EX(InventoryScreen, "net/minecraft/class_490", Screen)
        END_KLASS_DEF()

	BEGIN_KLASS_DEF_EX(GenericContainerScreen, "net/minecraft/class_476", HandledScreen)
	END_KLASS_DEF()

        BEGIN_KLASS_DEF_EX(CreativeInventoryScreen, "net/minecraft/class_481", Screen)
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(Potion, "net/minecraft/class_1842")
        END_KLASS_DEF()

	BEGIN_KLASS_DEF(PotionContentsComponent, "net/minecraft/class_1844")
		jni::method<jboolean, "method_57401", jni::NOT_STATIC, RegistryEntry> matches{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(Potions, "net/minecraft/class_1847")
		jni::field<RegistryEntry, "field_8963", jni::STATIC> HEALING{ *this };
		jni::field<RegistryEntry, "field_8980", jni::STATIC> STRONG_HEALING{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(SlotActionType, "net/minecraft/class_1713")
        jni::field<SlotActionType, "field_7790", jni::STATIC> PICKUP{ *this };
        jni::field<SlotActionType, "field_7794", jni::STATIC> QUICK_MOVE{ *this };
        jni::field<SlotActionType, "field_7791", jni::STATIC> SWAP{ *this };
    END_KLASS_DEF()

        /* PotionItem in Yarn/Fabric 1.21.1. The previous class_1807 mapping
           made every real potion fail the runtime instanceof check. */
        BEGIN_KLASS_DEF_EX(PotionItem, "net/minecraft/class_1812", Item)
    END_KLASS_DEF()

        BEGIN_KLASS_DEF_EX(SplashPotionItem, "net/minecraft/class_1828", PotionItem)
    END_KLASS_DEF()



        BEGIN_KLASS_DEF(PlayerInventory, "net/minecraft/class_1661")
        jni::method<jint, "method_7395", jni::NOT_STATIC, ItemStack> getSlotWithStack{ *this };
    jni::method<void, "method_7365", jni::NOT_STATIC, jint> swapSlotWithHotbar{ *this };
    jni::field<DefaultedList, "field_7547"> main{ *this };
	/* PlayerInventory.armor in Yarn 1.21.1. */
	jni::field<DefaultedList, "field_7548"> armor{ *this };
    jni::method<ItemStack, "method_7372", jni::NOT_STATIC, jint> getArmorStack{ *this };
    jni::field<DefaultedList, "field_7544"> offHand{ *this };
    jni::field<jint, "field_7545"> selectedSlot{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(Box, "net/minecraft/class_238")
        jni::field<jdouble, "field_1323"> minX{ *this };
    jni::field<jdouble, "field_1322"> minY{ *this };
    jni::field<jdouble, "field_1321"> minZ{ *this };
    jni::field<jdouble, "field_1320"> maxX{ *this };
    jni::field<jdouble, "field_1325"> maxY{ *this };
    jni::field<jdouble, "field_1324"> maxZ{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(EntityPose, "net/minecraft/class_4050")
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(EntityDimensions, "net/minecraft/class_4048")
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(StatusEffect, "net/minecraft/class_1291")
        END_KLASS_DEF()

	BEGIN_KLASS_DEF(StatusEffects, "net/minecraft/class_1294")
		jni::field<RegistryEntry, "field_5919", jni::STATIC> BLINDNESS{ *this };
		jni::field<StatusEffect, "field_5912", jni::STATIC> GLOWING{ *this };
    jni::field<StatusEffect, "field_5925", jni::STATIC> NIGHT_VISION{ *this };
    jni::field<StatusEffect, "field_5918", jni::STATIC> FIRE_RESISTANCE{ *this };
    jni::field<StatusEffect, "field_5924", jni::STATIC> REGENERATION{ *this };
    jni::field<StatusEffect, "field_5917", jni::STATIC> HASTE{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(StatusEffectInstance, "net/minecraft/class_1293")
        jni::constructor<StatusEffect, jint, jint> constructor{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(PlayerAbilities, "net/minecraft/class_1656")
        jni::field<jboolean, "field_7477"> creativeMode{ *this };
    jni::field<jboolean, "field_7478"> allowFlying{ *this };
    jni::field<jboolean, "field_7479"> flying{ *this };
    jni::field<jfloat, "field_7481"> flySpeed{ *this };
    jni::field<jfloat, "field_7482"> walkSpeed{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(OrderedText, "net/minecraft/class_5481")
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(Text, "net/minecraft/class_2561")
        jni::method<Text, "method_30163", jni::STATIC, String> of{ *this };
        jni::method<String, "getString"> getString{ *this };
        jni::method<String, "method_54160"> getLiteralString{ *this };
    jni::method<OrderedText, "method_30937", jni::NOT_STATIC> asOrderedText{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(Hand, "net/minecraft/class_1268")
        jni::field<Hand, "field_5808", jni::STATIC> MAIN_HAND{ *this };
    jni::field<Hand, "field_5810", jni::STATIC> OFF_HAND{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(PlayerInteractItemC2SPacket, "net/minecraft/class_2886")
        jni::constructor<maps::Hand, jint> constructor{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(HitResult$Type, "net/minecraft/class_239$class_240")
        jni::field<HitResult$Type, "field_1331", jni::STATIC> ENTITY{ *this };
    jni::field<HitResult$Type, "field_1332", jni::STATIC> BLOCK{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(HitResult, "net/minecraft/class_239")
        jni::method<HitResult$Type, "method_17783"> getType{ *this };
    jni::method<Vec3d, "method_17784"> getPos{ *this };
    END_KLASS_DEF()

		BEGIN_KLASS_DEF(Entity, "net/minecraft/class_1297")
		jni::method<jfloat, "method_5739", jni::NOT_STATIC, Entity> distanceTo{ *this };
		jni::method<jint, "method_5628"> getId{ *this };
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

	BEGIN_KLASS_DEF(ActionResult, "net/minecraft/class_1269")
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

        BEGIN_KLASS_DEF(UUID, "java/util/UUID")
        jni::method<String, "toString", jni::NOT_STATIC> toString{ *this };
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
