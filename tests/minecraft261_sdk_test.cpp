#include "../src/mappings.hpp"
#include "../src/platform/adapter/item_transform.hpp"
#include <cassert>
#include <iostream>
#include <limits>
#include <string_view>

using namespace std::literals;
static_assert(ONESTEP_MINECRAFT_VERSION == 26100);
static_assert(mc::target.id == mc::version::v26_1);
static_assert(mc::parse_version("26.1") == mc::target.id);
static_assert(!mc::matches_target(mc::parse_version("26.1.1")));
static_assert(!mc::matches_target(mc::parse_version("1.21.11")));
static_assert(!mc::matches_target(mc::parse_version("26.1-pre1")));
static_assert(std::string_view(maps::MinecraftClient::get_name()) == "net/minecraft/client/Minecraft"sv);
static_assert(std::string_view(maps::SlotActionType::get_name()) == "net/minecraft/world/inventory/ContainerInput"sv);
static_assert(std::string_view(decltype(maps::ClientPlayerInteractionManager_members::clickSlot)::get_name()) == "handleContainerInput"sv);
static_assert(std::string_view(decltype(maps::ClientPlayerInteractionManager_members::clickSlot)::get_signature()) ==
    "(IIILnet/minecraft/world/inventory/ContainerInput;Lnet/minecraft/world/entity/player/Player;)V"sv);
static_assert(std::string_view(decltype(maps::Camera_members::getFov)::get_signature()) == "()F"sv);
static_assert(std::string_view(decltype(maps::ItemRenderLayer_members::tints)::get_signature()) == "Lit/unimi/dsi/fastutil/ints/IntList;"sv);
static_assert(std::string_view(decltype(maps::ItemBakedQuad_members::getMaterial)::get_signature()) ==
    "()Lnet/minecraft/client/resources/model/geometry/BakedQuad$MaterialInfo;"sv);
static_assert(decltype(maps::ClientPlayerEntity_members::findCrosshairTarget)::is_method_static());

int main()
{
    using platform::adapter::apply_item_local_transform;
    std::array<float, 16> matrix{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    std::array<float, 3> point{1,2,3};
    assert(apply_item_local_transform(point, matrix));
    assert((point == std::array<float,3>{1,2,3}));
    matrix[12] = 4; matrix[13] = -1; matrix[14] = 2;
    assert(apply_item_local_transform(point, matrix));
    assert((point == std::array<float,3>{5,1,5}));
    // Local coordinates are transformed before the HUD's existing -.5 centering.
    matrix = {2,0,0,0, 0,3,0,0, 0,0,4,0, 0,0,0,1};
    point = {0.5f,0.5f,0.5f};
    assert(apply_item_local_transform(point, matrix));
    assert((point == std::array<float,3>{1,1.5f,2}));
    const auto original = point;
    matrix[0] = std::numeric_limits<float>::infinity();
    assert(!apply_item_local_transform(point, matrix) && point == original);
    matrix[0] = 1; matrix[15] = 0;
    assert(!apply_item_local_transform(point, matrix) && point == original);
    matrix[15] = 1; matrix[3] = 0.5f;
    assert(!apply_item_local_transform(point, matrix) && point == original);
    std::cout << "26.1 native SDK: exact names, contracts, static ray query, bounded affine item transforms passed\n";
}
