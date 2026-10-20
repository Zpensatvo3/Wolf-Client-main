package net.onestep.hybrid;

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.List;

/** Original OneStep world/macro I/O. Geometry, thresholds and pacing stay native. */
public final class WorldBridge {
    private final ClassLoader loader;
    private final Class<?> clientType, playerType;
    private final Field instance, player, world, screen, options, manager;
    private final Method onThread, alive;
    private final boolean[] prepared = new boolean[3];
    private Field sneak, bound, keySymbol, scanCode, mouse, vx, vz, minX, minY, minZ, maxX, maxZ;
    private Field selected, main, pearlItem, cooldown;
    private Method pressed, setPressed, code, category, ground, velocity, bounds, mainHand, empty, item, count, copy, same;
    private Method blockState, air, inventory, sync, use, usingItem;
    private Constructor<?> position;
    private Class<?> blockItem;
    private Object[] sneakOwner, pearlOwner;
    private long sequence, bridgeNonce, pearlNonce;
    private volatile boolean stopped;

    public WorldBridge(ClassLoader loader, int target) throws ReflectiveOperationException {
        if (loader == null || target != 12111) throw new IllegalArgumentException("WorldBridge requires Fabric 1.21.11");
        this.loader = loader; clientType = load("class_310"); playerType = load("class_746");
        instance = field(clientType, "field_1700", clientType, true);
        player = field(clientType, "field_1724", playerType, false);
        world = field(clientType, "field_1687", load("class_638"), false);
        screen = field(clientType, "field_1755", load("class_437"), false);
        options = field(clientType, "field_1690", load("class_315"), false);
        manager = field(clientType, "field_1761", load("class_636"), false);
        onThread = method(clientType, "method_18854", boolean.class);
        alive = method(playerType, "method_5805", boolean.class);
    }
    private Class<?> load(String name) throws ClassNotFoundException { return Class.forName("net.minecraft." + name, false, loader); }
    private static Field field(Class<?> owner, String name, Class<?> type, boolean stat) throws ReflectiveOperationException {
        for (Class<?> c = owner; c != null; c = c.getSuperclass()) try {
            Field f = c.getDeclaredField(name);
            if (f.getType() != type || Modifier.isStatic(f.getModifiers()) != stat || !f.trySetAccessible()) throw new IllegalAccessException(name);
            return f;
        } catch (NoSuchFieldException missing) { /* Exact inherited mapping only. */ }
        throw new NoSuchFieldException(name);
    }
    private static Method method(Class<?> owner, String name, Class<?> result, Class<?>... args) throws ReflectiveOperationException {
        for (Class<?> c = owner; c != null; c = c.getSuperclass()) try {
            Method m = c.getDeclaredMethod(name, args);
            if (m.getReturnType() != result || Modifier.isStatic(m.getModifiers()) || !m.trySetAccessible()) throw new IllegalAccessException(name);
            return m;
        } catch (NoSuchMethodException missing) { /* Exact inherited mapping only. */ }
        throw new NoSuchMethodException(name);
    }
    public void prepare(int feature) throws ReflectiveOperationException {
        if (stopped || feature < 1 || feature > 2) throw new IllegalStateException("World feature/lifecycle");
        if (prepared[feature]) return;
        Class<?> stack = load("class_1799");
        empty = method(stack, "method_7960", boolean.class); item = method(stack, "method_7909", load("class_1792"));
        count = method(stack, "method_7947", int.class); copy = method(stack, "method_7972", stack);
        same = stack.getDeclaredMethod("method_31577", stack, stack);
        if (same.getReturnType() != boolean.class || !Modifier.isStatic(same.getModifiers()) || !same.trySetAccessible()) throw new IllegalAccessException("stack comparison");
        if (feature == 1) {
            Class<?> binding = load("class_304"), input = load("class_3675$class_306"), type = load("class_3675$class_307");
            sneak = field(load("class_315"), "field_1832", binding, false);
            bound = field(binding, "field_1655", input, false);
            pressed = method(binding, "method_1434", boolean.class); setPressed = method(binding, "method_23481", void.class, boolean.class);
            code = method(input, "method_1444", int.class); category = method(input, "method_1442", type);
            keySymbol = field(type, "field_1668", type, true); scanCode = field(type, "field_1671", type, true); mouse = field(type, "field_1672", type, true);
            ground = method(playerType, "method_24828", boolean.class);
            velocity = method(playerType, "method_18798", load("class_243")); bounds = method(playerType, "method_5829", load("class_238"));
            vx = field(load("class_243"), "field_1352", double.class, false); vz = field(load("class_243"), "field_1350", double.class, false);
            Class<?> box = load("class_238");
            minX = field(box, "field_1323", double.class, false); minY = field(box, "field_1322", double.class, false);
            minZ = field(box, "field_1321", double.class, false); maxX = field(box, "field_1320", double.class, false); maxZ = field(box, "field_1324", double.class, false);
            mainHand = method(playerType, "method_6047", stack); blockItem = load("class_1747");
            Class<?> pos = load("class_2338"); position = pos.getDeclaredConstructor(int.class, int.class, int.class);
            if (!position.trySetAccessible()) throw new IllegalAccessException("BlockPos constructor");
            blockState = method(load("class_638"), "method_8320", load("class_2680"), pos);
            air = method(load("class_2680"), "method_26215", boolean.class);
        } else {
            Class<?> inv = load("class_1661"); inventory = method(playerType, "method_31548", inv);
            selected = field(inv, "field_7545", int.class, false); main = field(inv, "field_7547", load("class_2371"), false);
            pearlItem = field(load("class_1802"), "field_8634", load("class_1792"), true);
            sync = method(load("class_636"), "method_2911", void.class);
            cooldown = field(clientType, "field_1752", int.class, false); use = method(clientType, "method_1583", void.class);
            usingItem = method(playerType, "method_6115", boolean.class);
        }
        prepared[feature] = true;
    }
    private Object client() throws ReflectiveOperationException {
        if (stopped) return null;
        Object c = instance.get(null); return c != null && (boolean) onThread.invoke(c) ? c : null;
    }
    private boolean live(Object c, Object p, Object w) throws ReflectiveOperationException {
        return p != null && w != null && screen.get(c) == null && (boolean) alive.invoke(p);
    }
    private long next() { if (++sequence >= (1L << 53)) { stop(); return 0; } return sequence; }
    private boolean equal(Object a, Object b) throws ReflectiveOperationException {
        return a != null && b != null && (int) count.invoke(a) == (int) count.invoke(b) && (boolean) same.invoke(null, a, b);
    }
    /** {owner,p,w,options,key,boundKey,stackCopy,double[13]}. */
    public Object[] captureBridge() throws ReflectiveOperationException {
        Object c = client(); if (c == null) return null;
        bridgeNonce = 0; prepare(1);
        Object p = player.get(c), w = world.get(c), opts = options.get(c);
        if (!live(c, p, w) || opts == null) return null;
        Object key = sneak.get(opts), input = key == null ? null : bound.get(key);
        Object v = velocity.invoke(p), b = bounds.invoke(p), stack = mainHand.invoke(p);
        if (key == null || input == null || v == null || b == null || stack == null) return null;
        boolean block = !(boolean) empty.invoke(stack) && (int) count.invoke(stack) > 0 && blockItem.isInstance(item.invoke(stack));
        boolean owned = sneakOwner != null && sneakOwner[0] == p && sneakOwner[1] == w && sneakOwner[2] == opts && sneakOwner[3] == key && sneakOwner[4] == input;
        double[] data = {1, bridgeNonce = next(), vx.getDouble(v), vz.getDouble(v), minX.getDouble(b), minY.getDouble(b), minZ.getDouble(b),
            maxX.getDouble(b), maxZ.getDouble(b), block ? 1 : 0, (boolean) pressed.invoke(key) ? 1 : 0, (boolean) ground.invoke(p) ? 1 : 0, owned ? 1 : 0};
        for (double value : data) if (!Double.isFinite(value)) return null;
        return stopped ? null : new Object[]{this, p, w, opts, key, input, copy.invoke(stack), data};
    }
    private boolean validBridge(Object[] packet) throws ReflectiveOperationException {
        Object c = client();
        if (c == null || packet == null || packet.length != 8 || packet[0] != this || !(packet[7] instanceof double[] data) ||
            data.length != 13 || data[0] != 1 || data[1] <= 0 || data[1] != bridgeNonce) return false;
        Object p = player.get(c), w = world.get(c), opts = options.get(c);
        if (!live(c, p, w) || p != packet[1] || w != packet[2] || opts != packet[3] ||
            sneak.get(opts) != packet[4] || bound.get(packet[4]) != packet[5] || !equal(mainHand.invoke(p), packet[6])) return false;
        Object v = velocity.invoke(p), b = bounds.invoke(p);
        return (boolean) ground.invoke(p) && vx.getDouble(v) == data[2] && vz.getDouble(v) == data[3] &&
            minX.getDouble(b) == data[4] && minY.getDouble(b) == data[5] && minZ.getDouble(b) == data[6] &&
            maxX.getDouble(b) == data[7] && maxZ.getDouble(b) == data[8];
    }
    public int[] support(Object[] packet, int[] samples) throws ReflectiveOperationException {
        if (!validBridge(packet) || samples == null || samples.length != 24) return null;
        int[] result = new int[8];
        for (int i = 0; i < 8; ++i) {
            Object state = blockState.invoke(packet[2], position.newInstance(samples[i * 3], samples[i * 3 + 1], samples[i * 3 + 2]));
            if (state == null) return null;
            result[i] = (boolean) air.invoke(state) ? 1 : 0;
        }
        return result;
    }
    public boolean forceSneak(Object[] packet) throws ReflectiveOperationException {
        if (!validBridge(packet)) return false;
        bridgeNonce = 0;
        if (sneakOwner == null && (boolean) pressed.invoke(packet[4])) return true; // Manual hold is never claimed.
        sneakOwner = new Object[]{packet[1], packet[2], packet[3], packet[4], packet[5]};
        setPressed.invoke(packet[4], true); return true;
    }
    private int[] descriptor(Object input) throws ReflectiveOperationException {
        if (input == null) return new int[]{-1, -1};
        Object type = category.invoke(input);
        int kind = type == keySymbol.get(null) ? 0 : type == scanCode.get(null) ? 1 : type == mouse.get(null) ? 2 : -1;
        return new int[]{kind, (int) code.invoke(input)};
    }
    public int[] sneakInput() throws ReflectiveOperationException {
        if (sneakOwner == null) return null;
        Object c = client(); if (c == null) return null;
        Object opts = options.get(c), key = opts == null ? null : sneak.get(opts);
        return descriptor(key == null ? null : bound.get(key));
    }
    public boolean releaseSneak(int kind, int keyCode, boolean physical) throws ReflectiveOperationException {
        bridgeNonce = 0;
        if (sneakOwner == null) return true;
        Object c = client(); if (c == null) return false;
        Object opts = options.get(c), key = opts == null ? null : sneak.get(opts);
        if (player.get(c) == sneakOwner[0] && world.get(c) == sneakOwner[1] && opts == sneakOwner[2] && key == sneakOwner[3]) {
            int[] input = descriptor(bound.get(key));
            if (input[0] != kind || input[1] != keyCode) return false;
            if (!physical && (boolean) pressed.invoke(key)) setPressed.invoke(key, false);
        }
        sneakOwner = null; return true;
    }
    /** {owner,p,w,inventory,manager,stackCopies[9],double[13]:schema,nonce,selected,owned,pearlFlags[9]}. */
    public Object[] capturePearl() throws ReflectiveOperationException {
        Object c = client(); if (c == null) return null;
        pearlNonce = 0; prepare(2);
        Object p = player.get(c), w = world.get(c), m = manager.get(c);
        if (!live(c, p, w) || m == null || (boolean) usingItem.invoke(p)) return null;
        Object inv = inventory.invoke(p);
        if (inv == null || !(main.get(inv) instanceof List<?> stacks) || stacks.size() < 9) return null;
        int slot = selected.getInt(inv); if (slot < 0 || slot > 8) return null;
        boolean owned = pearlOwner != null && pearlOwner[0] == p && pearlOwner[1] == w && pearlOwner[2] == inv && pearlOwner[3] == m && (int) pearlOwner[5] == slot;
        if (pearlOwner != null && !owned) return null;
        Object[] copies = new Object[9]; double[] data = new double[13];
        data[0] = 1; data[1] = pearlNonce = next(); data[2] = slot; data[3] = owned ? 1 : 0;
        for (int i = 0; i < 9; ++i) {
            Object stack = stacks.get(i); if (stack == null) return null;
            copies[i] = copy.invoke(stack);
            data[4 + i] = !(boolean) empty.invoke(stack) && item.invoke(stack) == pearlItem.get(null) ? 1 : 0;
        }
        return stopped ? null : new Object[]{this, p, w, inv, m, copies, data};
    }
    public boolean beginPearl(Object[] packet, int slot) throws ReflectiveOperationException {
        if (client() == null || pearlOwner != null || packet == null || packet.length != 7 || packet[0] != this ||
            !(packet[6] instanceof double[] data) || data.length != 13 || data[0] != 1 || data[1] <= 0 || data[1] != pearlNonce || slot < 0 || slot > 8) return false;
        pearlNonce = 0; Object[] now = capturePearl(); pearlNonce = 0;
        if (now == null || ((double[]) now[6])[2] != data[2] || ((double[]) now[6])[4 + slot] != 1) return false;
        for (int i = 1; i <= 4; ++i) if (packet[i] != now[i]) return false;
        Object stack = ((Object[]) now[5])[slot];
        if (!equal(((Object[]) packet[5])[slot], stack)) return false;
        pearlOwner = new Object[]{now[1], now[2], now[3], now[4], (int) data[2], slot, stack, false};
        if ((int) data[2] != slot) { selected.setInt(now[3], slot); sync.invoke(now[4]); }
        return true;
    }
    public boolean usePearl() throws ReflectiveOperationException {
        if (pearlOwner == null || (boolean) pearlOwner[7]) return false;
        Object[] now = capturePearl(); pearlNonce = 0;
        if (now == null || ((double[]) now[6])[3] != 1) return false;
        int slot = (int) pearlOwner[5];
        if (((double[]) now[6])[4 + slot] != 1 || !equal(((Object[]) now[5])[slot], pearlOwner[6])) return false;
        pearlOwner[7] = true; // Non-repeatable even if a mod throws after the use was consumed.
        Object c = client(); cooldown.setInt(c, 0); use.invoke(c); return true;
    }
    public boolean releasePearl() throws ReflectiveOperationException {
        pearlNonce = 0;
        if (pearlOwner == null) return true;
        Object c = client(); if (c == null) return false;
        Object[] owner = pearlOwner;
        if (player.get(c) == owner[0] && world.get(c) == owner[1] && manager.get(c) == owner[3] && inventory.invoke(owner[0]) == owner[2] &&
            selected.getInt(owner[2]) == (int) owner[5] && (int) owner[4] != (int) owner[5]) {
            // Consume before invoking an overridable synchronizer; no delayed restore replay.
            pearlOwner = null; selected.setInt(owner[2], (int) owner[4]); sync.invoke(owner[3]);
        } else pearlOwner = null;
        return true;
    }
    public void stop() { stopped = true; sneakOwner = pearlOwner = null; bridgeNonce = pearlNonce = 0; }
}
