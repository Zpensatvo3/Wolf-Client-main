package net.onestep.visual;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

/** Loaded in a fresh child loader; no Minecraft/Fabric compile-time dependency. */
public final class SkyBridge implements InvocationHandler {
    private static volatile boolean active;
    private static volatile boolean shaderBlocked;
    private static Object iris;
    private static Method shaderCheck;
    private static Method projection, position, matrixGet, fogColor, fogStart, fogEnd;
    private final float[] projectionValues = new float[16];
    private final float[] positionValues = new float[16];
    private final float[] fogValues = new float[5];
    private static native void draw(float[] projection, float[] position, float[] fog);

    public static void install(ClassLoader gameLoader) throws ReflectiveOperationException {
        Class<?> context = Class.forName("net.fabricmc.fabric.api.client.rendering.v1.WorldRenderContext", true, gameLoader);
        projection = context.getMethod("projectionMatrix");
        position = context.getMethod("positionMatrix");
        matrixGet = Class.forName("org.joml.Matrix4f", true, gameLoader).getMethod("get", float[].class);
        Class<?> renderSystem = Class.forName("com.mojang.blaze3d.systems.RenderSystem", true, gameLoader);
        fogColor = renderSystem.getMethod("getShaderFogColor");
        fogStart = renderSystem.getMethod("getShaderFogStart");
        fogEnd = renderSystem.getMethod("getShaderFogEnd");
        try {
            Class<?> api = Class.forName("net.irisshaders.iris.api.v0.IrisApi", false, gameLoader);
            iris = api.getMethod("getInstance").invoke(null);
            shaderCheck = api.getMethod("isShaderPackInUse");
        } catch (ClassNotFoundException ignored) { }
        Class<?> events = Class.forName("net.fabricmc.fabric.api.client.rendering.v1.WorldRenderEvents", true, gameLoader);
        Class<?> listener = Class.forName("net.fabricmc.fabric.api.client.rendering.v1.WorldRenderEvents$BeforeEntities", true, gameLoader);
        Object event = events.getField("BEFORE_ENTITIES").get(null);
        Object proxy = Proxy.newProxyInstance(SkyBridge.class.getClassLoader(), new Class<?>[]{listener}, new SkyBridge());
        Class<?> eventType = Class.forName("net.fabricmc.fabric.api.event.Event", true, gameLoader);
        eventType.getMethod("register", Object.class).invoke(event, proxy);
        active = true;
    }

    public static void stop() { active = false; }
    public static boolean shaderBlocked() { return shaderBlocked; }
    public static boolean healthy() { return active; }

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
            matrixGet.invoke(projection.invoke(args[0]), (Object) projectionValues);
            matrixGet.invoke(position.invoke(args[0]), (Object) positionValues);
            // Read the same frame's terrain fog after opaque terrain has rendered.
            // These are copies; never modify RenderSystem's shared color array.
            float[] color = (float[])fogColor.invoke(null);
            System.arraycopy(color, 0, fogValues, 0, 3);
            fogValues[3] = ((Number)fogStart.invoke(null)).floatValue();
            fogValues[4] = ((Number)fogEnd.invoke(null)).floatValue();
            draw(projectionValues, positionValues, fogValues);
        } catch (Throwable failure) {
            // A stale/incompatible bridge must never throw into Minecraft's frame.
            active = false;
        }
        return null;
    }
}
