package net.onestep.visual;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

/** No Minecraft compile-time dependency. Installed only for the binary's exact target. */
public final class SkyBridge implements InvocationHandler {
    private static volatile boolean active, shaderBlocked;
    private static Object iris;
    private static Method shaderCheck, projection, position, matrixGet, fogColor, fogStart, fogEnd;
    private static Method projectionBuffer, shaderFog, modelView, sliceBuffer, sliceOffset, sliceLength, worldState;
    private static Method fogRecord;
    private static Method[] recordFog;
    private static Field bufferId, skyState, skyColor;
    private static volatile float[] colors = {0, 0, 0, 0, 0};
    private final float[] projectionValues = new float[16], positionValues = new float[16];
    private final float[] fogValues = new float[5], fogUniform = new float[8];
    private static native void draw(float[] projection, float[] position, float[] fog);
    private static native boolean readUniform(int buffer, long offset, long available, float[] values);

    public static void install(ClassLoader loader, int target) throws ReflectiveOperationException {
        if (target != 12101 && target != 12104 && target != 12108 && target != 12111)
            throw new IllegalArgumentException("Unsupported Sky target");
        String api = "net.fabricmc.fabric.api.client.rendering.v1." + (target >= 12111 ? "world." : "");
        Class<?> renderSystem = Class.forName("com.mojang.blaze3d.systems.RenderSystem", true, loader);
        Class<?> matrix = Class.forName("org.joml.Matrix4f", true, loader);
        matrixGet = matrix.getMethod("get", float[].class);
        if (target < 12111) {
            Class<?> context = Class.forName(api + "WorldRenderContext", true, loader);
            projection = context.getMethod("projectionMatrix");
            position = context.getMethod("positionMatrix");
        } else {
            projectionBuffer = renderSystem.getMethod("getProjectionMatrixBuffer");
            modelView = renderSystem.getMethod("getModelViewMatrix");
            Class<?> context = Class.forName(api + "AbstractWorldRenderContext", true, loader);
            worldState = context.getMethod("worldState");
            skyState = Class.forName("net.minecraft.class_11658", true, loader).getField("field_63087");
            skyColor = Class.forName("net.minecraft.class_12076", true, loader).getField("field_63097");
        }
        if (target == 12101) {
            fogColor = renderSystem.getMethod("getShaderFogColor");
            fogStart = renderSystem.getMethod("getShaderFogStart");
            fogEnd = renderSystem.getMethod("getShaderFogEnd");
        } else if (target == 12104) {
            fogRecord = renderSystem.getMethod("getShaderFog");
            Class<?> fog = Class.forName("net.minecraft.class_9958", true, loader);
            recordFog = new Method[]{fog.getMethod("comp_3012"), fog.getMethod("comp_3013"),
                fog.getMethod("comp_3014"), fog.getMethod("comp_3009"), fog.getMethod("comp_3010")};
        } else {
            shaderFog = renderSystem.getMethod("getShaderFog");
            Class<?> slice = Class.forName("com.mojang.blaze3d.buffers.GpuBufferSlice", true, loader);
            sliceBuffer = slice.getMethod("buffer");
            sliceOffset = slice.getMethod("offset"); sliceLength = slice.getMethod("length");
            bufferId = Class.forName("net.minecraft.class_10859", true, loader).getDeclaredField("field_57842");
            bufferId.setAccessible(true);
        }
        try {
            Class<?> irisApi = Class.forName("net.irisshaders.iris.api.v0.IrisApi", false, loader);
            iris = irisApi.getMethod("getInstance").invoke(null);
            shaderCheck = irisApi.getMethod("isShaderPackInUse");
        } catch (ClassNotFoundException ignored) { }
        SkyBridge handler = new SkyBridge();
        register(loader, api, "BEFORE_ENTITIES", "BeforeEntities", handler);
        if (target >= 12111) register(loader, api, "END_EXTRACTION", "EndExtraction", handler);
        active = true;
    }

