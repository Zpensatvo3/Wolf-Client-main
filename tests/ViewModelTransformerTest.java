import java.nio.file.*;
import java.util.jar.*;
import org.objectweb.asm.*;
import org.objectweb.asm.tree.*;
import net.onestep.visual.ViewModelTransformer;
import net.onestep.visual.ViewModelBridge;

/** Classfile-only regression: no Minecraft class is loaded or executed. */
public final class ViewModelTransformerTest {
    public static void main(String[] args) throws Exception {
        int target=Integer.parseInt(args[1]);
        String name=target>=26100 ? "net/minecraft/client/renderer/ItemInHandRenderer.class" : "net/minecraft/class_759.class";
        byte[] original;
        try (JarFile jar=new JarFile(args[0])) { original=jar.getInputStream(jar.getJarEntry(name)).readAllBytes(); }
        byte[] transformed=ViewModelTransformer.transform(original,target);
        ClassNode node=new ClassNode(); new ClassReader(transformed).accept(node,0);
        int calls=0;
        for (MethodNode method:node.methods) for (AbstractInsnNode instruction:method.instructions)
            if (instruction instanceof MethodInsnNode call && call.owner.equals("net/onestep/visual/ViewModelBridge")) {
                if (!call.name.equals("apply") || method.maxStack<4) throw new AssertionError("Invalid transform");
                ++calls;
            }
        if (calls!=1) throw new AssertionError("Exactly one hand-render hook required");
        boolean rejected=false;
        try { ViewModelTransformer.transform(original,99999); } catch (IllegalArgumentException expected) { rejected=true; }
        if (!rejected) throw new AssertionError("Unknown version accepted");
        rejected=false;
        try { ViewModelBridge.configure(new float[]{0,0,0,1,Float.NaN,0,0,1},true); }
        catch (IllegalArgumentException expected) { rejected=true; }
        if (!rejected) throw new AssertionError("Nonfinite transform accepted");
        ViewModelBridge.configure(new float[]{0,0,0,1,0,0,0,1},false);
        ViewModelBridge.apply(new Object(),new Object(),new Object(),new Object()); // Inactive hook is inert.
        ViewModelBridge.stop();
        System.out.println("PASS exact held-item transform and fail-closed configuration: "+target);
    }
}
