import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.util.HashMap;
import java.util.Map;
import java.util.zip.ZipFile;
import org.objectweb.asm.*;
import org.objectweb.asm.commons.ClassRemapper;
import org.objectweb.asm.commons.Remapper;
import org.objectweb.asm.tree.*;
import org.joml.Vector4f;
import net.onestep.privacy.NameProtectTransformer;
import net.onestep.visual.SkyTransformer;
import net.onestep.visual.SkyFogBridge;

/** Inspects local, unmodified Mojang jars; never starts or attaches to Minecraft. */
public final class BridgeCompatibilityTest {
    private final Map<String, String> classes = new HashMap<>(), methods = new HashMap<>(), fields = new HashMap<>();
    private final Map<String, String> official = new HashMap<>();

    private BridgeCompatibilityTest(Path mapping) throws Exception {
        try (ZipFile jar = new ZipFile(mapping.toFile())) {
            String owner = "";
            String text = new String(jar.getInputStream(jar.getEntry("mappings/mappings.tiny")).readAllBytes(), StandardCharsets.UTF_8);
            for (String line : text.split("\n")) {
                String[] parts = line.split("\t");
                if (parts[0].equals("c")) {
                    owner = parts[1]; classes.put(owner, parts[2]); official.put(parts[2], owner);
                } else if (parts.length >= 5 && parts[0].isEmpty()) {
                    String key = owner + "|" + parts[3] + "|" + parts[2];
                    if (parts[1].equals("m")) methods.put(key, parts[4]);
                    if (parts[1].equals("f")) fields.put(key, parts[4]);
                }
            }
        }
    }
    private byte[] read(ZipFile jar, String type) throws Exception {
        String name = official.getOrDefault(type, type);
        byte[] original = jar.getInputStream(jar.getEntry(name + ".class")).readAllBytes();
        ClassWriter output = new ClassWriter(0);
        new ClassReader(original).accept(new ClassRemapper(output, new Remapper(Opcodes.ASM9) {
            @Override public String map(String name) { return classes.getOrDefault(name, name); }
            @Override public String mapMethodName(String owner, String name, String desc) {
                return methods.getOrDefault(owner + "|" + name + "|" + desc, name);
            }
            @Override public String mapFieldName(String owner, String name, String desc) {
                return fields.getOrDefault(owner + "|" + name + "|" + desc, name);
            }
        }), 0);
        return output.toByteArray();
    }
    private static ClassNode node(byte[] bytes) {
        ClassNode result = new ClassNode(); new ClassReader(bytes).accept(result, 0); return result;
    }
    private static long calls(byte[] bytes, String owner, String name) {
        long result = 0;
        for (MethodNode method : node(bytes).methods)
            for (AbstractInsnNode instruction : method.instructions)
                if (instruction instanceof MethodInsnNode call && call.owner.equals(owner) && call.name.equals(name)) ++result;
        return result;
    }
    private static void method(byte[] bytes, String name) {
        if (node(bytes).methods.stream().noneMatch(method -> method.name.equals(name)))
            throw new AssertionError("Missing reflected method " + name);
    }
    private static void field(byte[] bytes, String name) {
        if (node(bytes).fields.stream().noneMatch(field -> field.name.equals(name)))
            throw new AssertionError("Missing reflected field " + name);
    }
    private static void fogBehavior() throws Exception {
        SkyFogBridge.prepare(BridgeCompatibilityTest.class.getClassLoader());
        Vector4f original = new Vector4f(.2f, .3f, .4f, .8f);
        SkyFogBridge.configure(0, 0xff0000, 1, true, true);
        Vector4f filtered = (Vector4f)SkyFogBridge.filter(original);
        if (Math.abs(filtered.x - .8f) > .0001f || Math.abs(filtered.y - .075f) > .0001f || filtered.w != original.w)
            throw new AssertionError("Fog blending or alpha changed");
        if (original.x != .2f || filtered == original) throw new AssertionError("Shared vanilla fog vector was mutated");
        SkyFogBridge.configure(0, 0xff0000, 1, false, true);
        if (SkyFogBridge.filter(original) != original) throw new AssertionError("Disabled fog matching changed fog");
        SkyFogBridge.stop();
        if (SkyFogBridge.filter(original) != original || SkyFogBridge.filter(null) != null)
            throw new AssertionError("Stopped bridge must pass through");
    }
    private static void uniformLayout(ZipFile client) throws Exception {
        String fog = new String(client.getInputStream(client.getEntry("assets/minecraft/shaders/include/fog.glsl")).readAllBytes(), StandardCharsets.UTF_8).replaceAll("\\s+", "");
        if (!fog.contains("uniformFog{vec4FogColor;floatFogEnvironmentalStart;floatFogEnvironmentalEnd;floatFogRenderDistanceStart;floatFogRenderDistanceEnd;"))
            throw new AssertionError("Fog uniform offsets changed");
        String projection = new String(client.getInputStream(client.getEntry("assets/minecraft/shaders/include/projection.glsl")).readAllBytes(), StandardCharsets.UTF_8).replaceAll("\\s+", "");
        if (!projection.contains("uniformProjection{mat4ProjMat;")) throw new AssertionError("Projection uniform layout changed");
    }
    private static void mouseOrder(byte[] bytes) {
        int checked = 0;
        for (MethodNode method : node(bytes).methods) {
            if (!method.name.equals("<init>") || !method.desc.equals("(II)V")) continue;
            for (AbstractInsnNode instruction : method.instructions) {
                if (!(instruction instanceof FieldInsnNode field) || field.getOpcode() != Opcodes.PUTFIELD) continue;
                int expected = field.name.equals("comp_4801") ? 1 : field.name.equals("comp_4797") ? 2 : 0;
                if (expected == 0) continue;
                AbstractInsnNode load = field.getPrevious();
                while (load != null && load.getOpcode() < 0) load = load.getPrevious();
                if (!(load instanceof VarInsnNode variable) || variable.getOpcode() != Opcodes.ILOAD || variable.var != expected)
                    throw new AssertionError("MouseInput constructor argument order changed");
                ++checked;
            }
        }
        if (checked != 2) throw new AssertionError("MouseInput button/modifiers fields did not match");
    }
    public static void main(String[] args) throws Exception {
        if (args.length != 3) throw new IllegalArgumentException("client.jar intermediary-v2.jar target-id");
        int target = Integer.parseInt(args[2]);
        BridgeCompatibilityTest test = new BridgeCompatibilityTest(Path.of(args[1]));
        try (ZipFile client = new ZipFile(args[0])) {
            byte[] font = test.read(client, "net/minecraft/class_327");
            byte[] changed = NameProtectTransformer.transform(font, target);
            int expected = target == 12101 ? 7 : target == 12104 ? 6 : 8;
            if (calls(changed, "net/onestep/privacy/NameProtectBridge", "filter") != expected)
                throw new AssertionError("Wrong font entry point count");
            try { NameProtectTransformer.transform(font, 0); throw new AssertionError("Unknown target accepted"); }
            catch (IllegalArgumentException expectedFailure) { }
            byte[] render = test.read(client, "com/mojang/blaze3d/systems/RenderSystem");
            if (target == 12101) {
                method(render, "getShaderFogColor"); method(render, "getShaderFogStart"); method(render, "getShaderFogEnd");
            } else if (target == 12104) {
                method(render, "getShaderFog");
                byte[] fog = test.read(client, "net/minecraft/class_9958");
                for (String component : new String[]{"comp_3012", "comp_3013", "comp_3014", "comp_3009", "comp_3010"}) method(fog, component);
            } else {
                method(render, "getShaderFog");
                byte[] slice = test.read(client, "com/mojang/blaze3d/buffers/GpuBufferSlice");
                method(slice, "buffer"); method(slice, "offset"); method(slice, "length");
                field(test.read(client, "net/minecraft/class_10859"), "field_57842");
                uniformLayout(client);
            }
            if (target >= 12104) {
                if (node(test.read(client, "net/minecraft/class_10444")).methods.stream()
                        .noneMatch(method -> method.name.equals("<init>") && method.desc.equals("()V")))
                    throw new AssertionError("ItemRenderState default constructor missing");
            }
            if (target >= 12111) {
                mouseOrder(test.read(client, "net/minecraft/class_11910"));
                method(render, "getProjectionMatrixBuffer"); method(render, "getModelViewMatrix");
                field(test.read(client, "net/minecraft/class_11658"), "field_63087");
                field(test.read(client, "net/minecraft/class_12076"), "field_63097");
                byte[] fog = test.read(client, "net/minecraft/class_758");
                byte[] transformed = SkyTransformer.transform(fog);
                if (calls(transformed, "net/onestep/visual/SkyFogBridge", "filter") == 0)
                    throw new AssertionError("No fog return sites transformed");
                fogBehavior();
            }
            System.out.println("PASS target " + target + ": real Minecraft font, render/fog contracts and transformations");
        }
    }
}
