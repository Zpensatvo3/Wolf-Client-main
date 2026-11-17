package net.onestep.hybrid;

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.lang.reflect.Proxy;
import java.util.List;
import java.util.HashMap;
import java.util.Map;

/** OneStep world I/O and Java-owned combat and Auto Totem controllers. */
public class WorldBridge {
    private final ClassLoader loader;
    private final int target;
    private final Map<String, String> names;
    private final Class<?> clientType, playerType;
    private final Field instance, player, world, screen, options, manager;
    private final Method onThread, alive;
    private final boolean[] prepared = new boolean[9];
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
        if (stopped || feature < 1 || feature > 8 || (feature < 3 && target != 12111)) throw new IllegalStateException("World feature/lifecycle");
        if (prepared[feature]) return;
        if (feature == 8) { prepareAura(); prepared[8] = true; return; }
        if (feature == 7) { prepareTotem(); prepared[7] = true; return; }
        if (feature == 6) { prepareAnchor(); prepared[6] = true; return; }
        if (feature == 5) { prepareJump(); prepared[5] = true; return; }
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
    private Field crystalYaw, crystalPitch;
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
        crystalYaw = mappedField(entity, "field_6031", float.class, false);
        crystalPitch = mappedField(entity, "field_5965", float.class, false);
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
        prepareTickLifecycle();
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
        crystalTickActive = false;
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
    private static boolean rayIntersectsAABB(double ox, double oy, double oz, double dx, double dy, double dz,
        double minX, double minY, double minZ, double maxX, double maxY, double maxZ, double maxDist) {
        double tmin = 0.0;
        double tmax = maxDist;
        if (Math.abs(dx) < 1e-7) {
            if (ox < minX || ox > maxX) return false;
        } else {
            double invD = 1.0 / dx;
            double t1 = (minX - ox) * invD;
            double t2 = (maxX - ox) * invD;
            if (t1 > t2) { double tmp = t1; t1 = t2; t2 = tmp; }
            tmin = Math.max(tmin, t1);
            tmax = Math.min(tmax, t2);
            if (tmin > tmax) return false;
        }
        if (Math.abs(dy) < 1e-7) {
            if (oy < minY || oy > maxY) return false;
        } else {
            double invD = 1.0 / dy;
            double t1 = (minY - oy) * invD;
            double t2 = (maxY - oy) * invD;
            if (t1 > t2) { double tmp = t1; t1 = t2; t2 = tmp; }
            tmin = Math.max(tmin, t1);
            tmax = Math.min(tmax, t2);
            if (tmin > tmax) return false;
        }
        if (Math.abs(dz) < 1e-7) {
            if (oz < minZ || oz > maxZ) return false;
        } else {
            double invD = 1.0 / dz;
            double t1 = (minZ - oz) * invD;
            double t2 = (maxZ - oz) * invD;
            if (t1 > t2) { double tmp = t1; t1 = t2; t2 = tmp; }
            tmin = Math.max(tmin, t1);
            tmax = Math.min(tmax, t2);
            if (tmin > tmax) return false;
        }
        return tmax >= 0.0 && tmin <= maxDist;
    }
    private boolean crystalAimingAt(Object c, Object e) throws ReflectiveOperationException {
        Object hit = crystalCrosshair.get(c);
        if (crystalEntityHitType.isInstance(hit) && crystalHitEntity.get(hit) == e) return true;
        if (crystalYaw == null || crystalPitch == null || crystalPlayer == null) return true;
        Object eye = crystalPosition.get(crystalPlayer), b = crystalBounds.invoke(e);
        if (eye == null || b == null) return false;
        double ox = crystalX.getDouble(eye), oy = (double) crystalEyeY.invoke(crystalPlayer), oz = crystalZ.getDouble(eye);
        double lx = crystalMinX.getDouble(b), ly = crystalMinY.getDouble(b), lz = crystalMinZ.getDouble(b);
        double hx = crystalMaxX.getDouble(b), hy = crystalMaxY.getDouble(b), hz = crystalMaxZ.getDouble(b);
        if (!Double.isFinite(lx) || !Double.isFinite(ly) || !Double.isFinite(lz) || !Double.isFinite(hx) ||
            !Double.isFinite(hy) || !Double.isFinite(hz) || lx > hx || ly > hy || lz > hz) return false;
        float yaw = crystalYaw.getFloat(crystalPlayer), pitch = crystalPitch.getFloat(crystalPlayer);
        if (!Float.isFinite(yaw) || !Float.isFinite(pitch)) return false;
        double yawRad = -Math.toRadians(yaw);
        double pitchRad = -Math.toRadians(pitch);
        double cosPitch = Math.cos(pitchRad);
        double dx = Math.sin(yawRad) * cosPitch;
        double dy = -Math.sin(Math.toRadians(pitch));
        double dz = Math.cos(yawRad) * cosPitch;
        double reach = (double) crystalEntityRange.invoke(crystalPlayer) + 0.05;
        if (!Double.isFinite(reach) || reach <= 0) reach = 3.05;
        return rayIntersectsAABB(ox, oy, oz, dx, dy, dz, lx, ly, lz, hx, hy, hz, reach);
    }
    private boolean breakCrystal(Object c, Object e, long now, int delay) throws ReflectiveOperationException {
        if (!crystalSession(c) || !crystalMatches(e) || !crystalVisible(e) || !crystalAimingAt(c, e)) return false;
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
            crystalTickActive = false;
            crystalRequireRelease = held;
            try { resetCrystal(restore); } finally { releaseCrystalUse(c, kind, keyCode, physicalUse); }
            return;
        }
        if (crystalFailed || crystalRequireRelease) return;
        crystalAutoSwitchArg = autoSwitch;
        crystalRestoreArg = restore;
        crystalPlaceDelayArg = placeDelay;
        crystalBreakDelayArg = breakDelay;
        crystalKindArg = kind;
        crystalKeyCodeArg = keyCode;
        crystalPhysicalUseArg = physicalUse;
        crystalTickActive = true;
        crystalLastConfigTime = System.nanoTime();