    private static void register(ClassLoader loader, String api, String field, String listener, SkyBridge handler)
            throws ReflectiveOperationException {
        Class<?> events = Class.forName(api + "WorldRenderEvents", true, loader);
        Class<?> type = Class.forName(api + "WorldRenderEvents$" + listener, true, loader);
        Object proxy = Proxy.newProxyInstance(SkyBridge.class.getClassLoader(), new Class<?>[]{type}, handler);
        Object event = events.getField(field).get(null);
        Class.forName("net.fabricmc.fabric.api.event.Event", true, loader).getMethod("register", Object.class).invoke(event, proxy);
    }

    public static void configureColors(int sky, int fog, float strength, boolean matchFog, boolean enabled) {
        float[] previous = colors;
        if (previous[0] == (enabled ? 1 : 0) && previous[1] == sky && previous[2] == fog
                && previous[3] == strength && previous[4] == (matchFog ? 1 : 0)) return;
        colors = new float[]{enabled ? 1 : 0, sky, fog, strength, matchFog ? 1 : 0};
    }
    public static void stop() { active = false; colors = new float[]{0, 0, 0, 0, 0}; }
    public static boolean shaderBlocked() { return shaderBlocked; }
    public static boolean healthy() { return active; }

    private boolean readSlice(Object slice, float[] values) throws ReflectiveOperationException {
        if (slice == null) return false;
        Object buffer = sliceBuffer.invoke(slice);
        return buffer != null && readUniform(bufferId.getInt(buffer),
            ((Number)sliceOffset.invoke(slice)).longValue(), ((Number)sliceLength.invoke(slice)).longValue(), values);
    }
    private static int blend(int original, int target, float strength) {
        int result = 0;
        for (int shift : new int[]{16, 8, 0}) {
            int channel = Math.round(((original>>shift)&255)*(1-strength) + ((target>>shift)&255)*strength);
            result |= Math.max(0, Math.min(255, channel)) << shift;
        }
        return original & 0xff000000 | result;
    }

    @Override public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
        if (method.getDeclaringClass() == Object.class) {
            if (method.getName().equals("hashCode")) return System.identityHashCode(proxy);
            if (method.getName().equals("equals")) return proxy == args[0];
            return "OnestepSky";
        }
        if (!active || args == null || args.length != 1) return null;
        try {
            shaderBlocked = shaderCheck != null && Boolean.TRUE.equals(shaderCheck.invoke(iris));
            if (shaderBlocked) return null;
            if (method.getName().equals("endExtraction")) {
                float[] config = colors;
                if (config[0] != 0) {
                    Object state = skyState.get(worldState.invoke(args[0]));
                    skyColor.setInt(state, blend(skyColor.getInt(state), (int)config[1], config[3]));
                }
                return null;
            }
            if (projectionBuffer != null) {
                if (!readSlice(projectionBuffer.invoke(null), projectionValues)) return null;
                matrixGet.invoke(modelView.invoke(null), (Object)positionValues);
            } else {
                matrixGet.invoke(projection.invoke(args[0]), (Object)projectionValues);
                matrixGet.invoke(position.invoke(args[0]), (Object)positionValues);
            }
            if (fogRecord != null) {
                Object fog = fogRecord.invoke(null);
                for (int index = 0; index < 5; ++index)
                    fogValues[index] = ((Number)recordFog[index].invoke(fog)).floatValue();
            } else if (shaderFog != null) {
                if (!readSlice(shaderFog.invoke(null), fogUniform)) return null;
                System.arraycopy(fogUniform, 0, fogValues, 0, 3);
                fogValues[3] = Math.min(fogUniform[4], fogUniform[6]);
                fogValues[4] = Math.min(fogUniform[5], fogUniform[7]);
            } else {
                float[] color = (float[])fogColor.invoke(null);
                System.arraycopy(color, 0, fogValues, 0, 3);
                fogValues[3] = ((Number)fogStart.invoke(null)).floatValue();
                fogValues[4] = ((Number)fogEnd.invoke(null)).floatValue();
            }
            draw(projectionValues, positionValues, fogValues);
        } catch (Throwable failure) {
            active = false;
        }
        return null;
    }
}
