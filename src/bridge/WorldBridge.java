package net.onestep.hybrid;

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.lang.reflect.Proxy;
import java.util.List;
import java.util.HashMap;
import java.util.Map;

/** OneStep world I/O and Java-owned Hit Crystal / Triggerbot controllers. */
public final class WorldBridge {
    private final ClassLoader loader;
    private final int target;
    private final Map<String, String> names;
    private final Class<?> clientType, playerType;
    private final Field instance, player, world, screen, options, manager;
    private final Method onThread, alive;
    private final boolean[] prepared = new boolean[5];
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
        this(loader, target, null);
    }
    public WorldBridge(ClassLoader loader, int target, String[] bindings) throws ReflectiveOperationException {
        if (loader == null || (target != 12101 && target != 12104 && target != 12108 && target != 12111 &&
            target != 26100 && target != 26101 && target != 26102) || (bindings == null && target != 12111))
            throw new IllegalArgumentException("WorldBridge requires an exact supported target and bindings");
        this.target = target;
        names = new HashMap<>();
        if (bindings != null) {
            if (bindings.length == 0 || (bindings.length & 1) != 0) throw new IllegalArgumentException("World binding pairs");
            for (int i = 0; i < bindings.length; i += 2)
                if (bindings[i] == null || bindings[i + 1] == null || bindings[i + 1].isEmpty() ||
                    names.putIfAbsent(bindings[i], bindings[i + 1]) != null) throw new IllegalArgumentException("World binding name");
        }
        this.loader = loader; clientType = load("class_310"); playerType = load("class_746");
        instance = mappedField(clientType, "field_1700", clientType, true);
        player = mappedField(clientType, "field_1724", playerType, false);
        world = mappedField(clientType, "field_1687", load("class_638"), false);
        screen = mappedField(clientType, "field_1755", load("class_437"), false);
        options = mappedField(clientType, "field_1690", load("class_315"), false);
        manager = mappedField(clientType, "field_1761", load("class_636"), false);
        onThread = mappedMethod(clientType, "method_18854", boolean.class);
        alive = mappedMethod(playerType, "method_5805", boolean.class);
    }
    private String name(String source) {
        if (names.isEmpty()) return source; // Existing 1.21.11 test/embedding constructor.
        String result = names.get(source);
        if (result == null) throw new IllegalArgumentException("Missing exact world binding: " + source);
        return result;
    }
    private Class<?> load(String source) throws ClassNotFoundException {
        return Class.forName(name("net/minecraft/" + source).replace('/', '.'), false, loader);
    }
    private Field mappedField(Class<?> owner, String source, Class<?> type, boolean stat) throws ReflectiveOperationException {
        return field(owner, name(source), type, stat);
    }
    private Method mappedMethod(Class<?> owner, String source, Class<?> result, Class<?>... args) throws ReflectiveOperationException {
        String mapped = name(source);
        try {
            Method m = owner.getMethod(mapped, args); // Includes public interface defaults such as raycast.
            if (m.getReturnType() != result || Modifier.isStatic(m.getModifiers()) || !m.trySetAccessible()) throw new IllegalAccessException(mapped);
            return m;
        } catch (NoSuchMethodException missing) { return method(owner, mapped, result, args); }
    }
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
        if (stopped || feature < 1 || feature > 4 || (feature < 3 && target != 12111)) throw new IllegalStateException("World feature/lifecycle");
        if (prepared[feature]) return;
        if (feature == 4) { prepareTrigger(); prepared[4] = true; return; }
        if (feature == 3) { prepareCrystal(); prepared[3] = true; return; }
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
    // Independent Java controller. No Argon code, tick listener, native callback,
    // queued task or worker owns this state; one client frame performs at most one action.
    private Class<?> crystalType, crystalHitType, crystalEntityHitType;
    private Field crystalHitEntity;
    private Field crystalCrosshair, crystalSelected, crystalPosition, crystalX, crystalY, crystalZ;
    private Field crystalMinX, crystalMinY, crystalMinZ, crystalMaxX, crystalMaxY, crystalMaxZ;
    private Method crystalInventory, crystalStack, crystalEmpty, crystalItem, crystalUsing, crystalSync;
    private Method crystalStateAt, crystalIsOf, crystalAir, crystalEntities, crystalById, crystalId, crystalAlive;
    private Method crystalGetBlock, crystalGetType, crystalGetX, crystalGetY, crystalGetZ, crystalBounds;
    private Method crystalBlockRange, crystalEntityRange, crystalEyeY, crystalRaycast, crystalPlace, crystalAccepted, crystalAttack, crystalSwing;
    private Constructor<?> crystalPosCtor, crystalVecCtor, crystalHitCtor, crystalRayCtor, crystalBoxCtor;
    private Object crystalItemValue, crystalObsidian, crystalBedrock, crystalHand, crystalUp, crystalBlockType, crystalCollider, crystalNoFluid;
    private Object crystalPlayer, crystalWorld, crystalManager, crystalInv, crystalBase, crystalTarget, crystalArea;
    private int crystalStage, crystalOldSlot = -1, crystalOwnedSlot = -1, crystalTargetId = -1;
    private int crystalBaseX, crystalBaseY, crystalBaseZ, crystalPlaceAttempts, crystalBreakAttempts;
    private long crystalNext, crystalStarted;
    private boolean crystalRestore = true, crystalRequireRelease, crystalFailed;
    private Field crystalUseKey, crystalPressCount;
    private Method crystalPressed, crystalSetPressed;
    private Object[] crystalUseOwner;

    private static Constructor<?> constructor(Class<?> owner, Class<?>... args) throws ReflectiveOperationException {
        Constructor<?> c = owner.getDeclaredConstructor(args);
        if (!c.trySetAccessible()) throw new IllegalAccessException("World constructor");
        return c;
    }
    private void prepareCrystal() throws ReflectiveOperationException {
        Class<?> entity = load("class_1297"), inv = load("class_1661"), stack = load("class_1799"), vec = load("class_243");
        Class<?> w = load("class_638"), pos = load("class_2338"), state = load("class_2680"), block = load("class_2248");
        Class<?> hit = load("class_239"), type = load("class_239$class_240"), box = load("class_238"), hand = load("class_1268");
        Class<?> m = load("class_636"), ray = load("class_3959"), shape = load("class_3959$class_3960"), fluid = load("class_3959$class_242");
        Class<?> binding = load("class_304"), input = load("class_3675$class_306"), inputType = load("class_3675$class_307");
        crystalUseKey = mappedField(load("class_315"), "field_1904", binding, false);
        crystalPressCount = mappedField(binding, "field_1661", int.class, false);
        crystalPressed = mappedMethod(binding, "method_1434", boolean.class);
        crystalSetPressed = mappedMethod(binding, "method_23481", void.class, boolean.class);
        bound = mappedField(binding, "field_1655", input, false);
        code = mappedMethod(input, "method_1444", int.class); category = mappedMethod(input, "method_1442", inputType);
        keySymbol = mappedField(inputType, "field_1668", inputType, true);
        scanCode = mappedField(inputType, "field_1671", inputType, true); mouse = mappedField(inputType, "field_1672", inputType, true);
        crystalType = load("class_1511"); crystalHitType = load("class_3965");
        crystalEntityHitType = load("class_3966");
        crystalHitEntity = mappedField(crystalEntityHitType, "field_17592", entity, false);
        crystalCrosshair = mappedField(clientType, "field_1765", hit, false);
        crystalSelected = mappedField(inv, "field_7545", int.class, false);
        crystalPosition = mappedField(entity, "field_22467", vec, false);
        crystalX = mappedField(vec, "field_1352", double.class, false);
        crystalY = mappedField(vec, "field_1351", double.class, false);
        crystalZ = mappedField(vec, "field_1350", double.class, false);
        crystalMinX = mappedField(box, "field_1323", double.class, false); crystalMaxX = mappedField(box, "field_1320", double.class, false);
        crystalMinY = mappedField(box, "field_1322", double.class, false); crystalMaxY = mappedField(box, "field_1325", double.class, false);
        crystalMinZ = mappedField(box, "field_1321", double.class, false); crystalMaxZ = mappedField(box, "field_1324", double.class, false);
        crystalInventory = mappedMethod(playerType, "method_31548", inv);
        crystalStack = mappedMethod(inv, "method_5438", stack, int.class);
        crystalEmpty = mappedMethod(stack, "method_7960", boolean.class);
        crystalItem = mappedMethod(stack, "method_7909", load("class_1792"));
        crystalUsing = mappedMethod(playerType, "method_6115", boolean.class);
        crystalSync = mappedMethod(m, "method_2911", void.class);
        crystalStateAt = mappedMethod(w, "method_8320", state, pos);
        crystalIsOf = mappedMethod(state, "method_27852", boolean.class, block);
        crystalAir = mappedMethod(state, "method_26215", boolean.class);
        crystalEntities = mappedMethod(w, "method_8335", List.class, entity, box);
        crystalById = mappedMethod(w, "method_8469", entity, int.class);
        crystalId = mappedMethod(entity, "method_5628", int.class);
        crystalAlive = mappedMethod(entity, "method_5805", boolean.class);
        crystalBounds = mappedMethod(entity, "method_5829", box);
        crystalGetType = mappedMethod(hit, "method_17783", type);
        crystalGetBlock = mappedMethod(crystalHitType, "method_17777", pos);
        crystalGetX = mappedMethod(pos, "method_10263", int.class);
        crystalGetY = mappedMethod(pos, "method_10264", int.class);
        crystalGetZ = mappedMethod(pos, "method_10260", int.class);
        crystalBlockRange = mappedMethod(playerType, "method_55754", double.class);
        crystalEntityRange = mappedMethod(playerType, "method_55755", double.class);
        crystalEyeY = mappedMethod(entity, "method_23320", double.class);
        crystalRaycast = mappedMethod(w, "method_17742", crystalHitType, ray);
        crystalPlace = mappedMethod(m, "method_2896", load("class_1269"), playerType, hand, crystalHitType);
        crystalAccepted = mappedMethod(load("class_1269"), "method_23665", boolean.class);
        crystalAttack = mappedMethod(m, "method_2918", void.class, load("class_1657"), entity);
        crystalSwing = mappedMethod(playerType, "method_6104", void.class, hand);
        crystalPosCtor = constructor(pos, int.class, int.class, int.class);
        crystalVecCtor = constructor(vec, double.class, double.class, double.class);
        crystalHitCtor = constructor(crystalHitType, vec, load("class_2350"), pos, boolean.class);
        crystalRayCtor = constructor(ray, vec, vec, shape, fluid, entity);
        crystalBoxCtor = constructor(box, double.class, double.class, double.class, double.class, double.class, double.class);
        crystalItemValue = mappedField(load("class_1802"), "field_8301", load("class_1792"), true).get(null);
        crystalObsidian = mappedField(load("class_2246"), "field_10540", block, true).get(null);
        crystalBedrock = mappedField(load("class_2246"), "field_9987", block, true).get(null);
        crystalHand = mappedField(hand, "field_5808", hand, true).get(null);
        crystalUp = mappedField(load("class_2350"), "field_11036", load("class_2350"), true).get(null);
        crystalBlockType = mappedField(type, "field_1332", type, true).get(null);
        crystalCollider = mappedField(shape, "field_17558", shape, true).get(null);
        crystalNoFluid = mappedField(fluid, "field_1348", fluid, true).get(null);
        if (crystalItemValue == null || crystalObsidian == null || crystalBedrock == null || crystalHand == null ||
            crystalUp == null || crystalBlockType == null || crystalCollider == null || crystalNoFluid == null)
            throw new IllegalStateException("Uninitialized crystal bindings");
    }
    private boolean crystalHeld(Object inv, int slot) throws ReflectiveOperationException {
        if (slot < 0 || slot > 8) return false;
        Object s = crystalStack.invoke(inv, slot);
        return s != null && !(boolean) crystalEmpty.invoke(s) && crystalItem.invoke(s) == crystalItemValue;
    }
    private boolean crystalSession(Object c) throws ReflectiveOperationException {
        return c != null && live(c, crystalPlayer, crystalWorld) && player.get(c) == crystalPlayer &&
            world.get(c) == crystalWorld && manager.get(c) == crystalManager &&
            crystalInventory.invoke(crystalPlayer) == crystalInv && crystalSelected.getInt(crystalInv) == crystalOwnedSlot;
    }
    private void resetCrystal(boolean restore) throws ReflectiveOperationException {
        Object p = crystalPlayer, w = crystalWorld, m = crystalManager, inv = crystalInv;
        int old = crystalOldSlot, owned = crystalOwnedSlot;
        // Consume ownership before an overridable synchronizer can throw. Never
        // restore into another world/player/inventory or over a manual selection.
        crystalPlayer = crystalWorld = crystalManager = crystalInv = crystalBase = crystalTarget = crystalArea = null;
        crystalOldSlot = crystalOwnedSlot = crystalTargetId = -1;
        crystalStage = crystalPlaceAttempts = crystalBreakAttempts = 0; crystalNext = crystalStarted = 0;
        if (!restore || old < 0 || old > 8 || inv == null) return;
        Object c = client();
        if (c != null && player.get(c) == p && world.get(c) == w && manager.get(c) == m &&
            crystalInventory.invoke(p) == inv && crystalSelected.getInt(inv) == owned) {
            crystalSelected.setInt(inv, old); crystalSync.invoke(m);
        }
    }
    public int[] crystalInput() throws ReflectiveOperationException {
        if (!prepared[3]) return null;
        Object c = client(); if (c == null) return null;
        Object opts = options.get(c), key = opts == null ? null : crystalUseKey.get(opts);
        return descriptor(key == null ? null : bound.get(key));
    }
    private boolean suppressCrystalUse(Object c, int kind, int keyCode) throws ReflectiveOperationException {
        Object opts = options.get(c), key = opts == null ? null : crystalUseKey.get(opts);
        Object input = key == null ? null : bound.get(key); int[] actual = descriptor(input);
        if (input == null || actual[0] != kind || actual[1] != keyCode) return false;
        if (crystalUseOwner != null && (crystalUseOwner[0] != player.get(c) || crystalUseOwner[1] != world.get(c) ||
            crystalUseOwner[2] != opts || crystalUseOwner[3] != key || crystalUseOwner[4] != input)) return false;
        if (crystalUseOwner == null) crystalUseOwner = new Object[]{player.get(c), world.get(c), opts, key, input};
        // Suppress both continuous vanilla use and the queued press belonging
        // to this physical activation. No Windows events or attack presses.
        crystalSetPressed.invoke(key, false); crystalPressCount.setInt(key, 0);
        return true;
    }
    private boolean releaseCrystalUse(Object c, int kind, int keyCode, boolean physical) throws ReflectiveOperationException {
        if (crystalUseOwner == null) return true;
        if (c == null) return false;
        Object[] owner = crystalUseOwner; crystalUseOwner = null;
        Object opts = options.get(c), key = opts == null ? null : crystalUseKey.get(opts);
        if (player.get(c) == owner[0] && world.get(c) == owner[1] && opts == owner[2] && key == owner[3] && bound.get(key) == owner[4]) {
            int[] actual = descriptor(owner[4]);
            if (actual[0] == kind && actual[1] == keyCode && !(boolean) crystalPressed.invoke(key) && physical)
                crystalSetPressed.invoke(key, true);
        }
        return true;
    }
    public boolean cancelCrystal(int kind, int keyCode, boolean physical) throws ReflectiveOperationException {
        if (!prepared[3]) return true;
        Object c = client(); if (c == null) return false;
        crystalRequireRelease = true;
        try { resetCrystal(crystalRestore); }
        finally { releaseCrystalUse(c, kind, keyCode, physical); }
        return true;
    }
    private boolean crystalSite() throws ReflectiveOperationException {
        Object state = crystalStateAt.invoke(crystalWorld, crystalBase);
        if (state == null || (!(boolean) crystalIsOf.invoke(state, crystalObsidian) &&
            !(boolean) crystalIsOf.invoke(state, crystalBedrock))) return false;
        Object above = crystalPosCtor.newInstance(crystalBaseX, crystalBaseY + 1, crystalBaseZ);
        Object space = crystalStateAt.invoke(crystalWorld, above);
        return space != null && (boolean) crystalAir.invoke(space);
    }
    private boolean crystalClear() throws ReflectiveOperationException {
        Object entities = crystalEntities.invoke(crystalWorld, null, crystalArea);
        if (!(entities instanceof Iterable<?> iterable)) return false;
        for (Object e : iterable) {
            if (e == null || !(boolean) crystalAlive.invoke(e)) continue;
            Object b = crystalBounds.invoke(e); if (b == null) return false;
            double lx = crystalMinX.getDouble(b), ly = crystalMinY.getDouble(b), lz = crystalMinZ.getDouble(b);
            double hx = crystalMaxX.getDouble(b), hy = crystalMaxY.getDouble(b), hz = crystalMaxZ.getDouble(b);
            if (!Double.isFinite(lx) || !Double.isFinite(ly) || !Double.isFinite(lz) ||
                !Double.isFinite(hx) || !Double.isFinite(hy) || !Double.isFinite(hz) || lx > hx || ly > hy || lz > hz) return false;
            if (hx > crystalBaseX && lx < crystalBaseX + 1. && hy > crystalBaseY + 1. &&
                ly < crystalBaseY + 3. && hz > crystalBaseZ && lz < crystalBaseZ + 1.) return false;
        }
        return true;
    }
    private boolean crystalReach(double x, double y, double z, Method rangeMethod) throws ReflectiveOperationException {
        Object v = crystalPosition.get(crystalPlayer); if (v == null) return false;
        double dx = crystalX.getDouble(v) - x, dy = (double) crystalEyeY.invoke(crystalPlayer) - y, dz = crystalZ.getDouble(v) - z;
        double range = (double) rangeMethod.invoke(crystalPlayer), distance = dx * dx + dy * dy + dz * dz;
        return Double.isFinite(range) && range > 0 && Double.isFinite(distance) && distance <= range * range;
    }
    private boolean crystalMatches(Object e) throws ReflectiveOperationException {
        if (e == null || !crystalType.isInstance(e) || !(boolean) crystalAlive.invoke(e)) return false;
        Object v = crystalPosition.get(e); if (v == null) return false;
        double dx = crystalX.getDouble(v) - (crystalBaseX + .5), dy = crystalY.getDouble(v) - (crystalBaseY + 1.),
            dz = crystalZ.getDouble(v) - (crystalBaseZ + .5);
        double distance = dx * dx + dy * dy + dz * dz;
        return Double.isFinite(distance) && distance < 1.75;
    }
    private Object findCrystal() throws ReflectiveOperationException {
        Object entities = crystalEntities.invoke(crystalWorld, null, crystalArea);
        if (!(entities instanceof Iterable<?> iterable)) throw new IllegalStateException("Crystal entities");
        for (Object e : iterable) if (crystalMatches(e)) return e;
        return null;
    }
    private boolean crystalVisible(Object e) throws ReflectiveOperationException {
        Object eye = crystalPosition.get(crystalPlayer), end = crystalPosition.get(e), b = crystalBounds.invoke(e);
        if (eye == null || end == null || b == null) return false;
        double x = crystalX.getDouble(eye), y = (double) crystalEyeY.invoke(crystalPlayer), z = crystalZ.getDouble(eye);
        double lx = crystalMinX.getDouble(b), ly = crystalMinY.getDouble(b), lz = crystalMinZ.getDouble(b);
        double hx = crystalMaxX.getDouble(b), hy = crystalMaxY.getDouble(b), hz = crystalMaxZ.getDouble(b);
        if (!Double.isFinite(lx) || !Double.isFinite(ly) || !Double.isFinite(lz) || !Double.isFinite(hx) ||
            !Double.isFinite(hy) || !Double.isFinite(hz) || lx > hx || ly > hy || lz > hz) return false;
        if (!crystalReach(Math.max(lx, Math.min(hx, x)), Math.max(ly, Math.min(hy, y)),
            Math.max(lz, Math.min(hz, z)), crystalEntityRange)) return false;
        double tx = crystalX.getDouble(end), ty = crystalY.getDouble(end) + .45, tz = crystalZ.getDouble(end);
        if (!Double.isFinite(tx) || !Double.isFinite(ty) || !Double.isFinite(tz)) return false;
        Object from = crystalVecCtor.newInstance(x, y, z), to = crystalVecCtor.newInstance(tx, ty, tz);
        Object ray = crystalRayCtor.newInstance(from, to, crystalCollider, crystalNoFluid, crystalPlayer);
        Object hit = crystalRaycast.invoke(crystalWorld, ray);
        return hit != null && crystalGetType.invoke(hit) != crystalBlockType;
    }
    private boolean breakCrystal(Object c, Object e, long now, int delay) throws ReflectiveOperationException {
        if (!crystalSession(c) || !crystalMatches(e) || !crystalVisible(e)) return false;
        int id = (int) crystalId.invoke(e);
        if (crystalById.invoke(crystalWorld, id) != e) return false;
        crystalTarget = e; crystalTargetId = id;
        if (crystalStage != 3) { crystalStarted = now; crystalBreakAttempts = 0; }
        crystalStage = 3; ++crystalBreakAttempts; crystalNext = now + Math.max(35, delay) * 1_000_000L;
        crystalAttack.invoke(crystalManager, crystalPlayer, e);
        if (crystalSession(client())) crystalSwing.invoke(crystalPlayer, crystalHand);
        return true;
    }
    public void tickCrystal(boolean active, boolean held, int kind, int keyCode, boolean physicalUse,
        boolean autoSwitch, boolean restore, int placeDelay, int breakDelay)
        throws ReflectiveOperationException {
        if (stopped) return;
        Object c = client(); if (c == null) return;
        if (!prepared[3]) { if (!active || !held) return; prepare(3); }
        crystalRestore = restore;
        if (!active || !held) {
            crystalRequireRelease = held;
            try { resetCrystal(restore); } finally { releaseCrystalUse(c, kind, keyCode, physicalUse); }
            return;
        }
        if (crystalFailed || crystalRequireRelease) return;
        placeDelay = Math.max(0, Math.min(250, placeDelay)); breakDelay = Math.max(0, Math.min(200, breakDelay));
        try {
            Object p = player.get(c), w = world.get(c), m = manager.get(c);
            if (!live(c, p, w) || m == null || (boolean) crystalUsing.invoke(p)) {
                crystalRequireRelease = true; resetCrystal(restore); return;
            }
            Object inv = crystalInventory.invoke(p);
            int slot = inv == null ? -1 : crystalSelected.getInt(inv);
            if (slot < 0 || slot > 8 || (crystalPlayer != null &&
                (crystalPlayer != p || crystalWorld != w || crystalManager != m || crystalInv != inv || slot != crystalOwnedSlot))) {
                crystalRequireRelease = true; resetCrystal(false); return;
            }
            long now = System.nanoTime();
            if (crystalUseOwner != null && !suppressCrystalUse(c, kind, keyCode)) {
                crystalRequireRelease = true; resetCrystal(restore); return;
            }
            if (crystalStage == 3) {
                Object e = crystalById.invoke(w, crystalTargetId);
                if (e == null || (e == crystalTarget && !(boolean) crystalAlive.invoke(e))) {
                    resetCrystal(restore); crystalStage = 4; crystalNext = now + placeDelay * 1_000_000L; return;
                }
                if (e != crystalTarget || now - crystalStarted > 360_000_000L || crystalBreakAttempts >= 4) {
                    crystalRequireRelease = true; resetCrystal(restore); return;
                }
                if (now - crystalNext >= 0 && !breakCrystal(c, e, now, breakDelay)) {
                    crystalRequireRelease = true; resetCrystal(restore);
                }
                return;
            }
            if (crystalStage == 2) {
                Object e = findCrystal();
                if (e != null && now - crystalNext >= 0) {
                    if (!breakCrystal(c, e, now, breakDelay)) { crystalRequireRelease = true; resetCrystal(restore); }
                } else if (now - crystalStarted > 480_000_000L) {
                    if (crystalPlaceAttempts < 3 && crystalSite() && crystalClear()) {
                        crystalStage = 1; crystalNext = now + 35_000_000L;
                    } else { crystalRequireRelease = true; resetCrystal(restore); }
                }
                return;
            }
            if (crystalStage == 4) {
                if (now - crystalNext < 0) return;
                crystalStage = 0;
            }
            if (crystalStage == 1) {
                if (now - crystalNext < 0) return;
                if (!crystalSession(c) || !crystalHeld(inv, slot) || !crystalSite() || !crystalClear() ||
                    !crystalReach(crystalBaseX + .5, crystalBaseY + 1., crystalBaseZ + .5, crystalBlockRange)) {
                    crystalRequireRelease = true; resetCrystal(restore); return;
                }
                Object point = crystalVecCtor.newInstance(crystalBaseX + .5, crystalBaseY + 1., crystalBaseZ + .5);
                Object hit = crystalHitCtor.newInstance(point, crystalUp, crystalBase, false);
                crystalStage = 2; ++crystalPlaceAttempts; crystalStarted = now; crystalNext = now + breakDelay * 1_000_000L;
                Object result = crystalPlace.invoke(m, p, crystalHand, hit);
                if (result == null || !(boolean) crystalAccepted.invoke(result)) {
                    crystalRequireRelease = true; resetCrystal(restore); return;
                }
                if (crystalSession(client())) crystalSwing.invoke(p, crystalHand);
                return;
            }
            Object hit = crystalCrosshair.get(c), base;
            if (crystalHitType.isInstance(hit) && crystalGetType.invoke(hit) == crystalBlockType) base = crystalGetBlock.invoke(hit);
            else if (crystalEntityHitType.isInstance(hit)) {
                Object e = crystalHitEntity.get(hit);
                if (!crystalType.isInstance(e) || !(boolean) crystalAlive.invoke(e)) return;
                Object v = crystalPosition.get(e); if (v == null) return;
                double bx = Math.floor(crystalX.getDouble(v)), by = Math.floor(crystalY.getDouble(v)) - 1., bz = Math.floor(crystalZ.getDouble(v));
                if (!Double.isFinite(bx) || !Double.isFinite(by) || !Double.isFinite(bz) || bx < Integer.MIN_VALUE || bx > Integer.MAX_VALUE ||
                    by < Integer.MIN_VALUE || by >= Integer.MAX_VALUE - 2 || bz < Integer.MIN_VALUE || bz > Integer.MAX_VALUE) return;
                base = crystalPosCtor.newInstance((int) bx, (int) by, (int) bz);
            } else return;
            if (base == null) return;
            crystalPlayer = p; crystalWorld = w; crystalManager = m; crystalInv = inv; crystalBase = base;
            crystalOwnedSlot = slot;
            crystalBaseX = (int) crystalGetX.invoke(base); crystalBaseY = (int) crystalGetY.invoke(base); crystalBaseZ = (int) crystalGetZ.invoke(base);
            crystalArea = crystalBoxCtor.newInstance((double) crystalBaseX, crystalBaseY + 1., (double) crystalBaseZ,
                crystalBaseX + 1., crystalBaseY + 3., crystalBaseZ + 1.);
            if (!crystalSite() || !crystalReach(crystalBaseX + .5, crystalBaseY + 1., crystalBaseZ + .5, crystalBlockRange)) {
                resetCrystal(false); return;
            }
            Object existing = findCrystal();
            if (existing == null && !crystalClear()) { resetCrystal(false); return; }
            int chosen = slot;
            if (!crystalHeld(inv, slot)) {
                if (!autoSwitch) { resetCrystal(false); return; }
                chosen = -1;
                for (int i = 0; i < 9; ++i) if (crystalHeld(inv, i)) { chosen = i; break; }
                if (chosen < 0) { resetCrystal(false); return; }
            }
            crystalStage = existing == null ? 1 : 2; crystalStarted = now;
            int firstDelay = existing == null ? placeDelay : breakDelay;
            crystalNext = now + (chosen == slot ? firstDelay : Math.max(25, firstDelay)) * 1_000_000L;
            if (!suppressCrystalUse(c, kind, keyCode)) { crystalRequireRelease = true; resetCrystal(false); return; }
            if (chosen != slot) {
                crystalOldSlot = slot; crystalOwnedSlot = chosen;
                crystalSelected.setInt(inv, chosen); crystalSync.invoke(m);
            }
        } catch (ReflectiveOperationException | RuntimeException error) {
            // A failed vanilla call may already have sent its packet. Suspend
            // rather than invoking it again through a second implementation.
            crystalFailed = crystalRequireRelease = true;
            try { resetCrystal(restore); } catch (ReflectiveOperationException cleanup) { error.addSuppressed(cleanup); }
            throw error;
        } finally {
            if (crystalRequireRelease || (crystalStage == 0 && crystalPlayer == null))
                releaseCrystalUse(c, kind, keyCode, physicalUse);
        }
    }

    // Triggerbot owns its complete decision loop in Java. The native callback
    // only exposes shared friend/bot services, attack ownership and other
    // modules' existing weapon preparation; it never chooses a trigger attack.
    private native long triggerService(int operation, Object p, Object w, Object e, int value, long epoch);
    private Class<?> triggerLiving, triggerPlayerType, triggerMob, triggerAnimal, triggerAxe, triggerSword, triggerHitType;
    private Field triggerCrosshair, triggerHitEntity, triggerSelected, triggerFall, triggerTicks, triggerYaw, triggerPos;
    private Field triggerX, triggerZ, triggerRenderer;
    private Method triggerAlive, triggerInvisible, triggerUsing, triggerInventory, triggerStack, triggerEmpty, triggerItem;
    private Method triggerCooldown, triggerGround, triggerWater, triggerSwimming, triggerFlying, triggerClimbing;
    private Method triggerRiding, triggerSprinting, triggerBlind, triggerBlocking, triggerId, triggerCamera, triggerPick;
    private Method triggerBlockRange, triggerEntityRange, triggerAttack, triggerSwing, triggerEntry, triggerInTag;
    private Method triggerGetWindow, triggerGetHandle, triggerGlfwMouse;
    private Object triggerBlindness, triggerHand, triggerSwordTag;
    private Object triggerPlayer, triggerWorld, triggerManager, triggerTarget, triggerInv;
    private long triggerEpoch, triggerGeneration, triggerManual, triggerReady, triggerCritical = -1, triggerNotBefore;
    private int triggerSlot = -1, triggerLastTicks = -1, triggerPreparedSlot = -1, triggerPreparationTicks = -1;
    private float triggerThreshold, triggerLastCooldown;
    private boolean triggerFailed;
    private boolean triggerSimulationPrepared;
    private Field triggerMouse, triggerMouseLeft, triggerBindings, triggerBindingPressed, triggerBindingCount, triggerBindingKey;
    private Method triggerMouseCallback, triggerKeyCode, triggerKeyCategory;
    private Constructor<?> triggerMouseInputConstructor;
    private Object triggerMouseKeyType;
    private boolean triggerTickHooked;
    private volatile boolean triggerActive;
    private volatile long triggerLastConfigTime;
    private volatile long triggerGenerationArg, triggerManualArg;
    private volatile float triggerLowArg = .9f, triggerHighArg = 1.f;
    private volatile int triggerReactionLowArg, triggerReactionHighArg;
    private volatile int triggerTargetsArg, triggerWeaponArg, triggerCriticalsArg;
    private volatile boolean triggerOnClickArg, triggerSwingHandArg, triggerClickSimulationArg;

    private void prepareTrigger() throws ReflectiveOperationException {
        Class<?> entity = load("class_1297"), inv = load("class_1661"), stack = load("class_1799");
        Class<?> itemType = load("class_1792"), vec = load("class_243"), hand = load("class_1268");
        Class<?> hit = load("class_239"), m = load("class_636"), entry = load("class_6880");
        triggerLiving = load("class_1309"); triggerPlayerType = load("class_1657");
        triggerMob = load("class_1308"); triggerAnimal = load("class_1429"); triggerAxe = load("class_1743");
        triggerHitType = load("class_3966");
        triggerCrosshair = mappedField(clientType, "field_1765", hit, false);
        triggerHitEntity = mappedField(triggerHitType, "field_17592", entity, false);
        triggerSelected = mappedField(inv, "field_7545", int.class, false);
        triggerFall = mappedField(entity, "field_6017", target < 12108 ? float.class : double.class, false);
        triggerTicks = mappedField(triggerLiving, "field_6273", int.class, false);
        triggerYaw = mappedField(entity, "field_6031", float.class, false);
        triggerPos = mappedField(entity, "field_22467", vec, false);
        triggerX = mappedField(vec, "field_1352", double.class, false);
        triggerZ = mappedField(vec, "field_1350", double.class, false);
        triggerAlive = mappedMethod(entity, "method_5805", boolean.class);
        triggerInvisible = mappedMethod(entity, "method_5767", boolean.class);
        triggerUsing = mappedMethod(playerType, "method_6115", boolean.class);
        triggerInventory = mappedMethod(playerType, "method_31548", inv);
        triggerStack = mappedMethod(inv, "method_5438", stack, int.class);
        triggerEmpty = mappedMethod(stack, "method_7960", boolean.class);
        triggerItem = mappedMethod(stack, "method_7909", itemType);
        triggerCooldown = mappedMethod(playerType, "method_7261", float.class, float.class);
        triggerGround = mappedMethod(entity, "method_24828", boolean.class);
        triggerWater = mappedMethod(entity, "method_5799", boolean.class);
        triggerSwimming = mappedMethod(entity, "method_5681", boolean.class);
        triggerFlying = mappedMethod(triggerLiving, "method_6128", boolean.class);
        triggerClimbing = mappedMethod(triggerLiving, "method_6101", boolean.class);
        triggerRiding = mappedMethod(entity, "method_5765", boolean.class);
        triggerSprinting = mappedMethod(entity, "method_5624", boolean.class);
        triggerBlind = mappedMethod(triggerLiving, "method_6059", boolean.class, entry);
        triggerBlindness = mappedField(load("class_1294"), "field_5919", entry, true).get(null);
        triggerBlocking = mappedMethod(triggerLiving, "method_6039", boolean.class);
        triggerId = mappedMethod(entity, "method_5628", int.class);
        triggerCamera = mappedMethod(clientType, "method_1560", entity);
        triggerBlockRange = mappedMethod(playerType, "method_55754", double.class);
        triggerEntityRange = mappedMethod(playerType, "method_55755", double.class);
        triggerAttack = mappedMethod(m, "method_2918", void.class, triggerPlayerType, entity);
        triggerSwing = mappedMethod(playerType, "method_6104", void.class, hand);
        triggerHand = mappedField(hand, "field_5808", hand, true).get(null);
        Class<?> window = load("class_1041");
        triggerGetWindow = mappedMethod(clientType, "method_22683", window);
        triggerGetHandle = mappedMethod(window, "method_4490", long.class);
        Class<?> glfw = Class.forName("org.lwjgl.glfw.GLFW", false, loader);
        triggerGlfwMouse = glfw.getMethod("glfwGetMouseButton", long.class, int.class);
        if (!Modifier.isStatic(triggerGlfwMouse.getModifiers()) || triggerGlfwMouse.getReturnType() != int.class ||
            !triggerGlfwMouse.trySetAccessible()) throw new IllegalAccessException("GLFW mouse input");
        if (target >= 12111) {
            triggerPick = playerType.getDeclaredMethod(name("method_76763"), entity, double.class, double.class, float.class);
            if (!Modifier.isStatic(triggerPick.getModifiers()) || triggerPick.getReturnType() != hit ||
                !triggerPick.trySetAccessible()) throw new IllegalAccessException("Static crosshair query");
        } else {
            Class<?> renderer = load("class_757");
            triggerRenderer = mappedField(clientType, "field_1773", renderer, false);
            triggerPick = mappedMethod(renderer, "method_56153", hit, entity, double.class, double.class, float.class);
        }
        if (target < 12108) triggerSword = load("class_1829");
        else {
            Class<?> reference = load("class_6880$class_6883"), tag = load("class_6862");
            triggerEntry = mappedMethod(itemType, "method_40131", reference);
            triggerInTag = mappedMethod(reference, "method_40220", boolean.class, tag);
            triggerSwordTag = mappedField(load("class_3489"), "field_42611", tag, true).get(null);
        }
        if (triggerHand == null || triggerBlindness == null || (target >= 12108 && triggerSwordTag == null))
            throw new IllegalStateException("Uninitialized trigger bindings");
        prepareTriggerLifecycle();
    }

    private void prepareTriggerLifecycle() {
        if (triggerTickHooked) return;
        try {
            Class<?> eventsClass = Class.forName("net.fabricmc.fabric.api.client.event.lifecycle.v1.ClientTickEvents", true, loader);
            Class<?> listenerType = Class.forName("net.fabricmc.fabric.api.client.event.lifecycle.v1.ClientTickEvents$StartTick", true, loader);
            Object event = eventsClass.getField("START_CLIENT_TICK").get(null);
            ClassLoader proxyLoader = listenerType.getClassLoader() != null ? listenerType.getClassLoader() : WorldBridge.class.getClassLoader();
            Object proxy = Proxy.newProxyInstance(proxyLoader, new Class<?>[]{listenerType}, (p, m, a) -> {
                if (m.getDeclaringClass() == Object.class) {
                    if (m.getName().equals("hashCode")) return System.identityHashCode(p);
                    if (m.getName().equals("equals")) return p == a[0];
                    return "WorldBridgeTriggerTick";
                }
                if (a != null && a.length >= 1) {
                    onTriggerTick(a[0]);
                }
                return null;
            });
            Class.forName("net.fabricmc.fabric.api.event.Event", true, loader).getMethod("register", Object.class).invoke(event, proxy);
            triggerTickHooked = true;
        } catch (Throwable ignored) {
            triggerTickHooked = false;
        }
    }

    private void onTriggerTick(Object c) {
        if (!triggerActive || triggerFailed || stopped || c == null) return;
        final long now = System.nanoTime();
        if (now - triggerLastConfigTime > 200_000_000L) {
            try { cancelTrigger(); } catch (ReflectiveOperationException ignored) {}
            return;
        }
        try {
            executeTrigger(c, triggerGenerationArg, triggerManualArg,
                triggerLowArg, triggerHighArg, triggerReactionLowArg, triggerReactionHighArg,
                triggerTargetsArg, triggerWeaponArg, triggerCriticalsArg,
                triggerOnClickArg, triggerSwingHandArg, triggerClickSimulationArg);
        } catch (Throwable t) {
            triggerFailed = true;
            try { cancelTrigger(); } catch (Throwable cleanup) { t.addSuppressed(cleanup); }
        }
    }

    private void prepareTriggerSimulation() throws ReflectiveOperationException {
        if (triggerSimulationPrepared) return;
        Class<?> mouse = load("class_312"), binding = load("class_304");
        Class<?> key = load("class_3675$class_306"), type = load("class_3675$class_307");
        triggerMouse = mappedField(clientType, "field_1729", mouse, false);
        triggerMouseLeft = mappedField(mouse, "field_1791", boolean.class, false);
        triggerBindings = mappedField(binding, "field_1657", Map.class, true);
        triggerBindingPressed = mappedField(binding, "field_1653", boolean.class, false);
        triggerBindingCount = mappedField(binding, "field_1661", int.class, false);
        triggerBindingKey = mappedField(binding, "field_1655", key, false);
        triggerKeyCode = mappedMethod(key, "method_1444", int.class);
        triggerKeyCategory = mappedMethod(key, "method_1442", type);
        triggerMouseKeyType = mappedField(type, "field_1672", type, true).get(null);
        if (triggerMouseKeyType == null) throw new IllegalStateException("Mouse input category");
        if (target >= 12111) {
            Class<?> input = load("class_11910");
            triggerMouseInputConstructor = constructor(input, int.class, int.class);
            triggerMouseCallback = mappedMethod(mouse, "method_1601", void.class, long.class, input, int.class);
        } else triggerMouseCallback = mappedMethod(mouse, "method_1601", void.class, long.class, int.class, int.class, int.class);
        triggerSimulationPrepared = true;
    }
    private void triggerMouseEdge(Object mouse, long handle, int action, Object input) throws ReflectiveOperationException {
        if (target >= 12111) triggerMouseCallback.invoke(mouse, handle, input, action);
        else triggerMouseCallback.invoke(mouse, handle, 0, action, 0);
    }
    private void simulateTriggerClick(Object c, Object p, Object w, long generation, long manual) throws ReflectiveOperationException {
        // Only notify the game's mouse callback after the single direct attack.
        // A synchronous pulse has no delayed UP and no worker outliving disable.
        if (client() != c || !live(c, p, w) || player.get(c) != p || world.get(c) != w ||
            triggerService(8, p, w, null, 0, generation) != manual) return;
        Object mouse = triggerMouse.get(c), window = triggerGetWindow.invoke(c);
        long handle = window == null ? 0 : (long) triggerGetHandle.invoke(window);
        Object registry = triggerBindings.get(null);
        if (mouse == null || handle == 0 || !(registry instanceof Map<?, ?> mappings)) return;
        java.util.ArrayList<Object[]> snapshots = new java.util.ArrayList<>();
        for (Object binding : mappings.values()) {
            if (binding == null) continue;
            Object key = triggerBindingKey.get(binding);
            if (key != null && triggerKeyCategory.invoke(key) == triggerMouseKeyType && (int) triggerKeyCode.invoke(key) == 0)
                snapshots.add(new Object[]{binding, key, triggerBindingPressed.getBoolean(binding), triggerBindingCount.getInt(binding)});
        }
        Object input = target >= 12111 ? triggerMouseInputConstructor.newInstance(0, 0) : null;
        boolean held = triggerMouseLeft.getBoolean(mouse);
        ReflectiveOperationException error = null;
        try {
            triggerMouseEdge(mouse, handle, 1, input);
        } catch (ReflectiveOperationException failure) { error = failure; }
        finally {
            // Always finish the pulse, even if a callback threw after its DOWN.
            try { triggerMouseEdge(mouse, handle, 0, input); }
            catch (ReflectiveOperationException failure) { if (error == null) error = failure; else error.addSuppressed(failure); }
            boolean sameSession = false;
            try {
                sameSession = client() == c && player.get(c) == p && world.get(c) == w && screen.get(c) == null &&
                    triggerGetWindow.invoke(c) == window && (long) triggerGetHandle.invoke(window) == handle &&
                    triggerService(8, p, w, null, 0, generation) == manual;
            } catch (ReflectiveOperationException failure) { if (error == null) error = failure; else error.addSuppressed(failure); }
            // Restore every left-mouse binding, including a rebound use key or
            // modded binding. Preserve pre-existing queued manual presses; do
            // not leave another vanilla attack queued after attackEntity.
            for (Object[] state : snapshots) try {
                if (triggerBindingKey.get(state[0]) == state[1]) {
                    triggerBindingPressed.setBoolean(state[0], sameSession && (boolean) state[2]);
                    triggerBindingCount.setInt(state[0], sameSession ? (int) state[3] : 0);
                }
            } catch (ReflectiveOperationException failure) { if (error == null) error = failure; else error.addSuppressed(failure); }
            try { triggerMouseLeft.setBoolean(mouse, sameSession && held); }
            catch (ReflectiveOperationException failure) { if (error == null) error = failure; else error.addSuppressed(failure); }
        }
        if (error != null) throw error;
    }
    private boolean triggerLeftDown(Object c) throws ReflectiveOperationException {
        Object window = triggerGetWindow.invoke(c);
        if (window == null) return false;
        long handle = (long) triggerGetHandle.invoke(window);
        // GLFW_MOUSE_BUTTON_LEFT = 0; GLFW_PRESS = 1. Poll only on the client thread.
        return handle != 0 && (int) triggerGlfwMouse.invoke(null, handle, 0) == 1;
    }
    private boolean triggerSword(Object s) throws ReflectiveOperationException {
        if (s == null || (boolean) triggerEmpty.invoke(s)) return false;
        Object i = triggerItem.invoke(s);
        if (i == null) return false;
        if (triggerSword != null) return triggerSword.isInstance(i);
        Object entry = triggerEntry.invoke(i);
        return entry != null && (boolean) triggerInTag.invoke(entry, triggerSwordTag);
    }
    private boolean triggerAxe(Object s) throws ReflectiveOperationException {
        return s != null && !(boolean) triggerEmpty.invoke(s) && triggerAxe.isInstance(triggerItem.invoke(s));
    }
    private boolean triggerWeapon(Object p, Object w, Object inv, int slot, int weapon) throws ReflectiveOperationException {
        int original = (int) triggerService(6, p, w, null, slot, 0);
        if (original >= 0 && original < 9) slot = original;
        Object s = triggerStack.invoke(inv, slot);
        return weapon == 0 || (weapon == 1 ? triggerSword(s) : triggerAxe(s));
    }
    private boolean triggerShield(Object p, Object e) throws ReflectiveOperationException {
        if (!triggerPlayerType.isInstance(e) || !(boolean) triggerBlocking.invoke(e)) return false;
        Object a = triggerPos.get(p), b = triggerPos.get(e);
        if (a == null || b == null) throw new IllegalStateException("Trigger shield position");
        double dx = triggerX.getDouble(a) - triggerX.getDouble(b), dz = triggerZ.getDouble(a) - triggerZ.getDouble(b);
        float yaw = triggerYaw.getFloat(e);
        if (!Double.isFinite(dx) || !Double.isFinite(dz) || !Float.isFinite(yaw)) throw new IllegalStateException("Trigger shield yaw");
        return Math.abs(Math.IEEEremainder(yaw - Math.toDegrees(Math.atan2(-dx, dz)), 360.)) < 90.;
    }
    private Object triggerFresh(Object c, Object p) throws ReflectiveOperationException {
        if (triggerCamera.invoke(c) != p) return null;
        double blocks = (double) triggerBlockRange.invoke(p), entities = (double) triggerEntityRange.invoke(p);
        if (!Double.isFinite(blocks) || !Double.isFinite(entities) || blocks <= 0 || entities <= 0) return null;
        Object owner = target >= 12111 ? null : triggerRenderer.get(c);
        if (target < 12111 && owner == null) return null;
        Object hit = triggerPick.invoke(owner, p, blocks, entities, 1.f);
        return triggerHitType.isInstance(hit) ? triggerHitEntity.get(hit) : null;
    }
    private boolean triggerEligible(Object c, Object p, Object w, Object m, Object e, int targets) throws ReflectiveOperationException {
        if (c == null || !live(c, p, w) || m == null || e == null || e == p ||
            !triggerLiving.isInstance(e) || !(boolean) triggerAlive.invoke(e) || (boolean) triggerInvisible.invoke(e) ||
            (boolean) triggerUsing.invoke(p)) return false;
        boolean isPlayer = triggerPlayerType.isInstance(e), animal = triggerAnimal.isInstance(e), mob = triggerMob.isInstance(e);
        if (!(isPlayer || (targets >= 1 && mob && !animal) || (targets == 2 && animal))) return false;
        if (isPlayer && triggerService(0, p, w, e, 0, 0) == 0) return false;
        return triggerFresh(c, p) == e;
    }
    private void triggerCycle(long now, float low, float high, int reactionLow, int reactionHigh) {
        java.util.concurrent.ThreadLocalRandom r = java.util.concurrent.ThreadLocalRandom.current();
        triggerThreshold = low == high ? low : (float) r.nextDouble(low, high);
        triggerReady = now + (reactionLow == reactionHigh ? reactionLow : r.nextInt(reactionLow, reactionHigh + 1)) * 1_000_000L;
        triggerCritical = -1;
    }
    public void cancelTrigger() throws ReflectiveOperationException {
        triggerActive = false;
        Object c = client(); if (c == null) return;
        // Do not retain targets or JNI objects across menus, focus/session loss or disable.
        triggerService(5, triggerPlayer != null ? triggerPlayer : player.get(c),
            triggerWorld != null ? triggerWorld : world.get(c), null, 0, 0);
        triggerPlayer = triggerWorld = triggerManager = triggerTarget = triggerInv = null;
        triggerSlot = triggerPreparedSlot = triggerPreparationTicks = triggerLastTicks = -1; triggerLastCooldown = 0; triggerCritical = -1;
        triggerReady = 0;
    }
    public void tickTrigger(boolean active, long generation, long manual,
        float low, float high, int reactionLow, int reactionHigh, int targets, int weapon, int criticals, boolean onClick, boolean swingHand, boolean clickSimulation)
        throws ReflectiveOperationException {
        Object c = client(); if (c == null) return;
        if (!active) { cancelTrigger(); return; }
        if (triggerFailed) return;
        if (!Float.isFinite(low) || !Float.isFinite(high) || low < .5f || high > 1.f || low > high ||
            reactionLow < 0 || reactionHigh > 150 || reactionLow > reactionHigh || targets < 0 || targets > 2 ||
            weapon < 0 || weapon > 2 || criticals < 0 || criticals > 2) { cancelTrigger(); return; }
        prepare(4);
        triggerActive = true;
        triggerLastConfigTime = System.nanoTime();
        triggerGenerationArg = generation;
        triggerManualArg = manual;
        triggerLowArg = low;
        triggerHighArg = high;
        triggerReactionLowArg = reactionLow;
        triggerReactionHighArg = reactionHigh;
        triggerTargetsArg = targets;
        triggerWeaponArg = weapon;
        triggerCriticalsArg = criticals;
        triggerOnClickArg = onClick;
        triggerSwingHandArg = swingHand;
        triggerClickSimulationArg = clickSimulation;
        if (!triggerTickHooked) {
            executeTrigger(c, generation, manual, low, high, reactionLow, reactionHigh,
                targets, weapon, criticals, onClick, swingHand, clickSimulation);
        }
    }

    private void executeTrigger(Object c, long generation, long manual,
        float low, float high, int reactionLow, int reactionHigh, int targets, int weapon, int criticals,
        boolean onClick, boolean swingHand, boolean clickSimulation) throws ReflectiveOperationException {
        final long now = System.nanoTime();
        try {
            if (onClick && !triggerLeftDown(c)) { cancelTrigger(); return; }
            if (clickSimulation) prepareTriggerSimulation();
            Object p = player.get(c), w = world.get(c), m = manager.get(c), hit = triggerCrosshair.get(c);
            Object e = triggerHitType.isInstance(hit) ? triggerHitEntity.get(hit) : null;
            if (!triggerEligible(c, p, w, m, e, targets)) { cancelTrigger(); return; }
            Object inv = triggerInventory.invoke(p);
            int slot = inv == null ? -1 : triggerSelected.getInt(inv);
            if (slot < 0 || slot > 8 || !triggerWeapon(p, w, inv, slot, weapon)) { cancelTrigger(); return; }
            float cd = (float) triggerCooldown.invoke(p, 0.f);
            int ticks = triggerTicks.getInt(p);
            if (!Float.isFinite(cd) || cd < 0 || cd > 1 || ticks < 0) { cancelTrigger(); return; }
            if (triggerPlayer != p || triggerWorld != w || triggerManager != m || triggerTarget != e ||
                triggerInv != inv || triggerSlot != slot || triggerGeneration != generation) {
                cancelTrigger();
                triggerPlayer = p; triggerWorld = w; triggerManager = m; triggerTarget = e; triggerInv = inv;
                triggerSlot = slot; triggerGeneration = generation; triggerManual = manual;
                triggerLastTicks = ticks; triggerLastCooldown = cd;
                if (++triggerEpoch <= 0) throw new IllegalStateException("Trigger epoch exhausted");
                triggerCycle(now, low, high, reactionLow, reactionHigh);
            }
            int id = (int) triggerId.invoke(e);
            if (id < 0 || triggerService(1, p, w, e, id, triggerEpoch) == 0) { cancelTrigger(); return; }
            boolean consumed = triggerPreparedSlot < 0 && (ticks < triggerLastTicks || (triggerLastCooldown >= .5f && cd < .25f));
            triggerLastTicks = ticks; triggerLastCooldown = cd;
            if (manual != triggerManual || consumed) {
                triggerManual = manual;
                if (triggerPreparedSlot >= 0) { cancelTrigger(); return; }
                triggerCycle(now, low, high, reactionLow, reactionHigh); return;
            }
            if (triggerPreparedSlot >= 0 && ticks == triggerPreparationTicks) return;
            if ((triggerNotBefore != 0 && now - triggerNotBefore < 0) || now - triggerReady < 0 || cd < triggerThreshold) return;
            boolean ground = (boolean) triggerGround.invoke(p);
            double fall = ((Number) triggerFall.get(p)).doubleValue();
            if (!Double.isFinite(fall)) { cancelTrigger(); return; }
            boolean possible = !(boolean) triggerWater.invoke(p) && !(boolean) triggerSwimming.invoke(p) &&
                !(boolean) triggerFlying.invoke(p) && !(boolean) triggerClimbing.invoke(p) &&
                !(boolean) triggerRiding.invoke(p) && !(boolean) triggerSprinting.invoke(p) &&
                !(boolean) triggerBlind.invoke(p, triggerBlindness);
            boolean critical = possible && !ground && fall > 0;
            if (criticals == 2 && !critical) return;
            if (criticals == 1 && !ground && possible && !critical) {
                if (triggerCritical < 0) triggerCritical = now;
                if (now - triggerCritical < 150_000_000L) return;
            } else triggerCritical = -1;
            boolean shield = triggerShield(p, e);
            if (triggerPreparedSlot >= 0) {
                if (triggerService(7, p, w, e, triggerPreparedSlot, 0) <= 0) { cancelTrigger(); return; }
            } else {
                int chosen = (int) triggerService(shield && !triggerAxe(triggerStack.invoke(inv, slot)) ? 2 : shield ? 10 : 3,
                    p, w, e, 0, 0);
                if (chosen >= 0 && chosen < 9) {
                    triggerPreparedSlot = triggerSlot = chosen;
                    triggerPreparationTicks = ticks;
                    triggerNotBefore = now + 25_000_000L; return;
                }
                if (shield && !triggerAxe(triggerStack.invoke(inv, slot))) return;
            }
            // All checks and the vanilla calls happen synchronously on the client
            // thread. Never queue a Windows click, replay failures or catch up.
            Object current = client();
            float finalCooldown = (float) triggerCooldown.invoke(p, 0.f);
            if (current != c || player.get(c) != p || world.get(c) != w || manager.get(c) != m ||
                triggerInventory.invoke(p) != inv || triggerSelected.getInt(inv) != triggerSlot ||
                !triggerEligible(c, p, w, m, e, targets) ||
                !Float.isFinite(finalCooldown) || finalCooldown > 1.f || finalCooldown < triggerThreshold ||
                triggerService(8, p, w, e, 0, generation) != manual ||
                (onClick && !triggerLeftDown(c)) ||
                (triggerShield(p, e) && !triggerAxe(triggerStack.invoke(inv, triggerSlot)))) { cancelTrigger(); return; }
            // Reserve the next cycle before a mutating call: an exception may
            // follow an already sent packet, so this feature then suspends.
            triggerNotBefore = now + 50_000_000L;
            triggerAttack.invoke(m, p, e);
            triggerSwing.invoke(p, triggerHand);
            if (clickSimulation) simulateTriggerClick(c, p, w, generation, manual);
            triggerService(4, p, w, e, 0, 0);
            triggerPreparedSlot = triggerPreparationTicks = -1;
            triggerLastTicks = triggerTicks.getInt(p);
            triggerLastCooldown = (float) triggerCooldown.invoke(p, 0.f);
            triggerCycle(now, low, high, reactionLow, reactionHigh);
        } catch (ReflectiveOperationException | RuntimeException error) {
            triggerFailed = true;
            try { cancelTrigger(); } catch (ReflectiveOperationException cleanup) { error.addSuppressed(cleanup); }
            throw error;
        }
    }

    public void stop() {
        triggerActive = false;
        triggerPlayer = triggerWorld = triggerManager = triggerTarget = triggerInv = null;
        stopped = true; sneakOwner = pearlOwner = null; bridgeNonce = pearlNonce = 0;
        crystalPlayer = crystalWorld = crystalManager = crystalInv = crystalBase = crystalTarget = crystalArea = null;
        crystalOldSlot = crystalOwnedSlot = crystalTargetId = -1; crystalStage = 0;
        crystalUseOwner = null;
    }
}
