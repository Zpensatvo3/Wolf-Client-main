package wolf.vape;

import org.objectweb.asm.*;
import org.objectweb.asm.tree.*;

/** Transport only: all combat decisions stay in the independent C++ engines. */
public final class NativeBridge {
    public static final int ABI_VERSION = 2;
    private static volatile boolean live;
    private static Thread owner;
    public static volatile int installedMask;
    private static native boolean event(int kind, Object self, Object first, Object second);
    public static synchronized boolean dispatch(int kind, Object self, Object first, Object second) {
        return live && event(kind, self, first, second);
    }
    public static synchronized void activate() { owner = Thread.currentThread(); live = true; }
    // Acquiring this monitor also drains callbacks before native DLL shutdown.
    public static synchronized void deactivate() { live = false; }
    public static void quiesce(Object client) throws InterruptedException {
        java.util.concurrent.CountDownLatch done = new java.util.concurrent.CountDownLatch(1);
        Runnable stop = () -> {
            synchronized (NativeBridge.class) {
                try { if (live) event(9, client, null, null); }
                finally { live = false; done.countDown(); }
            }
        };
        if (Thread.currentThread() == owner) stop.run();
        else {
            ((java.util.concurrent.Executor) client).execute(stop);
            done.await(2, java.util.concurrent.TimeUnit.SECONDS);
        }
    }
    public static void warmup() {
        Writer writer = new Writer();
        writer.visit(Opcodes.V17, Opcodes.ACC_PUBLIC, "wolf/vape/Warmup", null, "java/lang/Object", null);
        writer.visitEnd();
        new ClassReader(writer.toByteArray()).accept(new ClassNode(), 0);
    }
    static final class Writer extends ClassWriter {
        Writer() { super(ClassWriter.COMPUTE_FRAMES | ClassWriter.COMPUTE_MAXS); }
        @Override protected ClassLoader getClassLoader() { return NativeBridge.class.getClassLoader(); }
    }
    private static InsnList call(int kind, int first, int second, boolean cancel) {
        InsnList code = new InsnList();
        code.add(new LdcInsnNode(kind));
        code.add(new VarInsnNode(Opcodes.ALOAD, 0));
        code.add(first < 0 ? new InsnNode(Opcodes.ACONST_NULL) : new VarInsnNode(Opcodes.ALOAD, first));
        code.add(second < 0 ? new InsnNode(Opcodes.ACONST_NULL) : new VarInsnNode(Opcodes.ALOAD, second));
        code.add(new MethodInsnNode(Opcodes.INVOKESTATIC, "wolf/vape/NativeBridge", "dispatch",
            "(ILjava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)Z", false));
        if (cancel) {
            LabelNode proceed = new LabelNode();
            code.add(new JumpInsnNode(Opcodes.IFEQ, proceed));
            code.add(new InsnNode(Opcodes.RETURN));
            code.add(proceed);
        } else code.add(new InsnNode(Opcodes.POP));
        return code;
    }
    private static void tail(MethodNode method, int kind, int first, int second) {
        for (AbstractInsnNode instruction : method.instructions.toArray())
            if (instruction.getOpcode() == Opcodes.RETURN)
                method.instructions.insertBefore(instruction, call(kind, first, second, false));
    }
    public static byte[] transform(String name, byte[] original) {
        ClassNode type = new ClassNode();
        new ClassReader(original).accept(type, 0);
        int mask = 0;
        for (MethodNode method : type.methods) {
            if (name.equals("net/minecraft/class_746") && method.name.equals("method_5773") && method.desc.equals("()V")) {
                tail(method, 0, -1, -1); mask |= 1;
            } else if (name.equals("net/minecraft/class_636") && method.name.equals("method_2918") &&
                    method.desc.equals("(Lnet/minecraft/class_1657;Lnet/minecraft/class_1297;)V")) {
                // Add post events before adding the cancel return: cancelled attempts have no post event.
                tail(method, 3, 1, 2);
                method.instructions.insert(call(2, 1, 2, true)); mask |= 2;
            } else if (name.equals("net/minecraft/class_636") && method.name.equals("method_2897") &&
                    method.desc.equals("(Lnet/minecraft/class_1657;)V")) {
                method.instructions.insert(call(5, 1, -1, true)); mask |= 4;
            } else if (name.equals("net/minecraft/class_634") && method.name.equals("method_11132") &&
                    method.desc.equals("(Lnet/minecraft/class_2743;)V")) {
                // Post-velocity is distinct from pre/cancel. A jump performed
                // before vanilla setVelocity would immediately be overwritten.
                tail(method, 8, 1, -1);
                // Netty's invocation must reach vanilla forceMainThread first. Its scheduled client
                // invocation is the only one allowed to read player state or cancel the packet.
                for (AbstractInsnNode instruction : method.instructions.toArray())
                    if (instruction instanceof MethodInsnNode invoke &&
                        invoke.getOpcode() == Opcodes.INVOKESTATIC &&
                        invoke.owner.equals("net/minecraft/class_2600") && invoke.name.equals("method_11074") &&
                        invoke.desc.equals("(Lnet/minecraft/class_2596;Lnet/minecraft/class_2547;Lnet/minecraft/class_1255;)V")) {
                        method.instructions.insert(instruction, call(4, 1, -1, true)); mask |= 8; break;
                    }
            } else if (name.equals("net/minecraft/class_8673") && method.name.equals("method_52787") &&
                    method.desc.equals("(Lnet/minecraft/class_2596;)V")) {
                method.instructions.insert(call(6, 1, -1, true)); mask |= 16;
            } else if (name.equals("net/minecraft/class_757") && method.name.equals("method_3190") &&
                    method.desc.equals("(F)V")) {
                tail(method, 7, -1, -1); mask |= 32;
            }
        }
        int expected = switch (name) {
            case "net/minecraft/class_746" -> 1;
            case "net/minecraft/class_636" -> 6;
            case "net/minecraft/class_634" -> 8;
            case "net/minecraft/class_8673" -> 16;
            case "net/minecraft/class_757" -> 32;
            default -> 0;
        };
        if (mask != expected || mask == 0) throw new IllegalStateException("Vape hook mismatch: " + name);
        Writer writer = new Writer();
        type.accept(writer);
        installedMask |= mask;
        return writer.toByteArray();
    }
}
