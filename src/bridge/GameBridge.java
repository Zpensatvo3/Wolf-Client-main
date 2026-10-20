package net.onestep.hybrid;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.Arrays;

/** Original OneStep game-I/O adapter. No native callbacks, event hooks or worker threads. */
public final class GameBridge {
    public static final int NO_JUMP_DELAY = 1, FAST_STOP = 2;
    private static final int SCHEMA = 1, SIZE = 11;
    private final Field instance, player, world, screen, options, jumpCooldown, hurtTime;
    private final Field input, inputFlags, vectorX, vectorY, velocityX, velocityY, velocityZ, flying;
    private final Field forwardKey, backKey, leftKey, rightKey, jumpKey;
    private final Method onThread, alive, grounded, water, lava, climbing, gliding, riding;
    private final Method abilities, velocity, setVelocity, movement, jumping, pressed;
    private long sequence, jumpSequence, stopSequence;
    private volatile boolean stopped;

    public GameBridge(ClassLoader loader, int target) throws ReflectiveOperationException {
        if (target != 12111 || loader == null)
            throw new IllegalArgumentException("GameBridge requires exact Fabric 1.21.11");
        Class<?> client = load(loader, "class_310"), localPlayer = load(loader, "class_746");
        Class<?> worldType = load(loader, "class_638"), screenType = load(loader, "class_437");
        Class<?> optionsType = load(loader, "class_315"), inputType = load(loader, "class_744");
        Class<?> flagsType = load(loader, "class_10185"), vec2 = load(loader, "class_241");
        Class<?> vec3 = load(loader, "class_243"), abilityType = load(loader, "class_1656");
        Class<?> binding = load(loader, "class_304");
        instance = field(client, "field_1700", client, true);
        player = field(client, "field_1724", localPlayer, false);
        world = field(client, "field_1687", worldType, false);
        screen = field(client, "field_1755", screenType, false);
        options = field(client, "field_1690", optionsType, false);
        onThread = method(client, "method_18854", boolean.class);
        alive = method(localPlayer, "method_5805", boolean.class);
        jumpCooldown = field(localPlayer, "field_6228", int.class, false);
        hurtTime = field(localPlayer, "field_6235", int.class, false);
        grounded = method(localPlayer, "method_24828", boolean.class);
        water = method(localPlayer, "method_5799", boolean.class);
        lava = method(localPlayer, "method_5771", boolean.class);
        climbing = method(localPlayer, "method_6101", boolean.class);
        gliding = method(localPlayer, "method_6128", boolean.class);
        riding = method(localPlayer, "method_5765", boolean.class);
        abilities = method(localPlayer, "method_31549", abilityType);
        velocity = method(localPlayer, "method_18798", vec3);
        setVelocity = method(localPlayer, "method_18800", void.class, double.class, double.class, double.class);
        input = field(localPlayer, "field_3913", inputType, false);
        inputFlags = field(inputType, "field_54155", flagsType, false);
        movement = method(inputType, "method_3128", vec2);
        jumping = method(flagsType, "comp_3163", boolean.class);
        vectorX = field(vec2, "field_1343", float.class, false);
        vectorY = field(vec2, "field_1342", float.class, false);
        velocityX = field(vec3, "field_1352", double.class, false);
        velocityY = field(vec3, "field_1351", double.class, false);
        velocityZ = field(vec3, "field_1350", double.class, false);
        flying = field(abilityType, "field_7479", boolean.class, false);
        forwardKey = field(optionsType, "field_1894", binding, false);
        backKey = field(optionsType, "field_1881", binding, false);
        leftKey = field(optionsType, "field_1913", binding, false);
        rightKey = field(optionsType, "field_1849", binding, false);
        jumpKey = field(optionsType, "field_1903", binding, false);
        pressed = method(binding, "method_1434", boolean.class);
    }

    private static Class<?> load(ClassLoader loader, String name) throws ClassNotFoundException {
        return Class.forName("net.minecraft." + name, false, loader);
    }

    // Match exact names AND descriptors. Inherited private members are allowed;
    // no approximate field-name, arity-only or arbitrary namespace fallbacks.
    private static Field field(Class<?> owner, String name, Class<?> type, boolean isStatic)
            throws ReflectiveOperationException {
        for (Class<?> c = owner; c != null; c = c.getSuperclass()) {
            try {
                Field f = c.getDeclaredField(name);
                if (f.getType() != type || Modifier.isStatic(f.getModifiers()) != isStatic || !f.trySetAccessible())
                    throw new IllegalAccessException("GameBridge field mismatch: " + name);
                return f;
            } catch (NoSuchFieldException missing) { /* Try the actual superclass. */ }
        }
        throw new NoSuchFieldException(name);
    }

    private static Method method(Class<?> owner, String name, Class<?> result, Class<?>... parameters)
            throws ReflectiveOperationException {
        for (Class<?> c = owner; c != null; c = c.getSuperclass()) {
            try {
                Method m = c.getDeclaredMethod(name, parameters);
                if (m.getReturnType() != result || Modifier.isStatic(m.getModifiers()) || !m.trySetAccessible())
                    throw new IllegalAccessException("GameBridge method mismatch: " + name);
                return m;
            } catch (NoSuchMethodException missing) { /* Try the actual superclass. */ }
        }
        throw new NoSuchMethodException(name);
    }

