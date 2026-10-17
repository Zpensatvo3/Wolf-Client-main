package net.onestep.visual;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Opcodes;
import org.objectweb.asm.tree.*;

/** The 1.21.11 biome colors are environment attributes, not mutable BiomeEffects fields. */
public final class SkyTransformer {
    public static byte[] transform(byte[] original) {
        ClassReader reader = new ClassReader(original);
        ClassNode node = new ClassNode(); reader.accept(node, 0);
        if (!node.name.equals("net/minecraft/class_758")) throw new IllegalArgumentException("Wrong fog class");
        int changed = 0;
        for (MethodNode method : node.methods) {
            if (!method.name.equals("method_62185") || !method.desc.equals("(Lnet/minecraft/class_4184;FLnet/minecraft/class_638;IF)Lorg/joml/Vector4f;")) continue;
            for (AbstractInsnNode instruction : method.instructions.toArray()) {
                if (instruction.getOpcode() != Opcodes.ARETURN) continue;
                InsnList prefix = new InsnList();
                prefix.add(new MethodInsnNode(Opcodes.INVOKESTATIC, "net/onestep/visual/SkyFogBridge", "filter", "(Ljava/lang/Object;)Ljava/lang/Object;", false));
                prefix.add(new TypeInsnNode(Opcodes.CHECKCAST, "org/joml/Vector4f"));
                method.instructions.insertBefore(instruction, prefix); ++changed;
            }
        }
        if (changed == 0) throw new IllegalArgumentException("1.21.11 fog return sites did not match");
        ClassWriter writer = new ClassWriter(reader, 0); node.accept(writer); return writer.toByteArray();
    }
}
