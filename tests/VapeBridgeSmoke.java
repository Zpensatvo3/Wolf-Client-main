import org.objectweb.asm.*;
import org.objectweb.asm.tree.*;
import wolf.vape.NativeBridge;

/** One offline ABI/bytecode smoke check; never attaches to Minecraft. */
public final class VapeBridgeSmoke {
    static byte[] sample(String name, String[][] methods) {
        return sample(name, methods, "method_11074",
            "(Lnet/minecraft/class_2596;Lnet/minecraft/class_2547;Lnet/minecraft/class_1255;)V");
    }
    static byte[] sample(String name, String[][] methods, String gateName, String gateDescriptor) {
        ClassWriter writer=new ClassWriter(0);
        writer.visit(Opcodes.V17,Opcodes.ACC_PUBLIC,name,null,"java/lang/Object",null);
        for(String[] signature:methods) {
            MethodVisitor method=writer.visitMethod(Opcodes.ACC_PUBLIC,signature[0],signature[1],null,null);
            method.visitCode();
            if(signature[0].equals("method_11132")) {
                method.visitInsn(Opcodes.ACONST_NULL);method.visitInsn(Opcodes.ACONST_NULL);method.visitInsn(Opcodes.ACONST_NULL);
                // Independent fixture: Yarn 1.21.1 NetworkThreadUtils.forceMainThread
                // (Packet, PacketListener, ThreadExecutor), not a copied bridge constant.
                method.visitMethodInsn(Opcodes.INVOKESTATIC,"net/minecraft/class_2600",gateName,
                    gateDescriptor,false);
            }
            method.visitInsn(Opcodes.RETURN);method.visitMaxs(3,4);method.visitEnd();
        }
        writer.visitEnd();return writer.toByteArray();
    }
    static void rejectGate(String name, String descriptor) {
        try {
            NativeBridge.transform("net/minecraft/class_634", sample("net/minecraft/class_634",
                new String[][]{{"method_11132","(Lnet/minecraft/class_2743;)V"}}, name, descriptor));
            throw new AssertionError("incorrect main-thread gate was accepted: " + name + descriptor);
        } catch (IllegalStateException expected) {}
    }
    static void verify(String name,String[][] methods,int expectedCalls) {
        byte[] output=NativeBridge.transform(name,sample(name,methods));
        ClassNode type=new ClassNode();new ClassReader(output).accept(type,0);
        int calls=0;
        for(MethodNode method:type.methods)for(AbstractInsnNode instruction:method.instructions)
            if(instruction instanceof MethodInsnNode invoke&&invoke.owner.equals("wolf/vape/NativeBridge"))++calls;
        if(calls!=expectedCalls)throw new AssertionError(name+": "+calls);
    }
    public static void main(String[] args) {
        NativeBridge.warmup();
        verify("net/minecraft/class_746",new String[][]{{"method_5773","()V"}},1);
        verify("net/minecraft/class_636",new String[][]{
            {"method_2918","(Lnet/minecraft/class_1657;Lnet/minecraft/class_1297;)V"},
            {"method_2897","(Lnet/minecraft/class_1657;)V"}},3);
        verify("net/minecraft/class_634",new String[][]{{"method_11132","(Lnet/minecraft/class_2743;)V"}},2);
        verify("net/minecraft/class_8673",new String[][]{{"method_52787","(Lnet/minecraft/class_2596;)V"}},1);
        verify("net/minecraft/class_757",new String[][]{{"method_3190","(F)V"}},1);
        if(NativeBridge.installedMask!=63)throw new AssertionError("incomplete hook mask");
        try {
            NativeBridge.transform("net/minecraft/class_634",sample("net/minecraft/class_634",new String[][]{}));
            throw new AssertionError("missing hook was accepted");
        } catch(IllegalStateException expected) {}
        rejectGate("method_11054",
            "(Lnet/minecraft/class_2596;Lnet/minecraft/class_2547;Lnet/minecraft/class_1255;)V");
        rejectGate("method_11074", "(Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)V");
        if(NativeBridge.dispatch(0,null,null,null))throw new AssertionError("inactive native dispatch");
        System.out.println("Vape bridge ABI smoke passed (offline; no game attached).");
    }
}
