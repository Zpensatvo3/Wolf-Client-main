#pragma once
#include "world_blocks.hpp"

// Fabric 1.21.1 intermediary bindings; original member definitions preserved.
namespace maps
{
        BEGIN_KLASS_DEF(Packet, "net/minecraft/class_2596")
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(GameProfile, "com/mojang/authlib/GameProfile")
        jni::method<String, "getName", jni::NOT_STATIC> getName{ *this };
        jni::method<UUID, "getId"> getId{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(PlayerListEntry, "net/minecraft/class_640")
        jni::method<GameProfile, "method_2966"> getProfile{ *this };
        jni::method<jint, "method_2959"> getLatency{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(ClientPlayNetworkHandler, "net/minecraft/class_634")
        jni::method<void, "method_2883", jni::NOT_STATIC, Packet> sendPacket{ *this };
        jni::method<Collection, "method_2880"> getPlayerList{ *this };
        jni::method<PlayerListEntry, "method_2874", jni::NOT_STATIC, String> getPlayerListEntryByName{ *this };
        jni::method<PlayerListEntry, "method_2871", jni::NOT_STATIC, UUID> getPlayerListEntry{ *this };
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
        jni::method<Text, "method_7964"> getName{ *this };
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

    BEGIN_KLASS_DEF(Element, "net/minecraft/class_364")
        jni::method<jboolean, "method_25370"> isFocused{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF_EX(ParentElement, "net/minecraft/class_4069", Element)
        jni::method<List, "method_25396"> children{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF_EX(TextFieldWidget, "net/minecraft/class_342", Element)
        jni::method<jboolean, "method_1885"> isVisible{ *this };
        jni::method<jboolean, "method_20315"> isActive{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(RecipeBookWidget, "net/minecraft/class_507")
        jni::field<TextFieldWidget, "field_3089"> searchField{ *this };
        jni::method<jboolean, "method_2605"> isOpen{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(RecipeBookProvider, "net/minecraft/class_518")
        jni::method<RecipeBookWidget, "method_2659"> getRecipeBookWidget{ *this };
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
        jni::field<TextFieldWidget, "field_2894"> searchBox{ *this };
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

}
