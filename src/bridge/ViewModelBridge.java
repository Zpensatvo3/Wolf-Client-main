package net.onestep.visual;

import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.util.Arrays;

/** Render-only transforms inside vanilla's own push/pop. Never changes player/item state. */
public final class ViewModelBridge {
    private static volatile boolean active, broken;
    private static volatile float[] values = {0,0,0,1,0,0,0,1};
    private static Method translate, scale, multiply, rotate, mainArm, empty;
    private static Constructor<?> quaternion;
    private static Object mainHand, rightArm;
    private static final ThreadLocal<Object> rotations = new ThreadLocal<>();

    public static void prepare(ClassLoader loader, int target) throws ReflectiveOperationException {
        boolean named = target >= 26100;
        Class<?> matrix = Class.forName(named ? "com.mojang.blaze3d.vertex.PoseStack" : "net.minecraft.class_4587", false, loader);
        Class<?> player = Class.forName(named ? "net.minecraft.world.entity.LivingEntity" : "net.minecraft.class_1309", false, loader);
        Class<?> stack = Class.forName(named ? "net.minecraft.world.item.ItemStack" : "net.minecraft.class_1799", false, loader);
        Class<?> hand = Class.forName(named ? "net.minecraft.world.InteractionHand" : "net.minecraft.class_1268", false, loader);
        Class<?> arm = Class.forName(named ? "net.minecraft.world.entity.HumanoidArm" : "net.minecraft.class_1306", false, loader);
        Class<?> q = Class.forName("org.joml.Quaternionf", false, loader);
        Class<?> qc = Class.forName("org.joml.Quaternionfc", false, loader);
        translate = matrix.getMethod(named ? "translate" : "method_22904", double.class,double.class,double.class);
        scale = matrix.getMethod(named ? "scale" : "method_22905", float.class,float.class,float.class);
        multiply = matrix.getMethod(named ? "mulPose" : "method_22907", qc);
        rotate = q.getMethod("rotationXYZ",float.class,float.class,float.class);
        quaternion = q.getConstructor();
        mainArm = player.getMethod(named ? "getMainArm" : "method_6068");
        empty = stack.getMethod(named ? "isEmpty" : "method_7960");
        mainHand = hand.getField(named ? "MAIN_HAND" : "field_5808").get(null);
        rightArm = arm.getField(named ? "RIGHT" : "field_6183").get(null);
        broken=false; active=false;
    }

    public static void configure(float[] config, boolean enabled) {
        if (config == null || config.length != 8) throw new IllegalArgumentException("View Model config");
        for (float value : config) if (!Float.isFinite(value)) throw new IllegalArgumentException("Nonfinite View Model config");
        float[] next = config.clone();
        next[0] = Math.max(-.8f,Math.min(.8f,next[0])); next[1] = Math.max(-.8f,Math.min(.8f,next[1]));
        next[2] = Math.max(-.8f,Math.min(.8f,next[2])); next[3] = Math.max(.4f,Math.min(1.5f,next[3]));
        for (int i=4;i<7;++i) next[i] = Math.max(-90,Math.min(90,next[i]));
        if (!Arrays.equals(values,next)) values=next;
        active=enabled && !broken;
    }
    public static boolean healthy() { return !broken; }
    public static void stop() { active=false; rotations.remove(); }

    public static void apply(Object matrix, Object hand, Object player, Object stack) {
        if (!active || matrix == null || player == null || stack == null) return;
        float[] config=values;
        if (config[7] == 0 && hand != mainHand) return;
        try {
            if (Boolean.TRUE.equals(empty.invoke(stack))) return; // Bare arms remain vanilla.
            boolean right=mainArm.invoke(player)==rightArm;
            if (hand!=mainHand) right=!right;
            float mirror=right ? 1 : -1;
            Object rotation=rotations.get();
            if (rotation==null) { rotation=quaternion.newInstance(); rotations.set(rotation); }
            float radians=(float)(Math.PI/180);
            rotate.invoke(rotation,config[4]*radians,config[5]*radians*mirror,config[6]*radians*mirror);
            translate.invoke(matrix,(double)(config[0]*mirror),(double)config[1],(double)config[2]);
            multiply.invoke(matrix,rotation);
            scale.invoke(matrix,config[3],config[3],config[3]);
        } catch (Throwable failure) { active=false; broken=true; }
    }
}
