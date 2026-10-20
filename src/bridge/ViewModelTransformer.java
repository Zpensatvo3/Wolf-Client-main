package net.onestep.visual;
import org.objectweb.asm.*;
import org.objectweb.asm.tree.*;

/** Fail closed unless the exact first-person signature and its push site match. */
public final class ViewModelTransformer {
    public static byte[] transform(byte[] original, int target) {
        if (target!=12101 && target!=12104 && target!=12108 && target!=12111 && target!=26100 && target!=26101 && target!=26102)
            throw new IllegalArgumentException("Unsupported View Model target");
        boolean named=target>=26100;
        String owner=named ? "net/minecraft/client/renderer/ItemInHandRenderer" : "net/minecraft/class_759";
        String pose=named ? "com/mojang/blaze3d/vertex/PoseStack" : "net/minecraft/class_4587";
        String descriptor=named
            ? "(Lnet/minecraft/client/player/AbstractClientPlayer;FFLnet/minecraft/world/InteractionHand;FLnet/minecraft/world/item/ItemStack;FLcom/mojang/blaze3d/vertex/PoseStack;Lnet/minecraft/client/renderer/SubmitNodeCollector;I)V"
            : "(Lnet/minecraft/class_742;FFLnet/minecraft/class_1268;FLnet/minecraft/class_1799;FLnet/minecraft/class_4587;Lnet/minecraft/"+(target>=12111 ? "class_11659" : "class_4597")+";I)V";
        ClassReader reader=new ClassReader(original);
        ClassNode node=new ClassNode(); reader.accept(node,0);
        if (!node.name.equals(owner)) throw new IllegalArgumentException("Wrong held-item renderer");
        int matched=0;
        for (MethodNode method:node.methods) {
            if (!method.name.equals(named ? "renderArmWithItem" : "method_3228") || !method.desc.equals(descriptor)) continue;
            if ((method.access & (Opcodes.ACC_STATIC|Opcodes.ACC_NATIVE|Opcodes.ACC_ABSTRACT))!=0)
                throw new IllegalArgumentException("Unexpected held-item access");
            for (AbstractInsnNode instruction:method.instructions.toArray()) {
                if (!(instruction instanceof MethodInsnNode call) || !call.owner.equals(pose)
                    || !call.name.equals(named ? "pushPose" : "method_22903") || !call.desc.equals("()V")) continue;
                // Verify the receiver really is the caller's matrix parameter.
                AbstractInsnNode previous=instruction.getPrevious();
                while (previous!=null && previous.getOpcode()<0) previous=previous.getPrevious();
                if (!(previous instanceof VarInsnNode load) || load.getOpcode()!=Opcodes.ALOAD || load.var!=8)
                    throw new IllegalArgumentException("Unexpected held-item matrix stack");
                InsnList code=new InsnList();
                for (int local:new int[]{8,4,1,6}) code.add(new VarInsnNode(Opcodes.ALOAD,local));
                code.add(new MethodInsnNode(Opcodes.INVOKESTATIC,"net/onestep/visual/ViewModelBridge","apply",
                    "(Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)V",false));
                method.instructions.insert(instruction,code);
                ++matched; break; // One transform inside vanilla's outer push, never per sub-draw.
            }
        }
        if (matched!=1) throw new IllegalArgumentException("Held-item entry point did not match target "+target);
        ClassWriter writer=new ClassWriter(reader,ClassWriter.COMPUTE_MAXS);
        node.accept(writer); return writer.toByteArray();
    }
}