        if (!clientTickHooked) {
            tickCrystalDirect(c, active, held, kind, keyCode, physicalUse, autoSwitch, restore, placeDelay, breakDelay);
        }
    }

    private void tickCrystalDirect(Object c, boolean active, boolean held, int kind, int keyCode, boolean physicalUse,
        boolean autoSwitch, boolean restore, int placeDelay, int breakDelay)
        throws ReflectiveOperationException {
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
    private boolean triggerCycleSeeded;
    private boolean triggerFailed;
    private boolean triggerSimulationPrepared;
    private Field triggerMouse, triggerMouseLeft, triggerBindings, triggerBindingPressed, triggerBindingCount, triggerBindingKey;
    private Method triggerMouseCallback, triggerKeyCode, triggerKeyCategory;
    private Constructor<?> triggerMouseInputConstructor;
    private Object triggerMouseKeyType;
    private boolean triggerTickHooked;
    private boolean clientTickHooked;
    private volatile boolean crystalTickActive;
    private volatile long crystalLastConfigTime;
    private volatile boolean crystalAutoSwitchArg, crystalRestoreArg;
    private volatile int crystalPlaceDelayArg, crystalBreakDelayArg;
    private volatile int crystalKindArg, crystalKeyCodeArg;
    private volatile boolean crystalPhysicalUseArg;
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

    private void prepareTickLifecycle() {
        if (clientTickHooked) return;
        try {
            Class<?> eventsClass = Class.forName("net.fabricmc.fabric.api.client.event.lifecycle.v1.ClientTickEvents", true, loader);
            Class<?> listenerType = Class.forName("net.fabricmc.fabric.api.client.event.lifecycle.v1.ClientTickEvents$StartTick", true, loader);
            Object event = eventsClass.getField("START_CLIENT_TICK").get(null);
            ClassLoader proxyLoader = listenerType.getClassLoader() != null ? listenerType.getClassLoader() : WorldBridge.class.getClassLoader();
            Object proxy = Proxy.newProxyInstance(proxyLoader, new Class<?>[]{listenerType}, (p, m, a) -> {
                if (m.getDeclaringClass() == Object.class) {
                    if (m.getName().equals("hashCode")) return System.identityHashCode(p);
                    if (m.getName().equals("equals")) return p == a[0];
                    return "WorldBridgeTick";
                }
                if (a != null && a.length >= 1) {
                    onTriggerTick(a[0]);
                    onCrystalTick(a[0]);
                    onSafeAnchorTick(a[0]);
                    onAuraTick(a[0]);
                }
                return null;
            });
            Class.forName("net.fabricmc.fabric.api.event.Event", true, loader).getMethod("register", Object.class).invoke(event, proxy);
            clientTickHooked = true;
            triggerTickHooked = true;
        } catch (Throwable ignored) {
            clientTickHooked = false;
            triggerTickHooked = false;
        }
    }

    private void prepareTriggerLifecycle() {
        prepareTickLifecycle();
    }

    private native void safeAnchorNativeTick();
    private volatile boolean safeAnchorTickActive;
    public void setSafeAnchorActive(boolean active) {
        safeAnchorTickActive = active;
        if (active) {
            prepareTickLifecycle();
            try { installAnchorEvents(); } catch (Throwable ignored) {}
        }
    }
    public boolean isSafeAnchorActive() {
        return safeAnchorTickActive && (clientTickHooked || legitEvents != null);
    }
    private void onSafeAnchorTick(Object c) {
        if (!safeAnchorTickActive || stopped || c == null) return;
        try {
            safeAnchorNativeTick();
        } catch (Throwable ignored) {}
    }

    private void onCrystalTick(Object c) {
        if (!crystalTickActive || crystalFailed || stopped || c == null) return;
        final long now = System.nanoTime();
        if (now - crystalLastConfigTime > 200_000_000L) {
            try { cancelCrystal(crystalKindArg, crystalKeyCodeArg, crystalPhysicalUseArg); } catch (ReflectiveOperationException ignored) {}
            return;
        }
        try {
            tickCrystalDirect(c, true, true, crystalKindArg, crystalKeyCodeArg, crystalPhysicalUseArg,
                crystalAutoSwitchArg, crystalRestoreArg, crystalPlaceDelayArg, crystalBreakDelayArg);
        } catch (Throwable t) {
            crystalFailed = true;
            try { cancelCrystal(crystalKindArg, crystalKeyCodeArg, crystalPhysicalUseArg); } catch (Throwable cleanup) { t.addSuppressed(cleanup); }
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
        triggerCycleSeeded = true;
    }
    private void ensureTriggerCycle(long now, float low, float high, int reactionLow, int reactionHigh) {
        // A new target/slot is not a new attack. Keep the pending sample and deadline;
        // only an actual attack (ours or observed manual input) starts another cycle.
        if (!triggerCycleSeeded) triggerCycle(now, low, high, reactionLow, reactionHigh);
        else triggerThreshold = Math.max(low, Math.min(high, triggerThreshold));
    }
    public void cancelTrigger() throws ReflectiveOperationException {
        triggerActive = false;
        resetTriggerCycle();
    }
    private void resetTriggerCycle() throws ReflectiveOperationException {
        // Do not retain targets or JNI objects across menus, focus/session loss or disable.
        try {
            Object c = client();
            triggerService(5, triggerPlayer != null ? triggerPlayer : c != null ? player.get(c) : null,
                triggerWorld != null ? triggerWorld : c != null ? world.get(c) : null, null, 0, 0);
        } finally {
            triggerPlayer = triggerWorld = triggerManager = triggerTarget = triggerInv = null;
            triggerSlot = triggerPreparedSlot = triggerPreparationTicks = triggerLastTicks = -1;
            // Timing contains no game references and must survive target loss,
            // cancellation and helper restoration. Never reroll it on reacquisition.
            triggerLastCooldown = 0; triggerCritical = -1;
        }
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
                resetTriggerCycle();
                triggerPlayer = p; triggerWorld = w; triggerManager = m; triggerTarget = e; triggerInv = inv;
                triggerSlot = slot; triggerGeneration = generation; triggerManual = manual;
                triggerLastTicks = ticks; triggerLastCooldown = cd;
                if (++triggerEpoch <= 0) throw new IllegalStateException("Trigger epoch exhausted");
                ensureTriggerCycle(now, low, high, reactionLow, reactionHigh);
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
            if (swingHand) triggerSwing.invoke(p, triggerHand);
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

    // Independent OneStep controller; Argon AutoJumpReset is a behavior reference.
    // Physical input is read from GLFW; all game input writes stay in Java.
    // The native callback exposes authorization/cancellation only.
    protected native boolean jumpPermitted(long generation);
    private Class<?> jumpLiving;
    private Method jumpGetWindow, jumpGetHandle, jumpGlfwKey, jumpGlfwMouse, jumpGlfwScan, jumpGlfwAttrib;
    private int jumpScanCode = -1, jumpScanSymbol = -1;
    private long jumpWindowHandle;
    private Field jumpKey, jumpBound, jumpSymbols, jumpScans, jumpMouse, jumpHurt, jumpAge, jumpCooldown;
    private Field jumpAllowFlight, jumpFlying;
    private Method jumpCode, jumpCategory, jumpPressed, jumpSetPressed, jumpGround, jumpUsing, jumpSleeping;
    private Method jumpWater, jumpSwimming, jumpFallFlying, jumpClimbing, jumpRiding, jumpAbilities, jumpSource, jumpAttacker;
    private Object jumpPlayer, jumpWorld, jumpOptions, jumpBinding, jumpInput, jumpPreviousSource;
    private Object[] jumpOwner;
    private long jumpGeneration, jumpSourceRevision, jumpConsumedSource, jumpSourceAt;
    private long jumpReadyAt, jumpExpiresAt, jumpConfirmUntil, jumpHoldUntil, jumpNextAllowed;
    private long jumpLastGrounded = -1, jumpDamageAt = -1;
    private int jumpPreviousHurt, jumpSentAge, jumpMode, jumpChance, jumpDelay;
    private boolean jumpSeeded, jumpPending, jumpConfirmed, jumpHolding, jumpSourceLiving;
    private boolean jumpAwaitingSource, jumpOnlyCombat, jumpFailed;

    private void prepareJump() throws ReflectiveOperationException {
        Class<?> entity = load("class_1297"), binding = load("class_304");
        Class<?> input = load("class_3675$class_306"), type = load("class_3675$class_307");
        Class<?> abilities = load("class_1656"), source = load("class_1282");
        jumpLiving = load("class_1309");
        jumpKey = mappedField(load("class_315"), "field_1903", binding, false);
        jumpBound = mappedField(binding, "field_1655", input, false);
        jumpSymbols = mappedField(type, "field_1668", type, true);
        jumpScans = mappedField(type, "field_1671", type, true);
        jumpMouse = mappedField(type, "field_1672", type, true);
        jumpCode = mappedMethod(input, "method_1444", int.class);
        jumpCategory = mappedMethod(input, "method_1442", type);
        jumpPressed = mappedMethod(binding, "method_1434", boolean.class);
        jumpSetPressed = mappedMethod(binding, "method_23481", void.class, boolean.class);
        jumpHurt = mappedField(jumpLiving, "field_6235", int.class, false);
        jumpAge = mappedField(entity, "field_6012", int.class, false);
        jumpCooldown = mappedField(jumpLiving, "field_6228", int.class, false);
        jumpGround = mappedMethod(entity, "method_24828", boolean.class);
        jumpUsing = mappedMethod(jumpLiving, "method_6115", boolean.class);
        jumpSleeping = mappedMethod(jumpLiving, "method_6113", boolean.class);
        jumpWater = mappedMethod(entity, "method_5799", boolean.class);
        jumpSwimming = mappedMethod(entity, "method_5681", boolean.class);
        jumpFallFlying = mappedMethod(jumpLiving, "method_6128", boolean.class);
        jumpClimbing = mappedMethod(jumpLiving, "method_6101", boolean.class);
        jumpRiding = mappedMethod(entity, "method_5765", boolean.class);
        jumpAbilities = mappedMethod(playerType, "method_31549", abilities);
        jumpAllowFlight = mappedField(abilities, "field_7478", boolean.class, false);
        jumpFlying = mappedField(abilities, "field_7479", boolean.class, false);
        jumpSource = mappedMethod(jumpLiving, "method_6081", source);
        jumpAttacker = mappedMethod(source, "method_5529", entity);
        Class<?> window = load("class_1041");
        jumpGetWindow = mappedMethod(clientType, "method_22683", window);
        jumpGetHandle = mappedMethod(window, "method_4490", long.class);
        Class<?> glfw = Class.forName("org.lwjgl.glfw.GLFW", false, loader);
        jumpGlfwKey = jumpGlfwMethod(glfw, "glfwGetKey", long.class, int.class);
        jumpGlfwMouse = jumpGlfwMethod(glfw, "glfwGetMouseButton", long.class, int.class);
        jumpGlfwScan = jumpGlfwMethod(glfw, "glfwGetKeyScancode", int.class);
        jumpGlfwAttrib = jumpGlfwMethod(glfw, "glfwGetWindowAttrib", long.class, int.class);
    }
    private static Method jumpGlfwMethod(Class<?> glfw, String name, Class<?>... args) throws ReflectiveOperationException {
        Method m = glfw.getMethod(name, args);
        if (!Modifier.isStatic(m.getModifiers()) || m.getReturnType() != int.class || !m.trySetAccessible())
            throw new IllegalAccessException(name);
        return m;
    }
    private long jumpWindow(Object c) throws ReflectiveOperationException {
        Object window = jumpGetWindow.invoke(c);
        long handle = window == null ? 0 : (long) jumpGetHandle.invoke(window);
        // GLFW_FOCUSED. Never restore a held key into an unfocused/replaced window.
        return handle != 0 && (int) jumpGlfwAttrib.invoke(null, handle, 0x00020001) == 1 ? handle : 0;
    }
    private static boolean jumpKnownSymbol(int key) {
        return key == 32 || key == 39 || (key >= 44 && key <= 57) || key == 59 || key == 61 ||
            (key >= 65 && key <= 93) || key == 96 || key == 161 || key == 162 ||
            (key >= 256 && key <= 269) || (key >= 280 && key <= 284) || (key >= 290 && key <= 314) ||
            (key >= 320 && key <= 336) || (key >= 340 && key <= 348);
    }
    private int[] jumpDescriptor(Object input) throws ReflectiveOperationException {
        if (input == null) return null;
        Object type = jumpCategory.invoke(input); int value = (int) jumpCode.invoke(input);
        if (type == jumpSymbols.get(null) && jumpKnownSymbol(value)) return new int[]{0, value};
        if (type == jumpScans.get(null) && value > 0 && value < 512) return new int[]{1, value};
        if (type == jumpMouse.get(null) && value >= 0 && value < 8) return new int[]{2, value};
        return null;
    }
    private boolean jumpPhysical(Object input, long handle) throws ReflectiveOperationException {
        int[] descriptor = jumpDescriptor(input);
        if (descriptor == null || handle == 0) return true;
        int kind = descriptor[0], key = descriptor[1];
        if (kind == 1) {
            if (jumpScanCode != key) {
                jumpScanCode = key; jumpScanSymbol = -1;
                for (int symbol = 32; symbol <= 348; ++symbol)
                    if (jumpKnownSymbol(symbol) && (int) jumpGlfwScan.invoke(null, symbol) == key) {
                        jumpScanSymbol = symbol; break;
                    }
            }
            if (jumpScanSymbol < 0) return true;
            key = jumpScanSymbol;
        }
        int state = (int) (kind == 2 ? jumpGlfwMouse : jumpGlfwKey).invoke(null, handle, key);
        if (state != 0 && state != 1) throw new IllegalStateException("Invalid GLFW input state");
        return state == 1;
    }
    private boolean jumpSpace(Object input) throws ReflectiveOperationException {
        int[] descriptor = jumpDescriptor(input);
        return descriptor != null && ((descriptor[0] == 0 && descriptor[1] == 32) ||
            (descriptor[0] == 1 && descriptor[1] == (int) jumpGlfwScan.invoke(null, 32)));
    }
    private boolean jumpPlayable(Object c, Object p, Object w) throws ReflectiveOperationException {
        if (!live(c, p, w) || (boolean) jumpUsing.invoke(p) || (boolean) jumpSleeping.invoke(p) ||
            (boolean) jumpWater.invoke(p) || (boolean) jumpSwimming.invoke(p) ||
            (boolean) jumpFallFlying.invoke(p) || (boolean) jumpClimbing.invoke(p) || (boolean) jumpRiding.invoke(p)) return false;
        Object abilities = jumpAbilities.invoke(p);
        return abilities != null && !jumpAllowFlight.getBoolean(abilities) && !jumpFlying.getBoolean(abilities);
    }
    private boolean releaseJump() throws ReflectiveOperationException {
        if (jumpOwner != null) {
            Object c = client(); if (c == null) return false;
            // Clear our old synthetic press even if the binding object was replaced.
            // If that same binding was rebound, preserve the NEW physical key state.
            boolean current=options.get(c) == jumpOwner[0] && jumpKey.get(jumpOwner[0]) == jumpOwner[1];
            Object input=current ? jumpBound.get(jumpOwner[1]) : null;
            boolean physical=current && jumpDescriptor(input)!=null && jumpWindow(c)==(long)jumpOwner[3] &&
                jumpPhysical(input,(long)jumpOwner[3]);
            jumpSetPressed.invoke(jumpOwner[1],physical);
            jumpOwner = null;
        }
        return true;
    }
    public void cancelJump() throws ReflectiveOperationException {
        if (!prepared[5] || client() == null) return;
        try { releaseJump(); }
        finally {
            jumpSeeded = jumpPending = jumpConfirmed = jumpHolding = false;
            jumpPlayer = jumpWorld = jumpOptions = jumpBinding = jumpInput = jumpPreviousSource = null;
            jumpSourceRevision = jumpConsumedSource = 0; jumpSourceAt = 0; jumpSourceLiving = false;
            jumpLastGrounded = jumpDamageAt = -1; jumpAwaitingSource = false;
        }
    }
    public void tickJump(boolean active, long generation, int mode, int chance, int delay, boolean onlyCombat)
        throws ReflectiveOperationException {
        Object c = client(); if (c == null) return;
        if (!active) { cancelJump(); return; }
        prepare(5);
        try {
            if (jumpFailed || mode < 0 || mode > 1 || chance < 0 || chance > 100 || delay < 0 || delay > 100 ||
                chance == 0 || !jumpPermitted(generation)) { cancelJump(); return; }
            if (generation != jumpGeneration || mode != jumpMode || chance != jumpChance || delay != jumpDelay ||
                onlyCombat != jumpOnlyCombat) {
                cancelJump(); jumpGeneration = generation; jumpMode = mode; jumpChance = chance;
                jumpDelay = delay; jumpOnlyCombat = onlyCombat;
            }
            if (!jumpHolding && !releaseJump()) return;
            Object p = player.get(c), w = world.get(c), opts = options.get(c);
            long handle = jumpWindow(c);
            if (handle == 0 || !jumpPlayable(c, p, w) || opts == null) { cancelJump(); return; }
            Object key = jumpKey.get(opts), input = key == null ? null : jumpBound.get(key);
            if (jumpDescriptor(input) == null || (mode == 1 && !jumpSpace(input))) { cancelJump(); return; }
            long now = System.nanoTime() / 1_000_000L;
            int hurt = jumpHurt.getInt(p), age = jumpAge.getInt(p);
            boolean grounded = (boolean) jumpGround.invoke(p);
            if (age < 0 || hurt < 0) { cancelJump(); return; }
            if (!jumpSeeded || p != jumpPlayer || w != jumpWorld || opts != jumpOptions || key != jumpBinding || input != jumpInput || handle != jumpWindowHandle) {
                cancelJump(); if (!releaseJump()) return;
                jumpPlayer = p; jumpWorld = w; jumpOptions = opts; jumpBinding = key; jumpInput = input;
                jumpWindowHandle = handle; jumpPreviousHurt = hurt; jumpLastGrounded = grounded ? now : -1;
                jumpPreviousSource = jumpSource.invoke(p); jumpSeeded = true; return;
            }
            boolean damaged = hurt > 0 && hurt > jumpPreviousHurt; jumpPreviousHurt = hurt;
            if (grounded) jumpLastGrounded = now;
            Object source = jumpSource.invoke(p);
            if (source != null && source != jumpPreviousSource) {
                Object attacker = jumpAttacker.invoke(source);
                jumpSourceLiving = attacker != p && jumpLiving.isInstance(attacker);
                jumpPreviousSource = source; ++jumpSourceRevision; jumpSourceAt = now;
            }
            boolean livingDamage = source != null && jumpSourceLiving && jumpSourceRevision != jumpConsumedSource && now - jumpSourceAt <= 105;
            boolean physical = jumpPhysical(input, handle);
            if (jumpHolding) {
                if (physical || age > jumpSentAge || now >= jumpHoldUntil || age < jumpSentAge) {
                    jumpHolding = false; releaseJump();
                }
                return;
            }
            if (physical || (boolean) jumpPressed.invoke(key)) { jumpPending = false; return; }
            if (damaged) {
                jumpDamageAt = now; jumpAwaitingSource = !livingDamage;
                jumpPending = false;
                if (now >= jumpNextAllowed && (grounded || (jumpLastGrounded >= 0 && now - jumpLastGrounded <= 85)) &&
                    (chance == 100 || java.util.concurrent.ThreadLocalRandom.current().nextInt(1, 101) <= chance)) {
                    jumpPending = true; jumpConfirmed = !onlyCombat || livingDamage;
                    jumpConfirmUntil = now + 105; jumpReadyAt = now + delay; jumpExpiresAt = now + 170;
                }
            }
            if (livingDamage && (damaged || (jumpPending && jumpConfirmed) ||
                (jumpAwaitingSource && jumpDamageAt >= 0 && now - jumpDamageAt <= 105))) {
                jumpConsumedSource = jumpSourceRevision; jumpAwaitingSource = false;
            }
            if (!jumpPending) return;
            if (now >= jumpExpiresAt || hurt == 0) { jumpPending = false; return; }
            if (!jumpConfirmed) {
                if (now >= jumpConfirmUntil) { jumpPending = false; return; }
                jumpConfirmed = livingDamage; if (!jumpConfirmed) return;
            }
            if (now < jumpReadyAt || !grounded || jumpCooldown.getInt(p) != 0) return;
            // Final client-thread revalidation; consume this hit before mutation.
            if (client() != c || player.get(c) != p || world.get(c) != w || options.get(c) != opts ||
                jumpKey.get(opts) != key || jumpBound.get(key) != input || !jumpPlayable(c, p, w) ||
                !(boolean) jumpGround.invoke(p) || jumpCooldown.getInt(p) != 0 || jumpWindow(c) != handle || jumpPhysical(input, handle) ||
                (boolean) jumpPressed.invoke(key) || !jumpPermitted(generation)) {
                jumpPending = false; return;
            }
            jumpPending = false; jumpHolding = true; jumpSentAge = age;
            jumpHoldUntil = now + 200; jumpNextAllowed = now + 140;
            jumpOwner = new Object[]{opts, key, input, handle};
            jumpSetPressed.invoke(key, true);
        } catch (ReflectiveOperationException | RuntimeException error) {
            jumpFailed = true;
            try { cancelJump(); } catch (ReflectiveOperationException cleanup) { error.addSuppressed(cleanup); }
            throw error;
        }
    }

    // OneStep placement/confirmation sequence with Argon-style held-use settings.
    // Native only checks runtime authority; all input, inventory and world I/O is Java.
    protected native boolean anchorPermitted(long generation);
    private Class<?> anchorHitType, anchorShield;
    private Method anchorMainHand, anchorOffHand, anchorFoodComponent;
    private Object anchorFood;
    private Field anchorCrosshair, anchorSelected, anchorPos, anchorX, anchorY, anchorZ, anchorTicks;
    private Field anchorUseKey, anchorUseCount, anchorRightClicked;
    private Method anchorInventory, anchorStack, anchorEmpty, anchorItem, anchorSync, anchorUsing;
    private Method anchorStateAt, anchorIsOf, anchorReplaceable, anchorProperty, anchorSide, anchorDx, anchorDy, anchorDz;
    private Method anchorGetPos, anchorGetType, anchorGetX, anchorGetY, anchorGetZ, anchorEyeY, anchorRange;
    private Method anchorRaycast, anchorInteract, anchorAccepted, anchorSwing, anchorPlayers, anchorAlive;
    private Method anchorPressed, anchorSetPressed, anchorGetWindow, anchorGetHandle, anchorGlfwMouse, anchorGlfwKey, anchorGlfwScan, anchorGlfwAttrib;
    private Constructor<?> anchorPosCtor, anchorVecCtor, anchorHitCtor, anchorRayCtor;
    private Object anchorItemValue, anchorGlowstone, anchorBlockValue, anchorCharges, anchorHand, anchorBlockType, anchorCollider, anchorNoFluid;
    private Object[] anchorDirections;
    private Object anchorPlayer, anchorWorld, anchorManager, anchorInv, anchorTarget, anchorSupport, anchorFace;
    private Object anchorOwnershipWorld, anchorOwnershipPlayer;
    private final java.util.Set<String> anchorOwned = new java.util.HashSet<>();
    private Object[] anchorUseOwner;
    private boolean[] anchorFlags;
    private int[] anchorValues;
    private int anchorStage, anchorAfterSwitch, anchorDesiredSlot, anchorOldSlot = -1, anchorOwnedSlot = -1;
    private int anchorDueTick, anchorLastTick = -1;
    private long anchorGeneration, anchorWindowHandle, anchorStarted, anchorWaiting;
    private boolean anchorRequireRelease, anchorFailed;

    private void prepareAnchor() throws ReflectiveOperationException {
        Class<?> entity = load("class_1297"), inv = load("class_1661"), stack = load("class_1799");
        Class<?> pos = load("class_2338"), vec = load("class_243"), state = load("class_2680"), block = load("class_2248");
        Class<?> hit = load("class_239"), type = load("class_239$class_240"), dir = load("class_2350"), hand = load("class_1268");
        Class<?> w = load("class_638"), m = load("class_636"), ray = load("class_3959"), result = load("class_1269");
        Class<?> shape = load("class_3959$class_3960"), fluid = load("class_3959$class_242"), itemType = load("class_1792");
        Class<?> binding = load("class_304"), input = load("class_3675$class_306"), inputType = load("class_3675$class_307");
        anchorHitType = load("class_3965");
        anchorCrosshair = mappedField(clientType,"field_1765",hit,false);
        anchorSelected = mappedField(inv,"field_7545",int.class,false);
        anchorPos = mappedField(entity,"field_22467",vec,false);
        anchorX = mappedField(vec,"field_1352",double.class,false); anchorY = mappedField(vec,"field_1351",double.class,false);
        anchorZ = mappedField(vec,"field_1350",double.class,false); anchorTicks = mappedField(entity,"field_6012",int.class,false);
        anchorInventory = mappedMethod(playerType,"method_31548",inv); anchorStack = mappedMethod(inv,"method_5438",stack,int.class);
        anchorEmpty = mappedMethod(stack,"method_7960",boolean.class); anchorItem = mappedMethod(stack,"method_7909",itemType);
        anchorSync = mappedMethod(m,"method_2911",void.class); anchorUsing = mappedMethod(playerType,"method_6115",boolean.class);
        anchorMainHand = mappedMethod(playerType,"method_6047",stack); anchorOffHand = mappedMethod(playerType,"method_6079",stack);
        anchorShield = load("class_1819");
        Class<?> componentType = load("class_9331");
        anchorFood = mappedField(load("class_9334"),"field_50075",componentType,true).get(null);
        anchorFoodComponent = mappedMethod(stack,target >= 12108 ? "method_58694" : "method_57824",Object.class,componentType);
        anchorStateAt = mappedMethod(w,"method_8320",state,pos);
        anchorIsOf = mappedMethod(state,"method_27852",boolean.class,target >= 26100 ? Object.class : block);
        anchorReplaceable = mappedMethod(state,"method_45474",boolean.class);
        anchorProperty = mappedMethod(state,"method_11654",Comparable.class,load("class_2769"));
        anchorGetPos = mappedMethod(anchorHitType,"method_17777",pos); anchorSide = mappedMethod(anchorHitType,"method_17780",dir);
        anchorGetType = mappedMethod(hit,"method_17783",type);
        anchorGetX = mappedMethod(pos,"method_10263",int.class); anchorGetY = mappedMethod(pos,"method_10264",int.class);
        anchorGetZ = mappedMethod(pos,"method_10260",int.class);
        anchorDx = mappedMethod(dir,"method_10148",int.class); anchorDy = mappedMethod(dir,"method_10164",int.class);
        anchorDz = mappedMethod(dir,"method_10165",int.class);
        anchorEyeY = mappedMethod(entity,"method_23320",double.class); anchorRange = mappedMethod(playerType,"method_55754",double.class);
        anchorRaycast = mappedMethod(w,"method_17742",anchorHitType,ray);
        anchorInteract = mappedMethod(m,"method_2896",result,playerType,hand,anchorHitType);
        anchorAccepted = mappedMethod(result,"method_23665",boolean.class); anchorSwing = mappedMethod(playerType,"method_6104",void.class,hand);
        anchorPlayers = mappedMethod(w,"method_18456",List.class); anchorAlive = mappedMethod(entity,"method_5805",boolean.class);
        anchorPosCtor = constructor(pos,int.class,int.class,int.class); anchorVecCtor = constructor(vec,double.class,double.class,double.class);
        anchorHitCtor = constructor(anchorHitType,vec,dir,pos,boolean.class);
        anchorRayCtor = constructor(ray,vec,vec,shape,fluid,entity);
        anchorItemValue = mappedField(load("class_1802"),"field_23141",itemType,true).get(null);
        anchorGlowstone = mappedField(load("class_1802"),"field_8801",itemType,true).get(null);
        anchorBlockValue = mappedField(load("class_2246"),"field_23152",block,true).get(null);
        anchorCharges = mappedField(load("class_4969"),"field_23153",load("class_2758"),true).get(null);
        anchorHand = mappedField(hand,"field_5808",hand,true).get(null);
        anchorBlockType = mappedField(type,"field_1332",type,true).get(null);
        anchorCollider = mappedField(shape,"field_17558",shape,true).get(null);
        anchorNoFluid = mappedField(fluid,"field_1348",fluid,true).get(null);
        anchorDirections = new Object[6]; String[] directions = {"field_11033","field_11036","field_11043","field_11035","field_11039","field_11034"};
        for (int i=0;i<6;++i) anchorDirections[i] = mappedField(dir,directions[i],dir,true).get(null);
        anchorUseKey = mappedField(load("class_315"),"field_1904",binding,false);
        anchorUseCount = mappedField(binding,"field_1661",int.class,false);
        anchorPressed = mappedMethod(binding,"method_1434",boolean.class); anchorSetPressed = mappedMethod(binding,"method_23481",void.class,boolean.class);
        bound = mappedField(binding,"field_1655",input,false);
        code = mappedMethod(input,"method_1444",int.class); category = mappedMethod(input,"method_1442",inputType);
        keySymbol = mappedField(inputType,"field_1668",inputType,true); scanCode = mappedField(inputType,"field_1671",inputType,true);
        mouse = mappedField(inputType,"field_1672",inputType,true);
        Class<?> window = load("class_1041"), glfw = Class.forName("org.lwjgl.glfw.GLFW",false,loader);
        anchorGetWindow = mappedMethod(clientType,"method_22683",window); anchorGetHandle = mappedMethod(window,"method_4490",long.class);
        anchorGlfwMouse = jumpGlfwMethod(glfw,"glfwGetMouseButton",long.class,int.class);
        anchorGlfwKey = jumpGlfwMethod(glfw,"glfwGetKey",long.class,int.class);
        anchorGlfwScan = jumpGlfwMethod(glfw,"glfwGetKeyScancode",int.class);
        anchorGlfwAttrib = jumpGlfwMethod(glfw,"glfwGetWindowAttrib",long.class,int.class);
        if (anchorItemValue == null || anchorGlowstone == null || anchorBlockValue == null || anchorCharges == null)
            throw new IllegalStateException("Uninitialized anchor bindings");
    }
    private long anchorWindow(Object c) throws ReflectiveOperationException {
        Object w = anchorGetWindow.invoke(c); long handle = w == null ? 0 : (long) anchorGetHandle.invoke(w);
        return handle != 0 && (int) anchorGlfwAttrib.invoke(null,handle,0x00020001) == 1 ? handle : 0;
    }
    private boolean anchorPhysical(Object input,long handle) throws ReflectiveOperationException {
        if (handle == 0) return false;
        int[] key = descriptor(input); int kind = key[0], value = key[1];
        if (kind == 1) {
            int symbol = -1;
            for (int i=32;i<=348;++i) if (jumpKnownSymbol(i) && (int) anchorGlfwScan.invoke(null,i) == value) { symbol=i; break; }
            if (symbol < 0) return false; value=symbol;
        }
        if ((kind == 0 && !jumpKnownSymbol(value)) || kind < 0 || (kind == 2 && (value < 0 || value > 7))) return false;
        return (int) (kind == 2 ? anchorGlfwMouse : anchorGlfwKey).invoke(null,handle,value) == 1;
    }
    private boolean anchorSame(Object c) throws ReflectiveOperationException {
        return c != null && live(c,anchorPlayer,anchorWorld) && player.get(c) == anchorPlayer && world.get(c) == anchorWorld &&
            manager.get(c) == anchorManager && anchorInventory.invoke(anchorPlayer) == anchorInv &&
            anchorSelected.getInt(anchorInv) == anchorOwnedSlot && anchorWindow(c) == anchorWindowHandle;
    }
    private boolean suppressAnchorUse(Object c) throws ReflectiveOperationException {
        Object opts = options.get(c), key = opts == null ? null : anchorUseKey.get(opts), input = key == null ? null : bound.get(key);
        if (input == null || (anchorUseOwner != null && (opts != anchorUseOwner[0] || key != anchorUseOwner[1] || input != anchorUseOwner[2]))) return false;
        if (anchorUseOwner == null) anchorUseOwner = new Object[]{opts,key,input,anchorWindow(c)};
        anchorSetPressed.invoke(key,false); anchorUseCount.setInt(key,0); return true;
    }
    private void releaseAnchorUse(Object c) throws ReflectiveOperationException {
        if (anchorUseOwner == null || c == null) return;
        Object[] owner = anchorUseOwner;
        if (options.get(c) == owner[0] && anchorUseKey.get(owner[0]) == owner[1] && bound.get(owner[1]) == owner[2])
            anchorSetPressed.invoke(owner[1],anchorWindow(c) == (long) owner[3] && anchorPhysical(owner[2],(long) owner[3]));
        anchorUseOwner = null;
    }
    private void resetAnchorSlot() throws ReflectiveOperationException {
        Object p=anchorPlayer,w=anchorWorld,m=anchorManager,inv=anchorInv; int old=anchorOldSlot,owned=anchorOwnedSlot;
        anchorPlayer=anchorWorld=anchorManager=anchorInv=anchorTarget=anchorSupport=anchorFace=null;
        anchorOldSlot=anchorOwnedSlot=-1; anchorStage=0;
        Object c=client();
        if (c != null && inv != null && old >= 0 && old < 9 && player.get(c) == p && world.get(c) == w && manager.get(c) == m &&
            anchorInventory.invoke(p) == inv && anchorSelected.getInt(inv) == owned) {
            anchorSelected.setInt(inv,old); anchorSync.invoke(m);
        }
    }
    public void cancelAnchor() throws ReflectiveOperationException {
        if (!prepared[6]) return;
        Object c=client(); if (c == null) return;
        anchorRequireRelease = true;
        try { cancelAnchorLegit(); } finally {
            try { resetAnchorSlot(); } finally { releaseAnchorUse(c); }
        }
    }
    private String anchorKey(Object pos) throws ReflectiveOperationException {
        return anchorGetX.invoke(pos)+":"+anchorGetY.invoke(pos)+":"+anchorGetZ.invoke(pos);
    }
    private boolean anchorEqual(Object a,Object b) throws ReflectiveOperationException {
        return a != null && b != null && anchorKey(a).equals(anchorKey(b));
    }
    private int anchorCharge(Object pos) throws ReflectiveOperationException {
        Object state=anchorStateAt.invoke(anchorWorld,pos);
        if (state == null || !(boolean) anchorIsOf.invoke(state,anchorBlockValue)) return -1;
        Object value=anchorProperty.invoke(state,anchorCharges);
        if (!(value instanceof Integer n) || n < 0 || n > 4) throw new IllegalStateException("Unknown anchor charge");
        return n;
    }
    private Object anchorHit(Object pos,Object side) throws ReflectiveOperationException {
        int x=(int) anchorGetX.invoke(pos),y=(int) anchorGetY.invoke(pos),z=(int) anchorGetZ.invoke(pos);
        int dx=(int) anchorDx.invoke(side),dy=(int) anchorDy.invoke(side),dz=(int) anchorDz.invoke(side);
        double tx=x+.5+dx*.5,ty=y+.5+dy*.5,tz=z+.5+dz*.5;
        Object v=anchorPos.get(anchorPlayer); if (v == null) return null;
        double ex=anchorX.getDouble(v),ey=(double) anchorEyeY.invoke(anchorPlayer),ez=anchorZ.getDouble(v);
        double reach=(double) anchorRange.invoke(anchorPlayer),distance=(ex-tx)*(ex-tx)+(ey-ty)*(ey-ty)+(ez-tz)*(ez-tz);
        if (!Double.isFinite(distance) || !Double.isFinite(reach) || reach <= 0 || distance > reach*reach) return null;
        Object from=anchorVecCtor.newInstance(ex,ey,ez),to=anchorVecCtor.newInstance(x+.5+dx*.49,y+.5+dy*.49,z+.5+dz*.49);
        Object ray=anchorRayCtor.newInstance(from,to,anchorCollider,anchorNoFluid,anchorPlayer);
        Object visible=anchorRaycast.invoke(anchorWorld,ray);
        if (visible == null || anchorGetType.invoke(visible) != anchorBlockType || !anchorEqual(anchorGetPos.invoke(visible),pos)) return null;
        return anchorHitCtor.newInstance(anchorVecCtor.newInstance(tx,ty,tz),side,pos,false);
    }
    private Object anchorNearSide() throws ReflectiveOperationException {
        Object v=anchorPos.get(anchorPlayer);
        double dx=anchorX.getDouble(v)-((int) anchorGetX.invoke(anchorTarget)+.5);
        double dz=anchorZ.getDouble(v)-((int) anchorGetZ.invoke(anchorTarget)+.5);
        return Math.abs(dx) >= Math.abs(dz) ? anchorDirections[dx >= 0 ? 5 : 4] : anchorDirections[dz >= 0 ? 3 : 2];
    }
    private int anchorFind(Object item) throws ReflectiveOperationException {
        for (int i=0;i<9;++i) if (anchorHeld(i,item)) return i;
        return -1;
    }
    private boolean anchorHeld(int slot,Object item) throws ReflectiveOperationException {
        if (slot < 0 || slot > 8) return false;
        Object s=anchorStack.invoke(anchorInv,slot);
        return s != null && !(boolean) anchorEmpty.invoke(s) && anchorItem.invoke(s) == item;
    }
    private boolean anchorFoodOrShield(Object stack) throws ReflectiveOperationException {
        return stack != null && !(boolean) anchorEmpty.invoke(stack) &&
            (anchorShield.isInstance(anchorItem.invoke(stack)) || anchorFoodComponent.invoke(stack,anchorFood) != null);
    }
    private boolean anchorUsingAllowed() throws ReflectiveOperationException {
        return anchorFlags[0] || (!(boolean) anchorUsing.invoke(anchorPlayer) &&
            !anchorFoodOrShield(anchorMainHand.invoke(anchorPlayer)) && !anchorFoodOrShield(anchorOffHand.invoke(anchorPlayer)));
    }
    private boolean anchorDeadNearby() throws ReflectiveOperationException {
        Object values=anchorPlayers.invoke(anchorWorld);
        if (!(values instanceof List<?> list) || list.size() > 4096) return true;
        Object p=anchorPos.get(anchorPlayer);
        for (Object e:list) if (e != null && e != anchorPlayer && !(boolean) anchorAlive.invoke(e)) {
            Object v=anchorPos.get(e); if (v == null) continue;
            double dx=anchorX.getDouble(v)-anchorX.getDouble(p),dy=anchorY.getDouble(v)-anchorY.getDouble(p),dz=anchorZ.getDouble(v)-anchorZ.getDouble(p);
            if (Double.isFinite(dx+dy+dz) && dx*dx+dy*dy+dz*dz <= 36) return true;
        }
        return false;
    }
    private static boolean anchorRoll(int chance) {
        return chance == 100 || (chance > 0 && java.util.concurrent.ThreadLocalRandom.current().nextInt(1,101) <= chance);
    }
    private void anchorSchedule(int action,int slot,int tick) throws ReflectiveOperationException {
        anchorStage=1; anchorAfterSwitch=action; anchorDesiredSlot=slot;
        anchorDueTick=tick+(anchorSelected.getInt(anchorInv) == slot ? 0 : anchorValues[1]);
    }
    private void finishAnchor() throws ReflectiveOperationException {
        anchorRequireRelease=true; resetAnchorSlot(); // Keep the vanilla-use gate until physical release.
    }
    private void anchorClick(Object c) throws ReflectiveOperationException {
        prepareTriggerSimulation();
        if (anchorRightClicked == null) anchorRightClicked=mappedField(load("class_312"),"field_1788",boolean.class,false);
        Object mouse=triggerMouse.get(c),registry=triggerBindings.get(null);
        if (mouse == null || !(registry instanceof Map<?,?> mappings)) throw new IllegalStateException("Anchor click callback unavailable");
        java.util.ArrayList<Object[]> snapshots=new java.util.ArrayList<>();
        for (Object binding:mappings.values()) if (binding != null) {
            Object key=triggerBindingKey.get(binding);
            if (key != null && triggerKeyCategory.invoke(key) == triggerMouseKeyType && (int) triggerKeyCode.invoke(key) == 1)
                snapshots.add(new Object[]{binding,key,triggerBindingPressed.getBoolean(binding),triggerBindingCount.getInt(binding)});
        }
        long handle=anchorWindowHandle; Object input=target >= 12111 ? triggerMouseInputConstructor.newInstance(1,0) : null;
        boolean prior=anchorRightClicked.getBoolean(mouse); ReflectiveOperationException error=null;
        try {
            if (target >= 12111) triggerMouseCallback.invoke(mouse,handle,input,1);
            else triggerMouseCallback.invoke(mouse,handle,1,1,0);
        } catch (ReflectiveOperationException failure) { error=failure; }
        finally {
            try {
                if (target >= 12111) triggerMouseCallback.invoke(mouse,handle,input,0);
                else triggerMouseCallback.invoke(mouse,handle,1,0,0);
            } catch (ReflectiveOperationException failure) {
                if (error == null) error=failure; else error.addSuppressed(failure);
            }
            boolean same=false,held=false;
            try {
                same=anchorSame(client()) && anchorPermitted(anchorGeneration);
                held=same && (int) anchorGlfwMouse.invoke(null,handle,1) == 1;
            } catch (ReflectiveOperationException failure) {
                if (error == null) error=failure; else error.addSuppressed(failure);
            }
            for (Object[] saved:snapshots) try {
                if (triggerBindingKey.get(saved[0]) == saved[1]) {
                    triggerBindingPressed.setBoolean(saved[0],held && (boolean) saved[2]);
                    triggerBindingCount.setInt(saved[0],same ? (int) saved[3] : 0);
                }
            } catch (ReflectiveOperationException failure) {
                if (error == null) error=failure; else error.addSuppressed(failure);
            }
            try { anchorRightClicked.setBoolean(mouse,held && prior); }
            catch (ReflectiveOperationException failure) {
                if (error == null) error=failure; else error.addSuppressed(failure);
            }
        }
        if (error != null) throw error;
    }
    private boolean anchorUse(Object c,Object hit) throws ReflectiveOperationException {
        if (hit == null || !anchorSame(client()) || !anchorPermitted(anchorGeneration) ||
            (int) anchorGlfwMouse.invoke(null,anchorWindowHandle,1) != 1 ||
            !anchorUsingAllowed() || (anchorFlags[1] && anchorDeadNearby())) return false;
        anchorLastTick=anchorTicks.getInt(anchorPlayer); anchorWaiting=System.nanoTime();
        Object result=anchorInteract.invoke(anchorManager,anchorPlayer,anchorHand,hit);
        if (result == null || !(boolean) anchorAccepted.invoke(result)) return false;
        if (!anchorSame(client())) return false;
        anchorSwing.invoke(anchorPlayer,anchorHand);
        if (anchorFlags[2]) anchorClick(c);
        return true;
    }
    private void tickAnchorNormal(boolean active,long generation,boolean[] flags,int[] values) throws ReflectiveOperationException {
        Object c=client(); if (c == null) return;
        if (!active) { cancelAnchor(); return; }
        prepare(6);
        try {
            if (flags == null || flags.length != 5 || values == null || values.length != 8) throw new IllegalArgumentException("Anchor config shape");
            for (int i=0;i<8;++i) {
                int max=(i == 1 || i == 3 || i == 5) ? 20 : i == 7 ? 9 : 100;
                if (values[i] < (i == 7 ? 1 : 0) || values[i] > max) throw new IllegalArgumentException("Anchor config range");
            }
            boolean changed=anchorFlags != null && (generation != anchorGeneration || !java.util.Arrays.equals(flags,anchorFlags) || !java.util.Arrays.equals(values,anchorValues));
            if (changed) cancelAnchor();
            anchorGeneration=generation; anchorFlags=flags.clone(); anchorValues=values.clone();
            if (anchorFailed || !anchorPermitted(generation)) { cancelAnchor(); return; }
            Object p=player.get(c),w=world.get(c),m=manager.get(c); long handle=anchorWindow(c);
            if (!live(c,p,w) || m == null || handle == 0) { cancelAnchor(); return; }
            if (anchorOwnershipWorld != w || anchorOwnershipPlayer != p) {
                anchorOwned.clear(); anchorOwnershipWorld=w; anchorOwnershipPlayer=p;
            }
            boolean right=(int) anchorGlfwMouse.invoke(null,handle,1) == 1;
            if (!right) { cancelAnchor(); anchorRequireRelease=false; return; }
            if (anchorRequireRelease) { if (anchorUseOwner != null && !suppressAnchorUse(c)) cancelAnchor(); return; }
            if (anchorStage != 0 && !anchorSame(c)) { cancelAnchor(); return; }
            if (anchorStage == 0) {
                anchorPlayer=p; anchorWorld=w; anchorManager=m; anchorInv=anchorInventory.invoke(p);
                if (anchorInv == null) { cancelAnchor(); return; }
                anchorOldSlot=anchorOwnedSlot=anchorSelected.getInt(anchorInv); anchorWindowHandle=handle;
                if (anchorOldSlot < 0 || anchorOldSlot > 8) { cancelAnchor(); return; }
            }
            if (!anchorUsingAllowed() || (flags[1] && anchorDeadNearby())) { cancelAnchor(); return; }
            if (!suppressAnchorUse(c)) { cancelAnchor(); return; }
            int tick=anchorTicks.getInt(p); long now=System.nanoTime();
            if (tick < 0 || (anchorStage != 0 && (tick < anchorLastTick || now-anchorStarted > 10_000_000_000L))) { finishAnchor(); return; }
            if (anchorStage == 0) {
                Object cross=anchorCrosshair.get(c);
                if (!anchorHitType.isInstance(cross) || anchorGetType.invoke(cross) != anchorBlockType) { finishAnchor(); return; }
                anchorSupport=anchorGetPos.invoke(cross); anchorFace=anchorSide.invoke(cross);
                if (anchorSupport == null || anchorFace == null) { finishAnchor(); return; }
                int charge=anchorCharge(anchorSupport);
                if (charge >= 0) anchorTarget=anchorSupport;
                else {
                    int x=(int) anchorGetX.invoke(anchorSupport)+(int) anchorDx.invoke(anchorFace);
                    int y=(int) anchorGetY.invoke(anchorSupport)+(int) anchorDy.invoke(anchorFace);
                    int z=(int) anchorGetZ.invoke(anchorSupport)+(int) anchorDz.invoke(anchorFace);
                    anchorTarget=anchorPosCtor.newInstance(x,y,z);
                    Object emptyState=anchorStateAt.invoke(w,anchorTarget);
                    // Fire occupies a non-air state but is a valid vanilla placement destination.
                    if (emptyState == null || !(boolean) anchorReplaceable.invoke(emptyState)) { finishAnchor(); return; }
                }
                if (charge >= 0 && flags[3] && !anchorOwned.contains(anchorKey(anchorTarget))) { finishAnchor(); return; }
                anchorStarted=now; anchorLastTick=-1;
                if (charge < 1 && !anchorRoll(values[0])) { finishAnchor(); return; }
                if (charge < 0) anchorSchedule(2,anchorFind(anchorItemValue),tick);
                else if (charge == 0) anchorSchedule(4,anchorFind(anchorGlowstone),tick);
                else if (flags[4]) finishAnchor();
                else anchorSchedule(6,values[7]-1,tick);
                return;
            }
            if (tick == anchorLastTick) return;
            if (anchorStage == 1) {
                if (tick < anchorDueTick) return;
                int slot=anchorDesiredSlot;
                if (slot < 0 || slot > 8 || (anchorAfterSwitch == 2 && !anchorHeld(slot,anchorItemValue)) ||
                    (anchorAfterSwitch == 4 && !anchorHeld(slot,anchorGlowstone)) ||
                    (anchorAfterSwitch == 6 && anchorHeld(slot,anchorGlowstone))) { finishAnchor(); return; }
                if (anchorSelected.getInt(anchorInv) != slot) {
                    if (!anchorRoll(values[2]) || !anchorPermitted(generation) || !anchorSame(client())) { finishAnchor(); return; }
                    anchorOwnedSlot=slot; anchorSelected.setInt(anchorInv,slot); anchorLastTick=tick; anchorSync.invoke(m);
                }
                anchorStage=anchorAfterSwitch;
                anchorDueTick=tick+(anchorStage == 4 ? values[3] : anchorStage == 6 ? values[5] : 0);
                return;
            }
            if (tick < anchorDueTick) return;
            if (anchorStage == 2) {
                Object state=anchorStateAt.invoke(w,anchorTarget);
                if (!anchorHeld(anchorOwnedSlot,anchorItemValue) || state == null || !(boolean) anchorReplaceable.invoke(state)) { finishAnchor(); return; }
                Object hit=anchorHit(anchorSupport,anchorFace); anchorStage=3;
                if (!anchorUse(c,hit)) finishAnchor(); return;
            }
            if (anchorStage == 3) {
                if (anchorCharge(anchorTarget) >= 0) {
                    if (anchorOwned.size() >= 256) anchorOwned.clear(); anchorOwned.add(anchorKey(anchorTarget));
                    anchorSchedule(4,anchorFind(anchorGlowstone),tick);
                } else if (now-anchorWaiting >= 650_000_000L) finishAnchor();
                return;
            }
            if (anchorStage == 4) {
                if (anchorCharge(anchorTarget) != 0 || !anchorHeld(anchorOwnedSlot,anchorGlowstone) || !anchorRoll(values[4])) { finishAnchor(); return; }
                Object hit=anchorHit(anchorTarget,anchorNearSide()); anchorStage=5;
                if (!anchorUse(c,hit)) finishAnchor(); return;
            }
            if (anchorStage == 5) {
                int charge=anchorCharge(anchorTarget);
                if (charge > 0) {
                    if (flags[4]) finishAnchor(); else anchorSchedule(6,values[7]-1,tick);
                } else if (charge < 0 || now-anchorWaiting >= 650_000_000L) finishAnchor();
                return;
            }
            if (anchorStage == 6) {
                if (anchorCharge(anchorTarget) < 1 || anchorHeld(anchorOwnedSlot,anchorGlowstone) || !anchorRoll(values[6])) { finishAnchor(); return; }
                Object hit=anchorHit(anchorTarget,anchorNearSide()); anchorStage=7;
                if (!anchorUse(c,hit)) finishAnchor(); return;
            }
            if (anchorStage == 7) {
                if (anchorCharge(anchorTarget) < 0) { anchorOwned.remove(anchorKey(anchorTarget)); finishAnchor(); }
                else if (now-anchorWaiting >= 650_000_000L) finishAnchor();
            }
        } catch (ReflectiveOperationException | RuntimeException error) {
            anchorFailed=true;
            try { cancelAnchor(); } catch (ReflectiveOperationException cleanup) { error.addSuppressed(cleanup); }
            throw error;
        }
    }

    // Legit mirrors the reference event-driven controller. Normal's transaction
    // state and single-activation latch are not used by this mode.
    private int anchorExecutionMode;
    private volatile boolean legitActive;
    private boolean legitPrepared, legitFailed, legitFakeUse;
    private int legitSwitchClock, legitGlowClock, legitExplodeClock;
    private long legitGeneration;
    private boolean[] legitFlags;
    private int[] legitValues;
    private final java.util.Set<String> legitOwned = new java.util.HashSet<>();
    private final java.util.ArrayList<Object[]> legitPulses = new java.util.ArrayList<>();
    private Object[] legitUseOwner;
    private Object legitPulseMouse;
    private Class<?> legitEvents, legitSuccess;
    private Method legitStopEvents, legitReplaceable, legitDead, legitSwingTest, legitItemComponents, legitContains;
    private Object legitClientSwing;

    protected int anchorLegitPercent() { return java.util.concurrent.ThreadLocalRandom.current().nextInt(1,101); }
    public void installAnchorEvents() throws ReflectiveOperationException {
        if (legitEvents != null) return;
        Class<?> events=Class.forName("net.onestep.hybrid.AnchorEvents",true,loader);
        Method install=events.getMethod("install",Object.class,java.util.function.Consumer.class,java.util.function.Predicate.class);
        legitStopEvents=events.getMethod("stop",Object.class);
        install.invoke(null,this,(java.util.function.Consumer<Object>)this::onCombatTick,
            (java.util.function.Predicate<Object>)this::onAnchorLegitUse);
        legitEvents=events;
    }
    private void onCombatTick(Object c) {
        onTotemTick(c);
        onAnchorLegitTick(c);
        onSafeAnchorTick(c);
        onAuraTick(c);
    }
    private void prepareAnchorLegit() throws ReflectiveOperationException {
        if (legitPrepared) return;
        legitReplaceable=mappedMethod(load("class_2680"),"method_45474",boolean.class);
        legitDead=mappedMethod(load("class_1309"),"method_29504",boolean.class);
        Class<?> components=load("class_9323");
        legitItemComponents=mappedMethod(load("class_1792"),"method_57347",components);
        legitContains=mappedMethod(components,"method_57832",boolean.class,load("class_9331"));
        if (target == 12101) legitSwingTest=mappedMethod(load("class_1269"),"method_23666",boolean.class);
        else {
            legitSuccess=load("class_1269$class_9860"); Class<?> swing=load("class_1269$class_9861");
            legitSwingTest=mappedMethod(legitSuccess,"comp_2909",swing);
            legitClientSwing=mappedField(swing,"field_52427",swing,true).get(null);
        }
        legitPrepared=true;
    }
    private boolean legitContext(Object c) throws ReflectiveOperationException {
        return legitActive && !legitFailed && anchorExecutionMode == 1 && c != null && client() == c &&
            anchorPermitted(legitGeneration) && live(c,player.get(c),world.get(c)) && manager.get(c) != null && anchorWindow(c) != 0;
    }
    private void cancelAnchorLegit() throws ReflectiveOperationException {
        legitActive=false;
        Object c=client(); if (c == null) return;
        serviceAnchorLegitPulses(true);
        if (legitUseOwner != null) {
            Object[] owner=legitUseOwner;
            if (options.get(c) == owner[0] && anchorUseKey.get(owner[0]) == owner[1] && bound.get(owner[1]) == owner[2]) {
                if (legitFakeUse) anchorUseCount.setInt(owner[1],Math.max(0,anchorUseCount.getInt(owner[1])-1));
                anchorSetPressed.invoke(owner[1],anchorWindow(c) == (long) owner[3] && anchorPhysical(owner[2],(long) owner[3]));
            }
            legitUseOwner=null;
        }
        if (legitPulseMouse != null && triggerMouse.get(c) == legitPulseMouse) {
            if (anchorRightClicked == null) anchorRightClicked=mappedField(load("class_312"),"field_1788",boolean.class,false);
            long handle=anchorWindow(c);
            anchorRightClicked.setBoolean(legitPulseMouse,handle != 0 && (int)anchorGlfwMouse.invoke(null,handle,1) == 1);
        }
        legitPulseMouse=null; legitFakeUse=false;
    }
    private void anchorLegitFailure(Throwable error) {
        legitFailed=true;
        try { cancelAnchorLegit(); } catch (ReflectiveOperationException | RuntimeException cleanup) { error.addSuppressed(cleanup); }
    }
    private boolean legitFoodOrShield(Object stack) throws ReflectiveOperationException {
        if (stack == null || (boolean)anchorEmpty.invoke(stack)) return false;
        Object item=anchorItem.invoke(stack);
        return anchorShield.isInstance(item) || (item != null && (boolean)legitContains.invoke(legitItemComponents.invoke(item),anchorFood));
    }
    private boolean legitBody(Object c) throws ReflectiveOperationException {
        Object p=player.get(c),w=world.get(c),list=anchorPlayers.invoke(w);
        if (!(list instanceof Iterable<?> values)) return true;
        Object position=anchorPos.get(p);
        for (Object entity:values) if (entity != null && entity != p && (boolean)legitDead.invoke(entity)) {
            Object other=anchorPos.get(entity);
            double dx=anchorX.getDouble(other)-anchorX.getDouble(position),dy=anchorY.getDouble(other)-anchorY.getDouble(position),dz=anchorZ.getDouble(other)-anchorZ.getDouble(position);
            if (dx*dx+dy*dy+dz*dz <= 36) return true;
        }
        return false;
    }
    private int legitCharge(Object w,Object pos) throws ReflectiveOperationException {
        Object state=anchorStateAt.invoke(w,pos);
        if (state == null || !(boolean)anchorIsOf.invoke(state,anchorBlockValue)) return -1;
        Object charge=anchorProperty.invoke(state,anchorCharges);
        if (!(charge instanceof Integer value)) throw new IllegalStateException("Legit anchor charge type");
        return value;
    }
    private boolean legitHeld(Object p,Object wanted) throws ReflectiveOperationException {
        Object stack=anchorMainHand.invoke(p);
        return stack != null && anchorItem.invoke(stack) == wanted;
    }
    private boolean legitClock(int stage,int delay) {
        int value=stage == 0 ? legitSwitchClock : stage == 1 ? legitGlowClock : legitExplodeClock;
        if (value == delay) return true;
        if (stage == 0) ++legitSwitchClock; else if (stage == 1) ++legitGlowClock; else ++legitExplodeClock;
        return false;
    }
    private void legitSelect(Object c,Object p,int slot) throws ReflectiveOperationException {
        if (slot >= 0 && slot < 9 && legitContext(c) && player.get(c) == p)
            anchorSelected.setInt(anchorInventory.invoke(p),slot);
    }
    private void legitFindGlowstone(Object c,Object p) throws ReflectiveOperationException {
        Object inv=anchorInventory.invoke(p);
        for (int slot=0;slot<9;++slot) {
            Object stack=anchorStack.invoke(inv,slot);
            if (stack != null && anchorItem.invoke(stack) == anchorGlowstone) { legitSelect(c,p,slot); return; }
        }
    }
    private void anchorLegitPulse(Object c) throws ReflectiveOperationException {
        prepareTriggerSimulation();
        Object mouse=triggerMouse.get(c); long handle=anchorWindow(c);
        if (mouse == null || handle == 0) return;
        Object input=target >= 12111 ? triggerMouseInputConstructor.newInstance(1,0) : null;
        // Nominal 35 ms hold as in the reference. Release is serviced by the
        // game's tick/render thread, so no worker calls Minecraft or GLFW.
        legitPulseMouse=mouse;
        legitPulses.add(new Object[]{c,mouse,handle,System.nanoTime()+35_000_000L,input});
        legitFakeUse=true;
        if (target >= 12111) triggerMouseCallback.invoke(mouse,handle,input,1);
        else triggerMouseCallback.invoke(mouse,handle,1,1,0);
    }
    private void serviceAnchorLegitPulses(boolean force) throws ReflectiveOperationException {
        Object c=client(); if (c == null) return;
        ReflectiveOperationException error=null;
        for (int i=legitPulses.size()-1;i>=0;--i) {
            Object[] pulse=legitPulses.get(i);
            if (!force && System.nanoTime() < (long)pulse[3]) continue;
            legitPulses.remove(i);
            try {
                if (pulse[0] == c && triggerMouse.get(c) == pulse[1] && anchorWindow(c) == (long)pulse[2]) {
                    if (target >= 12111) triggerMouseCallback.invoke(pulse[1],(long)pulse[2],pulse[4],0);
                    else triggerMouseCallback.invoke(pulse[1],(long)pulse[2],1,0,0);
                }
            } catch (ReflectiveOperationException failure) { if (error == null) error=failure; else error.addSuppressed(failure); }
        }
        if (error != null) throw error;
    }
    private void legitInteract(Object c,Object p,Object w,Object m,Object hit) throws ReflectiveOperationException {
        if (!legitContext(c) || player.get(c) != p || world.get(c) != w || manager.get(c) != m) return;
        if (legitFlags[2]) anchorLegitPulse(c);
        Object result=anchorInteract.invoke(m,p,anchorHand,hit);
        if (result != null && (boolean)anchorAccepted.invoke(result)) {
            boolean swing=target == 12101 ? (boolean)legitSwingTest.invoke(result) :
                legitSuccess.isInstance(result) && legitSwingTest.invoke(result) == legitClientSwing;
            if (swing && legitContext(c)) anchorSwing.invoke(p,anchorHand);
        }
    }
    private void onAnchorLegitTick(Object c) {
        try {
            serviceAnchorLegitPulses(false);
            if (!legitContext(c)) return;
            Object p=player.get(c),w=world.get(c),m=manager.get(c); long handle=anchorWindow(c);
            boolean right=(int)anchorGlfwMouse.invoke(null,handle,1) == 1;
            if (!legitFlags[0] && right && (legitFoodOrShield(anchorMainHand.invoke(p)) || legitFoodOrShield(anchorOffHand.invoke(p)))) return;
            if (legitFlags[1] && legitBody(c)) return;
            int roll=anchorLegitPercent();
            if (!right) return;
            Object hit=anchorCrosshair.get(c);
            if (!anchorHitType.isInstance(hit)) return;
            Object pos=anchorGetPos.invoke(hit);
            if (pos == null || legitCharge(w,pos) < 0 || (legitFlags[3] && !legitOwned.contains(anchorKey(pos)))) return;
            Object opts=options.get(c),key=opts == null ? null : anchorUseKey.get(opts);
            if (key == null) return;
            Object input=bound.get(key);
            if (legitUseOwner == null || legitUseOwner[0] != opts || legitUseOwner[1] != key || legitUseOwner[2] != input)
                legitUseOwner=new Object[]{opts,key,input,handle};
            anchorSetPressed.invoke(key,false);
            if (legitCharge(w,pos) == 0) {
                roll=anchorLegitPercent();
                if (roll <= legitValues[0]) {
                    if (!legitHeld(p,anchorGlowstone)) {
                        if (!legitClock(0,legitValues[1])) return;
                        roll=anchorLegitPercent();
                        if (roll <= legitValues[2]) { legitSwitchClock=0; legitFindGlowstone(c,p); }
                    }
                    if (legitHeld(p,anchorGlowstone)) {
                        if (!legitClock(1,legitValues[3])) return;
                        roll=anchorLegitPercent();
                        if (roll <= legitValues[4]) { legitGlowClock=0; legitInteract(c,p,w,m,hit); }
                    }
                }
            }
            if (legitCharge(w,pos) > 0) {
                int slot=legitValues[7]-1; Object inv=anchorInventory.invoke(p);
                if (anchorSelected.getInt(inv) != slot) {
                    if (!legitClock(0,legitValues[1])) return;
                    if (roll <= legitValues[2]) { legitSwitchClock=0; legitSelect(c,p,slot); }
                }
                if (anchorSelected.getInt(inv) == slot) {
                    if (!legitClock(2,legitValues[5])) return;
                    roll=anchorLegitPercent();
                    if (roll <= legitValues[6]) {
                        legitExplodeClock=0;
                        if (!legitFlags[4]) { legitInteract(c,p,w,m,hit); legitOwned.remove(anchorKey(pos)); }
                    }
                }
            }
        } catch (ReflectiveOperationException | RuntimeException error) { anchorLegitFailure(error); }
    }
    private boolean onAnchorLegitUse(Object c) {
        try {
            if (!legitContext(c)) {
                // The reference's fake-click cancellation lives in the client
                // mixin, so it still consumes a queued fake use after disable.
                if (legitFakeUse && client() == c) { legitFakeUse=false; return true; }
                return false;
            }
            Object p=player.get(c),w=world.get(c),hit=anchorCrosshair.get(c);
            if (anchorHitType.isInstance(hit) && anchorGetType.invoke(hit) == anchorBlockType) {
                Object pos=anchorGetPos.invoke(hit);
                if (legitHeld(p,anchorItemValue)) {
                    Object state=anchorStateAt.invoke(w,pos),owned=pos;
                    if (!(boolean)legitReplaceable.invoke(state)) {
                        Object side=anchorSide.invoke(hit);
                        owned=anchorPosCtor.newInstance((int)anchorGetX.invoke(pos)+(int)anchorDx.invoke(side),
                            (int)anchorGetY.invoke(pos)+(int)anchorDy.invoke(side),(int)anchorGetZ.invoke(pos)+(int)anchorDz.invoke(side));
                    }
                    legitOwned.add(anchorKey(owned));
                }
                if (legitCharge(w,pos) > 0) legitOwned.remove(anchorKey(pos));
            }
            if (legitFakeUse) { legitFakeUse=false; return true; }
            return false;
        } catch (ReflectiveOperationException | RuntimeException error) { anchorLegitFailure(error); return false; }
    }
    public void tickAnchor(boolean active,long generation,int mode,boolean[] flags,int[] values) throws ReflectiveOperationException {
        Object c=client(); if (c == null) return;
        if (mode < 0 || mode > 1) throw new IllegalArgumentException("Anchor mode");
        if (mode != anchorExecutionMode) { cancelAnchor(); anchorExecutionMode=mode; anchorRequireRelease=false; }
        if (mode == 0) { tickAnchorNormal(active,generation,flags,values); return; }
        if (!active) { cancelAnchor(); return; }
        prepare(6);
        if (flags == null || flags.length != 5 || values == null || values.length != 8) throw new IllegalArgumentException("Anchor config shape");
        for (int i=0;i<8;++i) {
            int max=(i == 1 || i == 3 || i == 5) ? 20 : i == 7 ? 9 : 100;
            if (values[i] < (i == 7 ? 1 : 0) || values[i] > max) throw new IllegalArgumentException("Anchor config range");
        }
        prepareAnchorLegit();
        if (legitEvents == null) throw new IllegalStateException("Legit Anchor requires tick/item-use hooks");
        if (!legitActive) legitSwitchClock=legitGlowClock=legitExplodeClock=0;
        legitGeneration=generation; legitFlags=flags.clone(); legitValues=values.clone();
        legitActive=!legitFailed && anchorPermitted(generation);
        serviceAnchorLegitPulses(false);
    }
    // Existing callers/default configs retain the Normal controller.
    public void tickAnchor(boolean active,long generation,boolean[] flags,int[] values) throws ReflectiveOperationException {
        tickAnchor(active,generation,0,flags,values);
    }

    // Feature 7: Java port of Argon's AutoInventoryTotem tick flow.
    protected native boolean totemPermitted(long generation);
    private Class<?> totemScreenType;
    private Constructor<?> totemScreenCtor;
    private Method totemInventory, totemStack, totemOffhand, totemMainHand, totemEmpty, totemItem;
    private Method totemClick, totemSetScreen, totemClose;
    private Field totemHandler, totemScreenHandler, totemSync, totemSelected;
    private Object totemValue, totemSwap, totemPlayer, totemWorld, totemManager, totemInventoryOwner;
    private Object totemOpenedScreen;
    private int totemClock=-1, totemCloseClock=-1;
    private volatile boolean totemActive;
    private boolean totemFailed;
    private long totemGeneration;
    private int[] totemValues;
    private boolean totemAutoOpen;
    private final java.util.Random totemRandom=new java.util.Random();

    private void prepareTotem() throws ReflectiveOperationException {
        Class<?> inv=load("class_1661"), stack=load("class_1799"), itemType=load("class_1792");
        Class<?> handler=load("class_1703"), action=load("class_1713"), screenType=load("class_437");
        totemScreenType=load("class_490");
        Class<?> playerBase=load("class_1657");
        Class<?> fake;
        try { fake=Class.forName("net.onestep.hybrid.TotemScreen",false,WorldBridge.class.getClassLoader()); }
        catch (ClassNotFoundException missing) {
            Class<?> transformer=Class.forName("net.onestep.hybrid.AnchorTransformer",true,WorldBridge.class.getClassLoader());
            Method factory=transformer.getMethod("inventoryScreen",Class.class,Class.class,String.class,String.class);
            byte[] bytes=(byte[])factory.invoke(null,totemScreenType,playerBase,name("method_2383"),name("method_25402"));
            try { fake=java.lang.invoke.MethodHandles.lookup().defineClass(bytes); }
            catch (IllegalAccessException error) { throw new ReflectiveOperationException("Totem screen class",error); }
        }
        totemScreenCtor=constructor(fake,playerBase);
        totemInventory=mappedMethod(playerType,"method_31548",inv);
        totemStack=mappedMethod(inv,"method_5438",stack,int.class);
        totemOffhand=mappedMethod(playerType,"method_6079",stack);
        totemMainHand=mappedMethod(playerType,"method_6047",stack);
        totemEmpty=mappedMethod(stack,"method_7960",boolean.class);
        totemItem=mappedMethod(stack,"method_7909",itemType);
        totemValue=mappedField(load("class_1802"),"field_8288",itemType,true).get(null);
        totemSwap=mappedField(action,"field_7791",action,true).get(null);
        totemHandler=mappedField(playerType,"field_7512",handler,false);
        totemScreenHandler=mappedField(load("class_465"),"field_2797",handler,false);
        totemSync=mappedField(handler,"field_7763",int.class,false);
        totemSelected=mappedField(inv,"field_7545",int.class,false);
        totemClick=mappedMethod(load("class_636"),"method_2906",void.class,int.class,int.class,int.class,action,load("class_1657"));
        totemSetScreen=mappedMethod(clientType,"method_1507",void.class,screenType);
        totemClose=mappedMethod(screenType,"method_25419",void.class);
    }
    public void cancelTotem() {
        totemPlayer=totemWorld=totemManager=totemInventoryOwner=totemOpenedScreen=null;
        totemClock=totemCloseClock=-1; totemActive=false;
    }
    private boolean totemIs(Object stack) throws ReflectiveOperationException {
        return stack!=null && !(boolean)totemEmpty.invoke(stack) && totemItem.invoke(stack)==totemValue;
    }
    private int totemFind(Object inv,int mode) throws ReflectiveOperationException {
        java.util.ArrayList<Integer> slots=new java.util.ArrayList<>();
        for (int i=9;i<36;++i) if (totemIs(totemStack.invoke(inv,i))) {
            if (mode==0) return i;
            slots.add(i);
        }
        return slots.isEmpty() ? -1 : slots.get(totemRandom.nextInt(slots.size()));
    }
    private boolean totemHasOutside(Object inv) throws ReflectiveOperationException {
        return totemFind(inv,0)>=0;
    }
    private void totemClick(Object c,Object p,Object inv,Object h,int source,int destination) throws ReflectiveOperationException {
        if (source<9 || source>=36 || (destination!=40 && (destination<0 || destination>8))) return;
        int sync=totemSync.getInt(h);
        Object s=screen.get(c);
        if (player.get(c)!=p || world.get(c)!=totemWorld || manager.get(c)!=totemManager ||
            totemInventory.invoke(p)!=inv || totemHandler.get(p)!=h || !totemScreenType.isInstance(s) ||
            totemScreenHandler.get(s)!=h || !totemIs(totemStack.invoke(inv,source)) || !totemPermitted(totemGeneration)) return;
        totemClick.invoke(manager.get(c),sync,source,destination,totemSwap,p);
    }
    /** Argon AutoInventoryTotem settings: mode, delay ticks, hotbar, slot, auto switch, force, stay-open ticks. */
    public void tickTotem(boolean active,long generation,int[] values,boolean autoOpen) throws ReflectiveOperationException {
        if (!active || stopped) { cancelTotem(); return; }
        if (values==null || values.length!=7 || values[0]<0 || values[0]>1) throw new IllegalArgumentException("Auto Totem settings");
        prepare(7);
        if (client()==null) return;
        totemGeneration=generation; totemValues=values.clone(); totemAutoOpen=autoOpen;
        if (legitEvents==null) throw new IllegalStateException("Auto Totem requires tick hooks");
        totemActive=!totemFailed && totemPermitted(generation);
    }
    private void onTotemTick(Object c) {
        if (!totemActive || stopped) return;
        try { runTotemTick(c); }
        catch (ReflectiveOperationException | RuntimeException error) { totemFailed=true; cancelTotem(); }
    }
    private void runTotemTick(Object c) throws ReflectiveOperationException {
        if (c==null || client()!=c) return;
        int[] values=totemValues; boolean autoOpen=totemAutoOpen; long generation=totemGeneration;
        Object p=player.get(c),w=world.get(c),m=manager.get(c);
        if (p==null || w==null || m==null || !(boolean)alive.invoke(p) || !totemPermitted(generation)) { cancelTotem(); return; }
        Object inv=totemInventory.invoke(p),s=screen.get(c),h=totemHandler.get(p);
        if (inv==null || h==null) { cancelTotem(); return; }
        if (totemPlayer!=p || totemWorld!=w || totemManager!=m || totemInventoryOwner!=inv) {
            totemClock=totemCloseClock=-1; totemOpenedScreen=null;
            totemPlayer=p; totemWorld=w; totemManager=m; totemInventoryOwner=inv;
        }
        boolean screenIsOwned=s!=null && s==totemOpenedScreen;
        boolean invScreen=s!=null && totemScreenType.isInstance(s);
        boolean offhand=totemIs(totemOffhand.invoke(p));
        int desired=Math.max(1,Math.min(9,values[3]))-1;
        boolean hotbar=values[2]!=0;
        boolean desiredHotbar=totemIs(totemStack.invoke(inv,desired));
        boolean shouldOpen=(hotbar ? (!offhand || !desiredHotbar) : !offhand) && !screenIsOwned && totemHasOutside(inv);
        if (autoOpen && shouldOpen) {
            Object fake=totemScreenCtor.newInstance(p);
            if (!totemPermitted(generation)) return;
            totemSetScreen.invoke(c,fake);
            if (screen.get(c)==fake) {
                totemOpenedScreen=fake;
                // Continue this same tick through the local inventory screen.
                s=fake; screenIsOwned=true; invScreen=true;
            }
        }
        if (!invScreen) { totemClock=-1; totemCloseClock=-1; totemOpenedScreen=null; return; }
        if (totemScreenHandler.get(s)!=totemHandler.get(p)) return;
        int delay=Math.max(0,Math.min(20,values[1]));
        int stay=Math.max(0,Math.min(20,values[6]));
        if (totemClock==-1) totemClock=delay;
        if (totemCloseClock==-1) totemCloseClock=stay;
        if (totemClock>0) totemClock--;
        if (values[4]!=0) totemSelected.setInt(inv,desired);
        h=totemHandler.get(p);
        if (h==null) return;
        if (totemClock<=0) {
            offhand=totemIs(totemOffhand.invoke(p));
            if (!offhand) {
                int source=totemFind(inv,values[0]);
                if (source!=-1) { totemClick(c,p,inv,h,source,40); return; }
            }
            if (hotbar) {
                Object hand=totemMainHand.invoke(p);
                boolean empty=(boolean)totemEmpty.invoke(hand), already=totemIs(hand);
                if (empty || (values[5]!=0 && !already)) {
                    int source=totemFind(inv,values[0]);
                    if (source!=-1) {
                        int selected=totemSelected.getInt(inv);
                        totemClick(c,p,inv,h,source,selected); return;
                    }
                }
            }
            boolean close=screenIsOwned && (hotbar ? totemIs(totemStack.invoke(inv,desired)) && totemIs(totemOffhand.invoke(p)) : totemIs(totemOffhand.invoke(p)));
            if (close && autoOpen) {
                if (totemCloseClock!=0) { totemCloseClock--; return; }
                if (screen.get(c)==s && totemPermitted(generation)) totemClose.invoke(s);
                totemOpenedScreen=null; totemCloseClock=stay;
            }
        }
    }

    // Independent place/break controller, with bounded planning on the game thread.
    // References and design comparison: docs/CRYSTAL_AURA_REDESIGN.md.
    protected native long auraService(int operation, Object entity, int id, long generation, long epoch);
    private Class<?> auraLiving, auraPlayerClass, auraMobClass;
    private Method auraSpectator, auraHealth, auraOpaque, auraSide, auraCamera, auraOffhand;
    private Method auraArmor, auraAttribute, auraAbsorption, auraEffect, auraAmplifier, auraEquipped;
    private Method auraEnchantments, auraEnchantEntries, auraEnchantmentId, auraDifficulty, auraDifficultyId, auraAbilities;
    private Field auraYaw, auraPitch, auraTicks, auraInvulnerable;
    private Object auraObsidianItem, auraOffHand, auraToughness, auraResistance, auraWeakness, auraStrength;
    private Object[] auraArmorSlots;
    private volatile boolean auraActive;
    private volatile long auraConfigTime;
    private long auraGeneration, auraEpoch, auraPlaceReady, auraBreakReady, auraDeadline, auraSwitchReady, auraManualPause;
    private int[] auraConfig, auraBase, auraAimBase;
    private boolean auraFailed;
    private Object auraPlayer, auraWorld, auraManager, auraInv, auraTarget, auraCrystal, auraAimVictim;
    private int auraPhase, auraTargetId=-1, auraOldSlot=-1, auraOwnedSlot=-1;
    private int auraAimAction=-1, auraAimTick=-1, auraStatus, auraLastTick=-1;
    private int auraPendingAction=-1, auraPendingSlot=-1;
    private Object auraPendingVictim;
    private Object auraPendingHand;
    private int[] auraPendingBase;
    private double[] auraPendingAim;
    private List<Object[]> auraScene=List.of();
    private final Map<Object,double[]> auraDefenses=new java.util.IdentityHashMap<>();
    private final Map<Object,Integer> auraBreakAttempts=new java.util.IdentityHashMap<>();
    private final Map<String,Long> auraRejectedSites=new HashMap<>();
    private final List<int[]> auraSites=new java.util.ArrayList<>();
    private int auraSitesTick=-1, auraSiteEvaluations;
    private double[] auraSitesOrigin;
    private int auraDifficultyValue=2;
    private boolean auraSawDamageRejection, auraSawLowDamage, auraSawRetryLimit;
    private boolean auraWeaponPrepared;
    private Class<?> auraSwordClass, auraAxeClass;
    private Method auraItemEntry, auraSwordTagTest;
    private Object auraSwordTag;

    private void prepareAura() throws ReflectiveOperationException {
        prepare(3);
        Class<?> entity=load("class_1297"), entry=load("class_6880"), stack=load("class_1799");
        auraLiving=load("class_1309"); auraPlayerClass=load("class_1657"); auraMobClass=load("class_1308");
        auraSpectator=mappedMethod(entity,"method_7325",boolean.class);
        auraHealth=mappedMethod(auraLiving,"method_6032",float.class);
        auraOpaque=mappedMethod(load("class_2680"),"method_26225",boolean.class);
        auraSide=mappedMethod(crystalHitType,"method_17780",load("class_2350"));
        auraCamera=mappedMethod(clientType,"method_1560",entity);
        auraOffhand=mappedMethod(auraLiving,"method_6079",stack);
        auraYaw=mappedField(entity,"field_6031",float.class,false);
        auraPitch=mappedField(entity,"field_5965",float.class,false);
        auraTicks=mappedField(entity,"field_6012",int.class,false);
        auraObsidianItem=mappedField(load("class_1802"),"field_8281",load("class_1792"),true).get(null);
        auraOffHand=mappedField(load("class_1268"),"field_5810",load("class_1268"),true).get(null);
        auraArmor=mappedMethod(auraLiving,"method_6096",int.class);
        auraAttribute=mappedMethod(auraLiving,"method_45325",double.class,entry);
        auraAbsorption=mappedMethod(auraLiving,"method_6067",float.class);
        auraEffect=mappedMethod(auraLiving,"method_6112",load("class_1293"),entry);
        auraAmplifier=mappedMethod(load("class_1293"),"method_5578",int.class);
        auraToughness=mappedField(load("class_5134"),"field_23725",entry,true).get(null);
        Class<?> effects=load("class_1294");
        auraResistance=mappedField(effects,"field_5907",entry,true).get(null);
        auraWeakness=mappedField(effects,"field_5911",entry,true).get(null);
        auraStrength=mappedField(effects,"field_5910",entry,true).get(null);
        Class<?> equipment=load("class_1304");
        auraEquipped=mappedMethod(auraLiving,"method_6118",stack,equipment);
        auraArmorSlots=new Object[4]; String[] slots={"field_6166","field_6172","field_6174","field_6169"};
        for (int i=0;i<4;i++) auraArmorSlots[i]=mappedField(equipment,slots[i],equipment,true).get(null);
        auraEnchantments=mappedMethod(stack,"method_58657",load("class_9304"));
        auraEnchantEntries=mappedMethod(load("class_9304"),"method_57539",java.util.Set.class);
        auraEnchantmentId=mappedMethod(entry,"method_55840",String.class);
        auraDifficulty=mappedMethod(load("class_638"),"method_8407",load("class_1267"));
        auraDifficultyId=mappedMethod(load("class_1267"),"method_5461",int.class);
        auraAbilities=mappedMethod(auraPlayerClass,"method_31549",load("class_1656"));
        auraInvulnerable=mappedField(load("class_1656"),"field_7480",boolean.class,false);
        if (auraObsidianItem==null || auraOffHand==null || auraToughness==null || auraResistance==null || auraWeakness==null || auraStrength==null)
            throw new IllegalStateException("Crystal Aura registry bindings");
        prepareTickLifecycle();
    }
    private boolean auraFlag(int mask) { return (auraConfig[10]&mask)!=0; }
    private double auraSetting(int index) { return auraConfig[index]/1000.; }
    private static double auraDistance(double[] a,double[] b) {
        double x=a[0]-b[0],y=a[1]-b[1],z=a[2]-b[2]; return x*x+y*y+z*z;
    }
    private double[] auraPosition(Object entity) throws ReflectiveOperationException {
        Object pos=crystalPosition.get(entity);
        double[] p={crystalX.getDouble(pos),crystalY.getDouble(pos),crystalZ.getDouble(pos)};
        for(double v:p) if(!Double.isFinite(v)) throw new IllegalStateException("Aura nonfinite position");
        return p;
    }
    private double[] auraBox(Object entity) throws ReflectiveOperationException {
        Object box=crystalBounds.invoke(entity);
        double[] b={crystalMinX.getDouble(box),crystalMinY.getDouble(box),crystalMinZ.getDouble(box),
            crystalMaxX.getDouble(box),crystalMaxY.getDouble(box),crystalMaxZ.getDouble(box)};
        for(double v:b) if(!Double.isFinite(v)) throw new IllegalStateException("Aura nonfinite bounds");
        for(int i=0;i<3;i++) if(b[i]>=b[i+3]) throw new IllegalStateException("Aura empty bounds");
        return b;
    }
    private static double[] auraCenter(double[] b) { return new double[]{(b[0]+b[3])*.5,(b[1]+b[4])*.5,(b[2]+b[5])*.5}; }
    private static double auraBoxDistance(double[] p,double[] b) {
        double sum=0; for(int i=0;i<3;i++) { double d=p[i]-Math.max(b[i],Math.min(p[i],b[i+3])); sum+=d*d; } return sum;
    }
    private Object auraRay(double[] from,double[] to) throws ReflectiveOperationException {
        return crystalRaycast.invoke(auraWorld,crystalRayCtor.newInstance(
            crystalVecCtor.newInstance(from[0],from[1],from[2]),crystalVecCtor.newInstance(to[0],to[1],to[2]),
            crystalCollider,crystalNoFluid,auraPlayer));
    }
    private boolean auraVisible(double[] from,double[] to) throws ReflectiveOperationException {
        Object hit=auraRay(from,to); return hit!=null && crystalGetType.invoke(hit)!=crystalBlockType;
    }
    private Object auraState(int x,int y,int z) throws ReflectiveOperationException {
        return crystalStateAt.invoke(auraWorld,crystalPosCtor.newInstance(x,y,z));
    }
    private boolean auraBaseReady(int[] base) throws ReflectiveOperationException {
        Object state=auraState(base[0],base[1],base[2]);
        return state!=null && ((boolean)crystalIsOf.invoke(state,crystalObsidian) || (boolean)crystalIsOf.invoke(state,crystalBedrock));
    }
    private boolean auraAir(int x,int y,int z) throws ReflectiveOperationException {
        Object state=auraState(x,y,z); return state!=null && (boolean)crystalAir.invoke(state);
    }
    private static String auraSiteKey(int[] b) { return b[0]+":"+b[1]+":"+b[2]; }
    private static boolean auraOverlap(double[] box,int[] b,boolean support) {
        double bottom=b[1]+(support?0:1),top=bottom+(support?1:2);
        return box[0]<b[0]+1 && box[3]>b[0] && box[1]<top && box[4]>bottom && box[2]<b[2]+1 && box[5]>b[2];
    }
    private boolean auraEmpty(int[] b,boolean support) throws ReflectiveOperationException {
        double bottom=b[1]+(support?0:1),top=bottom+(support?1:2);
        Object volume=crystalBoxCtor.newInstance((double)b[0],bottom,(double)b[2],b[0]+1.,top,b[2]+1.);
        List<?> entities=(List<?>)crystalEntities.invoke(auraWorld,null,volume);
        if(entities==null || entities.size()>4096 || auraOverlap(auraBox(auraPlayer),b,support)) return false;
        for(Object e:entities) if((boolean)crystalAlive.invoke(e)) return false;
        return true;
    }
    private boolean auraClear(int[] b,boolean existing) throws ReflectiveOperationException {
        // Modern EndCrystalItem requires air in the first block, with a 1x2x1
        // entity query. A second air block is only a legacy 1.12 restriction.
        return auraAir(b[0],b[1]+1,b[2]) && (existing || (auraAir(b[0],b[1],b[2]) && auraEmpty(b,true))) && auraEmpty(b,false);
    }
    private Object auraCrystalAt(int[] b) throws ReflectiveOperationException {
        double[] spawn={b[0]+.5,b[1]+1.,b[2]+.5};
        for(Object[] row:auraScene) if(crystalType.isInstance(row[0]) && (boolean)crystalAlive.invoke(row[0]) && auraDistance((double[])row[2],spawn)<.25) return row[0];
        return null;
    }
    private boolean auraHitsTop(Object hit,int[] b) throws ReflectiveOperationException {
        if(!crystalHitType.isInstance(hit) || crystalGetType.invoke(hit)!=crystalBlockType || auraSide.invoke(hit)!=crystalUp) return false;
        Object p=crystalGetBlock.invoke(hit);
        return p!=null && (int)crystalGetX.invoke(p)==b[0] && (int)crystalGetY.invoke(p)==b[1] && (int)crystalGetZ.invoke(p)==b[2];
    }
    private static double[] auraTop(int[] b) { return new double[]{b[0]+.5,b[1]+.999,b[2]+.5}; }
    private boolean auraReachTop(double[] eye,int[] b,double range) throws ReflectiveOperationException {
        return auraDistance(eye,auraTop(b))<=range*range && auraHitsTop(auraRay(eye,auraTop(b)),b);
    }
    private boolean auraEligible(Object e) throws ReflectiveOperationException {
        if(e==null || e==auraPlayer || !auraLiving.isInstance(e) || !(boolean)crystalAlive.invoke(e) || (boolean)auraSpectator.invoke(e)) return false;
        boolean player=auraPlayerClass.isInstance(e);
        if(!(player?auraConfig[0]!=2:auraMobClass.isInstance(e) && auraConfig[0]!=0)) return false;
        float health=(float)auraHealth.invoke(e); int id=(int)crystalId.invoke(e);
        return Float.isFinite(health) && health>0 && id>=0 && crystalById.invoke(auraWorld,id)==e &&
            (!player || auraService(0,e,id,auraGeneration,auraEpoch)!=0);
    }
    private boolean auraScan(double[] eye) throws ReflectiveOperationException {
        double range=Math.max(18,auraSetting(5));
        Object volume=crystalBoxCtor.newInstance(eye[0]-range,eye[1]-range,eye[2]-range,eye[0]+range,eye[1]+range,eye[2]+range);
        List<?> entities=(List<?>)crystalEntities.invoke(auraWorld,auraPlayer,volume);
        if(entities==null || entities.size()>4096) return false;
        List<Object[]> scene=new java.util.ArrayList<>();
        for(Object e:entities) if((boolean)crystalAlive.invoke(e)) scene.add(new Object[]{e,auraBox(e),auraPosition(e)});
        auraScene=scene; return true;
    }
    private Object auraChoose(double[] eye) throws ReflectiveOperationException {
        Object best=null; double nearest=Double.POSITIVE_INFINITY; int bestId=Integer.MAX_VALUE;
        double range=auraSetting(5);
        for(Object[] row:auraScene) {
            Object e=row[0]; if(!auraEligible(e)) continue;
            double distance=auraBoxDistance(eye,(double[])row[1]); if(distance>range*range) continue;
            if(e==auraTarget) return e;
            int id=(int)crystalId.invoke(e);
            if(distance<nearest || (distance==nearest && id<bestId)) {best=e;nearest=distance;bestId=id;}
        }
        return best;
    }
    private double[] auraDefense(Object e) throws ReflectiveOperationException {
        double[] cached=auraDefenses.get(e); if(cached!=null) return cached;
        double armor=(int)auraArmor.invoke(e),tough=(double)auraAttribute.invoke(e,auraToughness);
        float health=(float)auraHealth.invoke(e),absorption=(float)auraAbsorption.invoke(e);
        Object resistance=auraEffect.invoke(e,auraResistance);
        int level=resistance==null?-1:(int)auraAmplifier.invoke(resistance),protection=0;
        for(Object equipment:auraArmorSlots) {
            Object stack=auraEquipped.invoke(e,equipment); if(stack==null || (boolean)crystalEmpty.invoke(stack)) continue;
            Object component=auraEnchantments.invoke(stack);
            java.util.Set<?> entries=(java.util.Set<?>)auraEnchantEntries.invoke(component);
            if(entries.size()>256) throw new IllegalStateException("Aura enchantment limit");
            for(Object value:entries) {
                Map.Entry<?,?> entry=(Map.Entry<?,?>)value;
                String id=(String)auraEnchantmentId.invoke(entry.getKey());
                int amount=Math.max(0,Math.min(255,((Number)entry.getValue()).intValue()));
                if("minecraft:protection".equals(id)) protection+=amount;
                if("minecraft:blast_protection".equals(id)) protection+=2*amount;
            }
        }
        boolean invulnerable=false;
        if(auraPlayerClass.isInstance(e)) {Object abilities=auraAbilities.invoke(e); invulnerable=abilities!=null && auraInvulnerable.getBoolean(abilities);}
        if(!Double.isFinite(tough) || !Float.isFinite(health) || !Float.isFinite(absorption)) throw new IllegalStateException("Aura defense values");
        double[] result={armor,Math.max(0,tough),level,Math.min(20,protection),health+absorption,invulnerable?1:0};
        auraDefenses.put(e,result); return result;
    }
    private static boolean auraSegmentBox(double[] from,double[] to,int[] base) {
        double enter=0,leave=1;
        for(int i=0;i<3;i++) {
            double delta=to[i]-from[i];
            if(Math.abs(delta)<1e-12) {if(from[i]<=base[i] || from[i]>=base[i]+1.) return false;continue;}
            double a=(base[i]-from[i])/delta,b=(base[i]+1.-from[i])/delta;
            enter=Math.max(enter,Math.min(a,b)); leave=Math.min(leave,Math.max(a,b));
            if(enter>=leave) return false;
        }
        return leave>1e-6 && enter<1-1e-6;
    }
    private double auraExposure(Object e,double[] explosion,int[] virtualSupport) throws ReflectiveOperationException {
        double[] box=auraBox(e);
        double sx=1/(2*(box[3]-box[0])+1),sy=1/(2*(box[4]-box[1])+1),sz=1/(2*(box[5]-box[2])+1);
        double ox=(1-Math.floor(1/sx)*sx)*.5,oz=(1-Math.floor(1/sz)*sz)*.5;
        int clear=0,total=0;
        // Vanilla sampling, bounded for nonstandard oversized entities.
        int nx=Math.min(16,(int)Math.floor(1/sx)+1),ny=Math.min(16,(int)Math.floor(1/sy)+1),nz=Math.min(16,(int)Math.floor(1/sz)+1);
        if(nx*ny*nz>256) {nx=3;ny=5;nz=3;sx=.5;sy=.25;sz=.5;ox=oz=0;}
        for(int x=0;x<nx;x++) for(int y=0;y<ny;y++) for(int z=0;z<nz;z++) {
            double[] sample={box[0]+(box[3]-box[0])*Math.min(1,x*sx)+ox,box[1]+(box[4]-box[1])*Math.min(1,y*sy),box[2]+(box[5]-box[2])*Math.min(1,z*sz)+oz};
            ++total;
            if((virtualSupport==null || !auraSegmentBox(sample,explosion,virtualSupport)) && auraVisible(sample,explosion)) ++clear;
        }
        return total==0?0:(double)clear/total;
    }
    private double auraDamage(Object e,double[] explosion,int[] virtualSupport) throws ReflectiveOperationException {
        double[] defense=auraDefense(e); if(defense[5]!=0) return 0;
        double distance=Math.sqrt(auraDistance(auraPosition(e),explosion)); if(distance>12) return 0;
        double impact=(1-distance/12)*auraExposure(e,explosion,virtualSupport);
        if(impact<=0) return 0;
        double damage=(impact*impact+impact)*42+1;
        if(auraPlayerClass.isInstance(e)) {
            if(auraDifficultyValue==0) return 0;
            if(auraDifficultyValue==1) damage=Math.min(damage,damage*.5+1);
            if(auraDifficultyValue==3) damage*=1.5;
        }
        double armor=Math.max(defense[0]*.2,Math.min(20,defense[0]-damage/(2+defense[1]/4)));
        damage*=1-armor/25;
        if(defense[2]>=0) damage*=Math.max(0,1-.2*(defense[2]+1));
        damage*=1-defense[3]/25;
        return Math.max(0,damage);
    }
    private double auraScore(double[] explosion,int[] virtualSupport) throws ReflectiveOperationException {
        double target=auraDamage(auraTarget,explosion,virtualSupport);
        if(target<auraSetting(8)) {auraSawLowDamage=true;return -1;}
        double self=auraDamage(auraPlayer,explosion,virtualSupport);
        if(auraFlag(32)) {
            if(self>auraSetting(9) || self>=auraDefense(auraPlayer)[4]) {auraSawDamageRejection=true;return -1;}
            for(Object[] row:auraScene) if(auraPlayerClass.isInstance(row[0]) && auraService(5,row[0],0,auraGeneration,auraEpoch)!=0) {
                double friend=auraDamage(row[0],explosion,virtualSupport);
                if(friend>auraSetting(9) || friend>=auraDefense(row[0])[4]) {auraSawDamageRejection=true;return -1;}
            }
        }
        return Math.max(.001,target-self*.1);
    }
    private int auraItemSlot(Object wanted) throws ReflectiveOperationException {
        int selected=crystalSelected.getInt(auraInv);
        if(selected>=0 && selected<9 && auraHolds(crystalStack.invoke(auraInv,selected),wanted)) return selected;
        for(int i=0;i<9;i++) if(auraHolds(crystalStack.invoke(auraInv,i),wanted)) return i;
        return -1;
    }
    private boolean auraHolds(Object stack,Object item) throws ReflectiveOperationException {
        return stack!=null && !(boolean)crystalEmpty.invoke(stack) && crystalItem.invoke(stack)==item;
    }
    private void prepareAuraWeapons() throws ReflectiveOperationException {
        if(auraWeaponPrepared) return;
        auraAxeClass=load("class_1743");
        if(target<12108) auraSwordClass=load("class_1829");
        else {
            Class<?> reference=load("class_6880$class_6883"),tag=load("class_6862");
            auraItemEntry=mappedMethod(load("class_1792"),"method_40131",reference);
            auraSwordTagTest=mappedMethod(reference,"method_40220",boolean.class,tag);
            auraSwordTag=mappedField(load("class_3489"),"field_42611",tag,true).get(null);
        }
        auraWeaponPrepared=true;
    }
    private boolean auraWeaknessWeapon(Object stack) throws ReflectiveOperationException {
        if(stack==null || (boolean)crystalEmpty.invoke(stack)) return false;
        Object item=crystalItem.invoke(stack);
        if(auraAxeClass.isInstance(item)) return true;
        if(auraSwordClass!=null) return auraSwordClass.isInstance(item);
        Object entry=auraItemEntry.invoke(item);
        return entry!=null && (boolean)auraSwordTagTest.invoke(entry,auraSwordTag);
    }
    private int auraWeaknessSlot() throws ReflectiveOperationException {
        Object weakness=auraEffect.invoke(auraPlayer,auraWeakness),strength=auraEffect.invoke(auraPlayer,auraStrength);
        if(weakness==null || (strength!=null && (int)auraAmplifier.invoke(strength)>(int)auraAmplifier.invoke(weakness))) return crystalSelected.getInt(auraInv);
        prepareAuraWeapons();
        int selected=crystalSelected.getInt(auraInv);
        if(auraWeaknessWeapon(crystalStack.invoke(auraInv,selected))) return selected;
        if(auraFlag(4)) for(int i=0;i<9;i++) if(auraWeaknessWeapon(crystalStack.invoke(auraInv,i))) return i;
        return -1;
    }
    private void auraClearAim() {auraAimAction=-1;auraAimTick=-1;auraAimVictim=null;auraAimBase=null;}
    private void auraCycle() {
        auraPhase=0;auraTarget=null;auraTargetId=-1;auraBase=null;auraCrystal=null;
        auraClearAim();auraPendingAction=-1;auraPendingVictim=null;auraPendingBase=null;auraPendingAim=null;auraPendingHand=null;
        auraSites.clear();auraSitesTick=-1;auraSitesOrigin=null;
    }
    public synchronized void cancelAura() throws ReflectiveOperationException {
        if(prepared[8] && client()==null) return;
        auraActive=false;
        Object p=auraPlayer,w=auraWorld,m=auraManager,inv=auraInv;
        int old=auraOldSlot,owned=auraOwnedSlot; boolean restore=auraConfig!=null && auraFlag(8);
        auraPlayer=auraWorld=auraManager=auraInv=null;auraOldSlot=auraOwnedSlot=-1;auraLastTick=-1;
        auraStatus=0;auraCycle();auraScene=List.of();auraDefenses.clear();auraBreakAttempts.clear();auraRejectedSites.clear();
        auraPlaceReady=auraBreakReady=auraDeadline=auraSwitchReady=0;
        if(prepared[8]) auraService(2,null,-1,0,0);
        if(!prepared[8] || !restore || old<0 || old>8 || inv==null) return;
        Object c=client();
        if(c!=null && player.get(c)==p && world.get(c)==w && manager.get(c)==m && crystalInventory.invoke(p)==inv && crystalSelected.getInt(inv)==owned) {
            crystalSelected.setInt(inv,old);crystalSync.invoke(m);
        }
    }
    public synchronized int[] tickAura(boolean active,long generation,int[] config) throws ReflectiveOperationException {
        if(!active || auraFailed || stopped) {cancelAura();return new int[]{-1,0,auraFailed?4:0};}
        Object c=client();if(c==null) return new int[]{auraTargetId,auraPhase,auraStatus};
        prepare(8);
        if(config==null || config.length!=11 || config[0]<0 || config[0]>2 || config[1]<50 || config[1]>300 ||
            config[2]<500 || config[2]>3000 || config[3]<0 || config[3]>1000 || config[4]<0 || config[4]>1000 ||
            config[5]<1000 || config[5]>12000 || config[6]<1000 || config[6]>6000 || config[7]<1000 || config[7]>6000 ||
            config[8]<0 || config[8]>36000 || config[9]<0 || config[9]>36000 || config[10]<0 || config[10]>63)
            throw new IllegalArgumentException("Aura config");
        if(generation!=auraGeneration || !java.util.Arrays.equals(config,auraConfig)) {
            cancelAura();auraConfig=config.clone();auraGeneration=generation;
        }
        auraConfigTime=System.nanoTime();auraActive=true;
        if(!clientTickHooked) onAuraTick(c);
        return new int[]{auraTargetId,auraPhase,auraStatus};
    }
    private boolean auraSession(Object c) throws ReflectiveOperationException {
        return c!=null && auraActive && !auraFailed && System.nanoTime()-auraConfigTime<=500_000_000L &&
            live(c,auraPlayer,auraWorld) && player.get(c)==auraPlayer && world.get(c)==auraWorld && manager.get(c)==auraManager &&
            crystalInventory.invoke(auraPlayer)==auraInv && auraService(4,null,-1,auraGeneration,auraEpoch)!=0;
    }
    private boolean auraPrepare(int slot,double[] eye,double[] to,int action,Object victim,int[] base,long now) throws ReflectiveOperationException {
        int selected=crystalSelected.getInt(auraInv);
        if(slot<0 || slot>8 || selected<0 || selected>8) return false;
        if(selected!=slot && !auraFlag(4)) {auraStatus=8;return false;}
        if(auraOldSlot<0) auraOldSlot=selected;
        boolean changed=false;
        if(selected!=slot) {
            auraOwnedSlot=slot;crystalSelected.setInt(auraInv,slot);crystalSync.invoke(auraManager);
            auraSwitchReady=now+auraConfig[1]*1_000_000L;changed=true;
        }
        auraOwnedSlot=slot;
        int tick=auraTicks.getInt(auraPlayer);
        if(auraAimAction!=action || auraAimVictim!=victim || !java.util.Arrays.equals(auraAimBase,base)) {
            auraAimAction=action;auraAimVictim=victim;auraAimBase=base==null?null:base.clone();auraAimTick=tick;changed=true;
        }
        double dx=to[0]-eye[0],dy=to[1]-eye[1],dz=to[2]-eye[2];
        float yaw=(float)(Math.toDegrees(Math.atan2(dz,dx))-90),pitch=(float)-Math.toDegrees(Math.atan2(dy,Math.hypot(dx,dz)));
        float oldYaw=auraYaw.getFloat(auraPlayer),oldPitch=auraPitch.getFloat(auraPlayer);
        if(!Float.isFinite(oldYaw) || !Float.isFinite(oldPitch)) return false;
        if(Math.abs(Math.IEEEremainder(oldYaw-yaw,360.))>.25 || Math.abs(oldPitch-pitch)>.25) {
            auraYaw.setFloat(auraPlayer,yaw);auraPitch.setFloat(auraPlayer,Math.max(-90,Math.min(90,pitch)));
            auraAimTick=tick;changed=true;
        }
        if(changed || now<auraSwitchReady || tick<=auraAimTick) {auraStatus=5;return false;}
        return true;
    }
    private void auraCacheSites(double[] eye,double blockRange) throws ReflectiveOperationException {
        int tick=auraTicks.getInt(auraPlayer);
        if(auraSitesTick>=0 && tick-auraSitesTick<4 && auraSitesOrigin!=null && auraDistance(eye,auraSitesOrigin)<.25) return;
        auraSites.clear();auraSitesTick=tick;auraSitesOrigin=eye.clone();
        double[] target=auraPosition(auraTarget);int range=(int)Math.ceil(blockRange);
        int cx=(int)Math.floor(eye[0]),cy=(int)Math.floor(eye[1]),cz=(int)Math.floor(eye[2]);
        if(Math.abs(eye[0])>30000000 || Math.abs(eye[1])>30000000 || Math.abs(eye[2])>30000000) return;
        for(int x=cx-range;x<=cx+range;x++) for(int y=cy-range;y<=cy+range;y++) for(int z=cz-range;z<=cz+range;z++) {
            int[] b={x,y,z};if(auraDistance(new double[]{x+.5,y+1.,z+.5},target)>36) continue;
            if(auraDistance(eye,auraTop(b))>blockRange*blockRange+1) continue;
            boolean existing=auraBaseReady(b);
            if(!existing && (!auraFlag(16) || !auraAir(x,y,z))) continue;
            auraSites.add(new int[]{x,y,z,existing?0:1});
        }
        auraSites.sort(java.util.Comparator.<int[]>comparingDouble(b -> b[3]*4+auraDistance(new double[]{b[0]+.5,b[1]+1.,b[2]+.5},target)));
    }
    private double[] auraCrystalAim(double[] eye,double[] box,double range) throws ReflectiveOperationException {
        double[] near=new double[3];
        for(int i=0;i<3;i++) near[i]=Math.max(box[i]+.01,Math.min(eye[i],box[i+3]-.01));
        double[] center=auraCenter(box);
        double[][] points={near,center,new double[]{center[0],box[1]+.05,center[2]},
            new double[]{center[0],box[4]-.05,center[2]}};
        for(double[] point:points) if(auraDistance(eye,point)<=range*range && auraVisible(eye,point)) return point;
        return null;
    }
    private Object auraBestCrystal(double[] eye,double range) throws ReflectiveOperationException {
        Object best=null;double score=-1;int evaluated=0;
        if(auraAimAction==3 && auraAimVictim!=null) for(Object[] row:auraScene) if(row[0]==auraAimVictim) {
            if(auraBreakAttempts.getOrDefault(row[0],0)<3 && auraBoxDistance(eye,(double[])row[1])<=range*range &&
                auraCrystalAim(eye,(double[])row[1],range)!=null) {
                ++evaluated;if(auraScore((double[])row[2],null)>=0) return row[0];
            }
            break;
        }
        for(Object[] row:auraScene) {
            if(evaluated>=32) break;
            Object e=row[0];if(!crystalType.isInstance(e) || auraBoxDistance(eye,(double[])row[1])>range*range) continue;
            if(auraBreakAttempts.getOrDefault(e,0)>=3) {auraSawRetryLimit=true;continue;}
            if(auraCrystalAim(eye,(double[])row[1],range)==null) continue;
            double value=auraScore((double[])row[2],null);
            if(value>score) {score=value;best=e;}
            if(++evaluated>=32) break;
        }
        return best;
    }
    private double auraSiteScore(int[] b,double[] eye,double blockRange,double entityRange,long now) throws ReflectiveOperationException {
        if(auraRejectedSites.getOrDefault(auraSiteKey(b),0L)>now) return -1;
        boolean existing=auraBaseReady(b);
        if(!existing && (!auraFlag(16) || auraItemSlot(auraObsidianItem)<0)) return -1;
        int[] clicked={b[0],b[1]-(existing?0:1),b[2]};
        if(!existing) {Object state=auraState(clicked[0],clicked[1],clicked[2]);if(state==null || !(boolean)auraOpaque.invoke(state)) return -1;}
        if(!auraClear(b,existing) || !auraReachTop(eye,clicked,blockRange)) return -1;
        double[] spawn={b[0]+.5,b[1]+1.,b[2]+.5};
        double[] crystalBox={b[0]-.5,b[1]+1.,b[2]-.5,b[0]+1.5,b[1]+3.,b[2]+1.5};
        if(auraFlag(2) && auraBoxDistance(eye,crystalBox)>entityRange*entityRange) return -1;
        if(auraSiteEvaluations>=16) return -1;
        ++auraSiteEvaluations;
        return auraScore(spawn,existing?null:b);
    }
    private int[] auraPlan(double[] eye,double blockRange,double entityRange,long now) throws ReflectiveOperationException {
        int[] best=null;double score=-1;auraSiteEvaluations=0;
        // A moving target must not continually restart an already valid aim.
        if((auraAimAction==1 || auraAimAction==2) && auraAimBase!=null &&
            auraSiteScore(auraAimBase,eye,blockRange,entityRange,now)>=0) return auraAimBase.clone();
        auraCacheSites(eye,blockRange);
        for(int[] candidate:auraSites) {
            if(auraSiteEvaluations>=16) break;
            int[] b={candidate[0],candidate[1],candidate[2]};
            double value=auraSiteScore(b,eye,blockRange,entityRange,now);
            if(value<0) continue;
            boolean existing=auraBaseReady(b);
            // Prefer an existing base at equal damage, instead of needless support.
            if(value>=0 && (!existing)) value-=.01;
            if(value>score) {score=value;best=b;}
        }
        return best;
    }
    // Synchronous callback under the native shared combat lock; all final game
    // validation and vanilla actions remain here, with no queued click replay.
    private boolean executeAuraAction() throws ReflectiveOperationException {
        int action=auraPendingAction,slot=auraPendingSlot;Object victim=auraPendingVictim,hand=auraPendingHand;
        int[] base=auraPendingBase;double[] aim=auraPendingAim;
        auraPendingAction=-1;
        Object c=client();
        if(action<1 || action>3 || !auraSession(c) || !auraEligible(auraTarget) || auraCamera.invoke(c)!=auraPlayer ||
            crystalSelected.getInt(auraInv)!=slot || (boolean)crystalUsing.invoke(auraPlayer)) return false;
        auraDefenses.clear();
        double[] eye=auraPosition(auraPlayer);eye[1]=(double)crystalEyeY.invoke(auraPlayer);
        double er=Math.min(auraSetting(7),(double)crystalEntityRange.invoke(auraPlayer));
        double br=Math.min(auraSetting(6),(double)crystalBlockRange.invoke(auraPlayer));
        if(!Double.isFinite(eye[1]) || !Double.isFinite(er) || !Double.isFinite(br) || er<=0 || br<=0) return false;
        if(action==3) {
            if(!auraFlag(2) || victim==null || !crystalType.isInstance(victim) || !(boolean)crystalAlive.invoke(victim) ||
                crystalById.invoke(auraWorld,(int)crystalId.invoke(victim))!=victim || auraBoxDistance(eye,auraBox(victim))>er*er ||
                !auraVisible(eye,aim) || auraScore(auraPosition(victim),null)<0 || auraWeaknessSlot()!=slot) return false;
            crystalAttack.invoke(auraManager,auraPlayer,victim);crystalSwing.invoke(auraPlayer,crystalHand);
            return true;
        }
        if(!auraFlag(1) || base==null || hand==null) return false;
        boolean support=action==1;
        int[] clicked={base[0],base[1]-(support?1:0),base[2]};
        if(!(support?auraAir(base[0],base[1],base[2]):auraBaseReady(base)) || !auraClear(base,!support) || !auraReachTop(eye,clicked,br)) return false;
        if(support) {Object state=auraState(clicked[0],clicked[1],clicked[2]);if(state==null || !(boolean)auraOpaque.invoke(state)) return false;}
        Object stack=hand==auraOffHand?auraOffhand.invoke(auraPlayer):crystalStack.invoke(auraInv,slot);
        if(!auraHolds(stack,support?auraObsidianItem:crystalItemValue) || auraScore(new double[]{base[0]+.5,base[1]+1.,base[2]+.5},support?base:null)<0) return false;
        double[] top=auraTop(clicked);
        Object hit=crystalHitCtor.newInstance(crystalVecCtor.newInstance(top[0],top[1],top[2]),crystalUp,
            crystalPosCtor.newInstance(clicked[0],clicked[1],clicked[2]),false);
        Object result=crystalPlace.invoke(auraManager,auraPlayer,hand,hit);
        boolean accepted=result!=null && (boolean)crystalAccepted.invoke(result);
        if(accepted) crystalSwing.invoke(auraPlayer,hand);
        return accepted;
    }
    private void auraFinishWait() {auraPhase=0;auraBase=null;auraCrystal=null;auraClearAim();}
    private synchronized void onAuraTick(Object c) {
        if(!auraActive || auraFailed || stopped) return;
        try {runAuraTick(c);}
        catch(Throwable error) {
            auraFailed=true;
            try {cancelAura();} catch(Throwable cleanup) {error.addSuppressed(cleanup);}
            auraStatus=4;System.err.println("[OneStep] Crystal Aura Java controller suspended");error.printStackTrace();
        }
    }
    private void runAuraTick(Object c) throws ReflectiveOperationException {
        if(c!=client() || auraService(4,null,-1,auraGeneration,auraEpoch)==0 || System.nanoTime()-auraConfigTime>500_000_000L) {cancelAura();return;}
        Object p=player.get(c),w=world.get(c),m=manager.get(c);
        if(!live(c,p,w) || m==null || auraCamera.invoke(c)!=p) {cancelAura();return;}
        Object inv=crystalInventory.invoke(p);
        if(p!=auraPlayer || w!=auraWorld || m!=auraManager || inv!=auraInv) {
            auraService(2,null,-1,0,0);auraCycle();auraOldSlot=auraOwnedSlot=-1;
            auraBreakAttempts.clear();auraRejectedSites.clear();auraPlaceReady=auraBreakReady=0;
            auraPlayer=p;auraWorld=w;auraManager=m;auraInv=inv;auraLastTick=-1;auraEpoch=next();if(auraEpoch==0) return;
        }
        int tick=auraTicks.getInt(p);if(tick==auraLastTick) return;auraLastTick=tick;
        long now=System.nanoTime();if(now<auraManualPause) {auraStatus=9;return;}
        int selected=crystalSelected.getInt(inv);if(selected<0 || selected>8) {cancelAura();return;}
        if(auraOwnedSlot>=0 && selected!=auraOwnedSlot) {cancelAura();auraManualPause=now+300_000_000L;auraStatus=9;return;}
        if((boolean)crystalUsing.invoke(p)) {cancelAura();auraStatus=3;return;}
        double[] eye=auraPosition(p);eye[1]=(double)crystalEyeY.invoke(p);
        double er=Math.min(auraSetting(7),(double)crystalEntityRange.invoke(p)),br=Math.min(auraSetting(6),(double)crystalBlockRange.invoke(p));
        if(!Double.isFinite(eye[1]) || !Double.isFinite(er) || !Double.isFinite(br) || er<=0 || br<=0 || !auraScan(eye)) {cancelAura();return;}
        Object chosen=auraChoose(eye);if(chosen==null) {cancelAura();return;}
        if(chosen!=auraTarget) {auraCycle();auraTarget=chosen;auraTargetId=(int)crystalId.invoke(chosen);}
        if(auraService(1,auraTarget,auraTargetId,auraGeneration,auraEpoch)==0) {cancelAura();auraStatus=3;return;}
        auraDefenses.clear();auraSawDamageRejection=auraSawLowDamage=auraSawRetryLimit=false;auraStatus=0;
        Object difficulty=auraDifficulty.invoke(w);auraDifficultyValue=difficulty==null?2:(int)auraDifficultyId.invoke(difficulty);
        auraRejectedSites.entrySet().removeIf(entry -> entry.getValue()<=now);
        java.util.Set<Object> liveEntities=java.util.Collections.newSetFromMap(new java.util.IdentityHashMap<>());
        for(Object[] row:auraScene) liveEntities.add(row[0]);
        auraBreakAttempts.keySet().removeIf(e -> !liveEntities.contains(e));
        if(auraBreakAttempts.size()>128) {auraStatus=11;return;}
        if(auraPhase==2) {
            if(auraBaseReady(auraBase)) auraFinishWait();
            else if(now>=auraDeadline) {auraRejectedSites.put(auraSiteKey(auraBase),now+500_000_000L);auraFinishWait();auraStatus=10;return;}
            else {auraStatus=6;return;}
        } else if(auraPhase==4) {
            if(auraCrystalAt(auraBase)!=null) auraFinishWait();
            else if(now>=auraDeadline) {auraRejectedSites.put(auraSiteKey(auraBase),now+500_000_000L);auraFinishWait();auraStatus=10;return;}
            else {auraStatus=6;return;}
        } else if(auraPhase==6) {
            Object live=crystalById.invoke(w,(int)crystalId.invoke(auraCrystal));
            if(live==null || !(boolean)crystalAlive.invoke(auraCrystal)) auraFinishWait();
            else if(live!=auraCrystal) {auraFinishWait();auraStatus=10;return;}
            else if(now>=auraDeadline) {auraFinishWait();auraStatus=10;return;}
            else {auraStatus=6;return;}
        }
        Object victim=auraFlag(2) && now>=auraBreakReady?auraBestCrystal(eye,er):null;
        int action=-1,slot=selected;Object hand=crystalHand;int[] base=null;double[] aim=null;
        if(victim!=null) {
            slot=auraWeaknessSlot();if(slot<0) {auraStatus=12;return;}
            action=3;aim=auraCrystalAim(eye,auraBox(victim),er);auraPhase=5;
        } else if(auraFlag(1) && now>=auraPlaceReady) {
            boolean offhand=auraHolds(auraOffhand.invoke(p),crystalItemValue);
            int crystals=auraItemSlot(crystalItemValue);
            if(!offhand && crystals<0) {auraStatus=1;return;}
            base=auraPlan(eye,br,er,now);
            if(base==null) {auraStatus=auraSawDamageRejection?7:auraSawLowDamage?13:2;return;}
            boolean support=!auraBaseReady(base);action=support?1:2;
            slot=support?auraItemSlot(auraObsidianItem):offhand?selected:crystals;
            hand=!support && offhand?auraOffHand:crystalHand;
            int[] clicked={base[0],base[1]-(support?1:0),base[2]};aim=auraTop(clicked);auraPhase=support?1:3;
        } else {auraStatus=auraSawDamageRejection?7:auraSawLowDamage?13:auraSawRetryLimit?11:0;return;}
        if(!auraPrepare(slot,eye,aim,action,victim,base,now)) return;
        auraPendingAction=action;auraPendingSlot=slot;auraPendingVictim=victim;auraPendingBase=base;auraPendingAim=aim;auraPendingHand=hand;
        boolean submitted;
        try {submitted=auraService(3,auraTarget,auraTargetId,auraGeneration,auraEpoch)!=0;}
        finally {auraPendingAction=-1;auraPendingVictim=null;auraPendingBase=null;auraPendingAim=null;auraPendingHand=null;}
        if(action==3) auraBreakReady=now+Math.max(50,auraConfig[4])*1_000_000L;
        else auraPlaceReady=now+Math.max(50,auraConfig[3])*1_000_000L;
        if(submitted) {
            auraPhase=action==1?2:action==2?4:6;auraBase=base;auraCrystal=victim;auraDeadline=now+auraConfig[2]*1_000_000L;
            if(action==3) auraBreakAttempts.put(victim,auraBreakAttempts.getOrDefault(victim,0)+1);
            auraStatus=6;
        } else {auraPhase=0;auraStatus=10;}
    }

    public synchronized void stop() {
        try { cancelAura(); } catch (ReflectiveOperationException ignored) {}
        auraActive=false; auraPlayer=auraWorld=auraManager=auraInv=null; auraCycle();
        auraScene=List.of();auraDefenses.clear();auraBreakAttempts.clear();auraRejectedSites.clear();
        safeAnchorTickActive=false;
        cancelTotem();
        legitActive=false; legitFakeUse=false; legitPulses.clear(); legitUseOwner=null; legitPulseMouse=null;
        if (legitEvents != null) try { legitStopEvents.invoke(null,this); } catch (ReflectiveOperationException ignored) {}
        legitEvents=null;
        anchorPlayer=anchorWorld=anchorManager=anchorInv=anchorTarget=anchorSupport=anchorFace=null;
        anchorUseOwner=null; anchorOwned.clear(); anchorOwnershipWorld=anchorOwnershipPlayer=null; anchorStage=0;
        jumpSeeded = jumpHolding = jumpPending = false;
        jumpPlayer = jumpWorld = jumpOptions = jumpBinding = jumpInput = jumpPreviousSource = null;
        jumpOwner = null;
        triggerActive = false;
        triggerPlayer = triggerWorld = triggerManager = triggerTarget = triggerInv = null;
        triggerCycleSeeded = false; triggerReady = triggerNotBefore = 0;
        stopped = true; sneakOwner = pearlOwner = null; bridgeNonce = pearlNonce = 0;
        crystalPlayer = crystalWorld = crystalManager = crystalInv = crystalBase = crystalTarget = crystalArea = null;
        crystalOldSlot = crystalOwnedSlot = crystalTargetId = -1; crystalStage = 0;
        crystalUseOwner = null;
    }
}
