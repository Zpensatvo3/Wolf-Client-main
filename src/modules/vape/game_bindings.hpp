#pragma once
#include "../../mappings.hpp"

// Raw Fabric 1.21.1 / Yarn bindings, deliberately local to the Vape adapter.
// Mapping reference: https://maven.fabricmc.net/docs/yarn-1.21.1+build.3/
namespace vape_bindings
{
    // Unlike a normal Java call, JNI NewObject with a missing constructor ID
    // can terminate the VM. Keep this guard local to the Vape IO boundary.
    template<class Type,class Members,class... Args>
    Type construct(jni::constructor<Args...> Members::* member,
        const std::type_identity_t<Args>&... args)
    {
        auto* env=jni::get_env();
        if(!env||env->ExceptionCheck())return {};
        Type type;
        if(!type.owner_klass||!(type.*member).is_valid()||env->ExceptionCheck())return {};
        return Type::new_object(member,args...);
    }
    BEGIN_KLASS_DEF(Hunger, "net/minecraft/class_1702")
        jni::method<jint, "method_7586"> getFoodLevel{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF_EX(Player, "net/minecraft/class_1657", maps::PlayerEntity)
        jni::method<Hunger, "method_7344"> getHungerManager{*this};
        jni::field<jboolean, "field_5976"> horizontalCollision{*this};
        jni::method<jint, "method_6096"> getArmor{*this};
        jni::method<jboolean, "method_6057", jni::NOT_STATIC, maps::Entity> canSee{*this};
        jni::method<jdouble,"method_45325",jni::NOT_STATIC,maps::RegistryEntry> attribute{*this};
        jni::method<jfloat,"method_6067"> absorption{*this};
        jni::method<maps::StatusEffectInstance,"method_6112",jni::NOT_STATIC,maps::RegistryEntry> effect{*this};
        jni::method<jboolean,"method_5771"> inLava{*this};
        jni::field<jboolean,"field_6252"> handSwinging{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF_EX(World,"net/minecraft/class_638",maps::ClientWorld)
        jni::method<maps::Entity,"method_8469",jni::NOT_STATIC,jint> entity{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Attributes,"net/minecraft/class_5134")
        jni::field<maps::RegistryEntry,"field_23725",jni::STATIC> toughness{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Blocks,"net/minecraft/class_2246")
        jni::field<maps::Block,"field_9987",jni::STATIC> bedrock{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Effects,"net/minecraft/class_1294")
        jni::field<maps::RegistryEntry,"field_5907",jni::STATIC> resistance{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF_EX(Effect,"net/minecraft/class_1293",maps::StatusEffectInstance)
        jni::method<jint,"method_5578"> amplifier{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Optional,"java/util/Optional")
        jni::method<jboolean,"isPresent"> present{*this};
        jni::method<maps::Object,"get"> get{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(RegistryKey,"net/minecraft/class_5321")
        jni::method<maps::Identifier,"method_29177"> value{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF_EX(Entry,"net/minecraft/class_6880",maps::RegistryEntry)
        jni::method<Optional,"method_40230"> key{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF_EX(Set,"java/util/Set",maps::Collection)
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Enchantments,"net/minecraft/class_9304")
        jni::method<Set,"method_57534"> entries{*this};
        jni::method<jint,"method_57536",jni::NOT_STATIC,maps::RegistryEntry> level{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Components,"net/minecraft/class_9334")
        jni::field<maps::ComponentType,"field_49633",jni::STATIC> enchantments{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(VelocityPacket,"net/minecraft/class_2743")
        jni::method<jint,"method_11818"> id{*this};
        jni::method<jdouble,"method_11815"> x{*this};
        jni::method<jdouble,"method_11816"> y{*this};
        jni::method<jdouble,"method_11819"> z{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(ActionPacket,"net/minecraft/class_2846")
        jni::field<maps::PlayerActionC2SPacket$Action,"field_12966"> action{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(CommandMode,"net/minecraft/class_2848$class_2849")
        jni::field<CommandMode,"field_12985",jni::STATIC> stop{*this};
        jni::field<CommandMode,"field_12981",jni::STATIC> start{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Command,"net/minecraft/class_2848")
        jni::constructor<maps::Entity,CommandMode> constructor{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Network,"net/minecraft/class_8673")
        jni::method<void,"method_52787",jni::NOT_STATIC,maps::Packet> send{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(MovePacket,"net/minecraft/class_2828")
        jni::method<jdouble,"method_12269",jni::NOT_STATIC,jdouble> x{*this};
        jni::method<jdouble,"method_12268",jni::NOT_STATIC,jdouble> y{*this};
        jni::method<jdouble,"method_12274",jni::NOT_STATIC,jdouble> z{*this};
        jni::method<jboolean,"method_12273"> ground{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(FullMove,"net/minecraft/class_2828$class_2830")
        jni::constructor<jdouble,jdouble,jdouble,jfloat,jfloat,jboolean> constructor{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF_EX(Manager,"net/minecraft/class_636",maps::ClientPlayerInteractionManager)
        jni::method<jboolean,"method_2923"> breaking{*this};
        jni::method<void,"method_2897",jni::NOT_STATIC,maps::PlayerEntity> stopUsing{*this};
        jni::method<maps::ActionResult,"method_2919",jni::NOT_STATIC,maps::PlayerEntity,maps::Hand> use{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF_EX(Result,"net/minecraft/class_1269",maps::ActionResult)
        jni::method<jboolean,"method_23665"> accepted{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Explosion,"net/minecraft/class_1927")
        jni::method<jfloat,"method_17752",jni::STATIC,maps::Vec3d,maps::Entity> exposure{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(EntityHit,"net/minecraft/class_3966")
        jni::constructor<maps::Entity,maps::Vec3d> constructor{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF_EX(Client,"net/minecraft/class_310",maps::MinecraftClient)
        jni::field<maps::Entity,"field_1692"> targetedEntity{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Bow, "net/minecraft/class_1753")
        jni::method<jfloat, "method_7722", jni::STATIC, jint> getPullProgress{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Crossbow, "net/minecraft/class_1764")
        jni::method<jboolean, "method_7781", jni::STATIC, maps::ItemStack> isCharged{*this};
    END_KLASS_DEF()
}