    private static boolean yes(Method method, Object receiver) throws ReflectiveOperationException {
        return receiver != null && (boolean) method.invoke(receiver);
    }

    /** Frame-local {bridge identity, player, world, double[11]}; never retained here. */
    public Object[] capture(int feature) throws ReflectiveOperationException {
        if (stopped || (feature != NO_JUMP_DELAY && feature != FAST_STOP)) return null;
        Object client = instance.get(null);
        // isOnThread is checked before touching the world, player or input.
        if (client == null || !yes(onThread, client)) return null;
        if (feature == NO_JUMP_DELAY) jumpSequence = 0; else stopSequence = 0;
        Object p = player.get(client), w = world.get(client);
        if (p == null || w == null || screen.get(client) != null || !yes(alive, p)) return null;
        double[] data = new double[SIZE];
        data[0] = SCHEMA;
        data[1] = feature;
        int flags = 1; // alive
        if (feature == NO_JUMP_DELAY) {
            data[3] = jumpCooldown.getInt(p);
        } else {
            Object in = input.get(p), opts = options.get(client), ab = abilities.invoke(p);
            Object motion = velocity.invoke(p);
            if (in == null || opts == null || ab == null || motion == null) return null;
            Object axes = movement.invoke(in), keys = inputFlags.get(in);
            Object forward = forwardKey.get(opts), back = backKey.get(opts);
            Object left = leftKey.get(opts), right = rightKey.get(opts), jump = jumpKey.get(opts);
            if (axes == null || keys == null || forward == null || back == null || left == null || right == null || jump == null)
                return null;
            if (yes(grounded, p)) flags |= 2;
            if (yes(water, p)) flags |= 4;
            if (yes(lava, p)) flags |= 8;
            if (yes(climbing, p)) flags |= 16;
            if (yes(gliding, p)) flags |= 32;
            if (yes(riding, p)) flags |= 64;
            if (flying.getBoolean(ab)) flags |= 128;
            if (yes(jumping, keys) || yes(pressed, jump)) flags |= 256;
            if (yes(pressed, forward) || yes(pressed, back) || yes(pressed, left) || yes(pressed, right)) flags |= 512;
            data[4] = hurtTime.getInt(p);
            data[5] = vectorY.getFloat(axes);
            data[6] = vectorX.getFloat(axes);
            data[7] = velocityX.getDouble(motion);
            data[8] = velocityY.getDouble(motion);
            data[9] = velocityZ.getDouble(motion);
        }
        data[2] = flags;
        for (double value : data) if (!Double.isFinite(value)) return null;
        // Integers remain exactly representable on the double-array wire format.
        if (++sequence >= (1L << 53)) { stopped = true; return null; }
        data[10] = sequence;
        if (feature == NO_JUMP_DELAY) jumpSequence = sequence; else stopSequence = sequence;
        return new Object[]{this, p, w, data};
    }

    private double[] consume(Object[] snapshot, int feature) throws ReflectiveOperationException {
        if (stopped || snapshot == null || snapshot.length != 4 || snapshot[0] != this ||
                !(snapshot[3] instanceof double[] data) || data.length != SIZE ||
                data[0] != SCHEMA || data[1] != feature || data[10] <= 0 ||
                data[10] != (feature == NO_JUMP_DELAY ? jumpSequence : stopSequence)) return null;
        Object client = instance.get(null);
        if (client == null || !yes(onThread, client)) return null;
        if (feature == NO_JUMP_DELAY) jumpSequence = 0; else stopSequence = 0;
        return data;
    }

    public boolean clearJump(Object[] snapshot) throws ReflectiveOperationException {
        double[] before = consume(snapshot, NO_JUMP_DELAY);
        if (before == null || before[3] <= 0) return false;
        Object[] now = capture(NO_JUMP_DELAY);
        jumpSequence = 0;
        if (now == null || now[1] != snapshot[1] || now[2] != snapshot[2] ||
                ((double[]) now[3])[3] != before[3]) return false;
        jumpCooldown.setInt(now[1], 0);
        return true;
    }

    public boolean stopMotion(Object[] snapshot) throws ReflectiveOperationException {
        double[] before = consume(snapshot, FAST_STOP);
        if (before == null) return false;
        // The C++ release-edge engine decides WHEN; Java only checks write safety.
        if ((int) before[2] != 3 || before[4] != 0 || before[5] != 0 || before[6] != 0 || before[8] > 0)
            return false;
        Object[] now = capture(FAST_STOP);
        stopSequence = 0;
        if (now == null || now[1] != snapshot[1] || now[2] != snapshot[2]) return false;
        double[] current = (double[]) now[3];
        // Exclude only the sequence number. Reject changed input/physics/hurt state.
        if (!Arrays.equals(before, 0, 10, current, 0, 10)) return false;
        setVelocity.invoke(now[1], 0.0, current[8], 0.0);
        return true;
    }

    public void stop() {
        stopped = true;
        jumpSequence = stopSequence = 0;
    }
}
