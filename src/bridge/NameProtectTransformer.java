package net.onestep.privacy;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Opcodes;
import org.objectweb.asm.Type;
import org.objectweb.asm.tree.*;
import java.util.Set;

/** Only the seven 1.21.1 font drawing/measurement entry points are changed. */
public final class NameProtectTransformer {
    private static final Set<String> METHODS = Set.of("method_27521", "method_27522", "method_22942",
        "method_37296", "method_1727", "method_27525", "method_30880");

    public static byte[] transform(byte[] original) {
        ClassReader reader = new ClassReader(original);
        ClassNode node = new ClassNode(); reader.accept(node, 0);
        if (!node.name.equals("net/minecraft/class_327")) throw new IllegalArgumentException("Wrong class");
        int count = 0;
        for (MethodNode method : node.methods) {
            if (!METHODS.contains(method.name) || (method.access & (Opcodes.ACC_STATIC | Opcodes.ACC_ABSTRACT | Opcodes.ACC_NATIVE)) != 0) continue;
            Type[] args = Type.getArgumentTypes(method.desc);
            if (args.length == 0) continue;
            String type = args[0].getInternalName();
            if (!type.equals("java/lang/String") && !type.equals("net/minecraft/class_5481") && !type.equals("net/minecraft/class_5348")) continue;
            InsnList prefix = new InsnList();
            prefix.add(new VarInsnNode(Opcodes.ALOAD, 1));
            prefix.add(new MethodInsnNode(Opcodes.INVOKESTATIC, "net/onestep/privacy/NameProtectBridge", "filter", "(Ljava/lang/Object;)Ljava/lang/Object;", false));
            prefix.add(new TypeInsnNode(Opcodes.CHECKCAST, type));
            prefix.add(new VarInsnNode(Opcodes.ASTORE, 1));
            method.instructions.insert(prefix); ++count;
        }
        if (count != 7) throw new IllegalArgumentException("Fabric 1.21.1 font entry points did not match");
        // Stack is empty again before original instructions; original frames and maxima remain valid.
        ClassWriter writer = new ClassWriter(reader, 0); node.accept(writer); return writer.toByteArray();
    }
}
