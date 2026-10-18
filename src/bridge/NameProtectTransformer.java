package net.onestep.privacy;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Opcodes;
import org.objectweb.asm.Type;
import org.objectweb.asm.tree.*;
import java.util.Set;

/** Version-scoped font entry points; refuse an unexpected font implementation. */
public final class NameProtectTransformer {
    private static final Set<String> METHODS = Set.of("method_27521", "method_27522", "method_22942",
        "method_37296", "method_1727", "method_27525", "method_30880");
    private static final Set<String> NAMED_METHODS = Set.of("drawInBatch", "drawInBatch8xOutline", "width", "prepareText");

    public static byte[] transform(byte[] original, int target) {
        int expected = switch (target) {
            case 12101 -> 7;
            case 12104 -> 6;
            case 12108, 12111, 26100, 26101, 26102 -> 8;
            default -> throw new IllegalArgumentException("Unsupported font target");
        };
        ClassReader reader = new ClassReader(original);
        ClassNode node = new ClassNode(); reader.accept(node, 0);
        boolean named = target == 26100 || target == 26101 || target == 26102;
        if (!node.name.equals(named ? "net/minecraft/client/gui/Font" : "net/minecraft/class_327"))
            throw new IllegalArgumentException("Wrong font class for target " + target);
        int count = 0;
        for (MethodNode method : node.methods) {
            boolean prepare = target >= 12108 && (method.name.equals("method_71795") || method.name.equals("method_71796"));
            if (!(named ? NAMED_METHODS.contains(method.name) : METHODS.contains(method.name) || prepare)
                    || (method.access & (Opcodes.ACC_STATIC | Opcodes.ACC_ABSTRACT | Opcodes.ACC_NATIVE)) != 0) continue;
            Type[] args = Type.getArgumentTypes(method.desc);
            if (args.length == 0) continue;
            String type = args[0].getInternalName();
            if (!type.equals("java/lang/String") && !type.equals(named ? "net/minecraft/util/FormattedCharSequence" : "net/minecraft/class_5481")
                    && !type.equals(named ? "net/minecraft/network/chat/FormattedText" : "net/minecraft/class_5348")) continue;
            InsnList prefix = new InsnList();
            prefix.add(new VarInsnNode(Opcodes.ALOAD, 1));
            prefix.add(new MethodInsnNode(Opcodes.INVOKESTATIC, "net/onestep/privacy/NameProtectBridge", "filter", "(Ljava/lang/Object;)Ljava/lang/Object;", false));
            prefix.add(new TypeInsnNode(Opcodes.CHECKCAST, type));
            prefix.add(new VarInsnNode(Opcodes.ASTORE, 1));
            method.instructions.insert(prefix); ++count;
        }
        if (count != expected) throw new IllegalArgumentException("Minecraft font entry points did not match target " + target);
        // Stack is empty again before original instructions; original frames and maxima remain valid.
        ClassWriter writer = new ClassWriter(reader, 0); node.accept(writer); return writer.toByteArray();
    }
}
