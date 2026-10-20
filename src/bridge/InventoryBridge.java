package net.onestep.hybrid;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Original OneStep inventory I/O. Ranking, ordering and delays remain native. */
public final class InventoryBridge {
    public static final int CHEST = 1, ARMOR = 2, ELYTRA = 3;
    public static final int REFILL = 4, MOUSE = 5;
    private final ClassLoader loader;
    private final Class<?> playerType;
    private final Field instance, player, world, screen, manager, currentHandler;
    private final Method onThread, alive;
    private final boolean[] prepared = new boolean[6];
    private final long[] nonce = new long[6];
    // Explicitly owned swaps only; cancel/stop drop these refs without issuing clicks.
    private final Object[][] swaps = new Object[6][];
    private Object[] refill;
    private Field focused, slotInventory, slotIndex, swapAction, potionComponent, healing, strongHealing;
    private Method playerInventory, enabled, insert, limit, maxCount, component, matches;
    private Class<?> creativeScreen, potionItem;
    private Field handler, slots, slotId, syncId, pickup, quickMove, ownHandler;
    private Method getStack, cursor, empty, copy, count, same, item, key, click;
    private Class<?> handled, chestScreen, inventoryScreen, playerHandler;
    private long sequence;
    private volatile boolean stopped;

    public InventoryBridge(ClassLoader loader, int target) throws ReflectiveOperationException {
        if (loader == null || target != 12111) throw new IllegalArgumentException("InventoryBridge requires Fabric 1.21.11");
        this.loader = loader;
        Class<?> client = load("class_310"); playerType = load("class_746");
        instance = field(client, "field_1700", client, true);
        player = field(client, "field_1724", playerType, false);
        world = field(client, "field_1687", load("class_638"), false);
        screen = field(client, "field_1755", load("class_437"), false);
        manager = field(client, "field_1761", load("class_636"), false);
        currentHandler = field(playerType, "field_7512", load("class_1703"), false);
        onThread = method(client, "method_18854", boolean.class, false);
        alive = method(playerType, "method_5805", boolean.class, false);
    }
    private Class<?> load(String name) throws ClassNotFoundException {
        return Class.forName("net.minecraft." + name, false, loader);
    }
    private static Field field(Class<?> owner, String name, Class<?> type, boolean isStatic) throws ReflectiveOperationException {
        for (Class<?> c = owner; c != null; c = c.getSuperclass()) try {
            Field f = c.getDeclaredField(name);
            if (f.getType() != type || Modifier.isStatic(f.getModifiers()) != isStatic || !f.trySetAccessible())
                throw new IllegalAccessException("InventoryBridge field mismatch: " + name);
            return f;
        } catch (NoSuchFieldException missing) { /* Exact inherited member only. */ }
        throw new NoSuchFieldException(name);
    }
    private static Method method(Class<?> owner, String name, Class<?> result, boolean isStatic, Class<?>... args)
            throws ReflectiveOperationException {
        for (Class<?> c = owner; c != null; c = c.getSuperclass()) try {
            Method m = c.getDeclaredMethod(name, args);
            if (m.getReturnType() != result || Modifier.isStatic(m.getModifiers()) != isStatic || !m.trySetAccessible())
                throw new IllegalAccessException("InventoryBridge method mismatch: " + name);
            return m;
        } catch (NoSuchMethodException missing) { /* Exact inherited member only. */ }
        throw new NoSuchMethodException(name);
    }
    private static void feature(int f) { if (f < 1 || f > 5) throw new IllegalArgumentException("Inventory feature"); }
    public void prepare(int f) throws ReflectiveOperationException {
        feature(f);
        if (stopped) throw new IllegalStateException("InventoryBridge stopped");
        if (prepared[f]) return;
        Class<?> h = load("class_1703"), slot = load("class_1735"), stack = load("class_1799"), action = load("class_1713");
        handled = load("class_465");
        handler = field(handled, "field_2797", h, false);
        slots = field(h, "field_7761", load("class_2371"), false);
        syncId = field(h, "field_7763", int.class, false);
        slotId = field(slot, "field_7874", int.class, false);
        getStack = method(slot, "method_7677", stack, false);
        cursor = method(h, "method_34255", stack, false);
        empty = method(stack, "method_7960", boolean.class, false);
        copy = method(stack, "method_7972", stack, false);
        count = method(stack, "method_7947", int.class, false);
        same = method(stack, "method_31577", boolean.class, true, stack, stack);
        click = method(load("class_636"), "method_2906", void.class, false,
            int.class, int.class, int.class, action, load("class_1657"));
        if (f == CHEST) {
            chestScreen = load("class_476"); quickMove = field(action, "field_7794", action, true);
        } else if (f == ARMOR || f == ELYTRA) {
            inventoryScreen = load("class_490"); playerHandler = load("class_1723");
            ownHandler = field(playerType, "field_7498", playerHandler, false);
            pickup = field(action, "field_7790", action, true);
            item = method(stack, "method_7909", load("class_1792"), false);
            key = method(load("class_1792"), "method_7876", String.class, false);
        } else {
            focused = field(handled, "field_2787", slot, false);
            playerInventory = method(playerType, "method_31548", load("class_1661"), false);
            slotInventory = field(slot, "field_7871", load("class_1263"), false);
            slotIndex = field(slot, "field_7875", int.class, false);
            enabled = method(slot, "method_7682", boolean.class, false);
            insert = method(slot, "method_7680", boolean.class, false, stack);
            limit = method(slot, "method_7676", int.class, false, stack);
            maxCount = method(stack, "method_7914", int.class, false);
            pickup = field(action, "field_7790", action, true);
            quickMove = field(action, "field_7794", action, true);
            if (f == REFILL) {
                creativeScreen = load("class_481"); potionItem = load("class_1812");
                swapAction = field(action, "field_7791", action, true);
                item = method(stack, "method_7909", load("class_1792"), false);
                potionComponent = field(load("class_9334"), "field_49651", load("class_9331"), true);
                // get() is inherited from an interface, not necessarily declared by ItemStack.
                component = method(load("class_9473"), "method_58694", Object.class, false, load("class_9331"));
                matches = method(load("class_1844"), "method_57401", boolean.class, false, load("class_6880"));
                healing = field(load("class_1847"), "field_8963", load("class_6880"), true);
                strongHealing = field(load("class_1847"), "field_8980", load("class_6880"), true);
            }
        }
        prepared[f] = true;
    }
    private Object client() throws ReflectiveOperationException {
        if (stopped) return null;
        Object c = instance.get(null);
        return c != null && (boolean) onThread.invoke(c) ? c : null;
    }
    private boolean equalStack(Object a, Object b) throws ReflectiveOperationException {
        return a != null && b != null && (int) count.invoke(a) == (int) count.invoke(b) && (boolean) same.invoke(null, a, b);
    }
    private boolean swapMatches(Object[] tx, Object p, Object w, Object s, Object h, Object m,
                                Object[] slotObjects, Object[] stacks, Object carried) throws ReflectiveOperationException {
        int[] indices = (int[]) tx[10];
        return tx[0] == p && tx[1] == w && tx[2] == s && tx[3] == h && tx[4] == m &&
            indices[3] == syncId.getInt(h) && indices[0] < stacks.length && indices[1] < stacks.length &&
            tx[5] == slotObjects[indices[0]] && tx[6] == slotObjects[indices[1]] &&
            equalStack(tx[7], stacks[indices[0]]) && equalStack(tx[8], stacks[indices[1]]) && equalStack(tx[9], carried);
    }
    /** {owner,p,w,screen,handler,manager,slots[],copies[],cursorCopy,keys[40],double[8+n]}. */
    public Object[] capture(int f, boolean allowClosed) throws ReflectiveOperationException {
        feature(f);
        if (f > ELYTRA) throw new IllegalArgumentException("Use captureContainer for container features");
        Object c = client(); if (c == null) return null;
        nonce[f] = 0; prepare(f);
        Object p = player.get(c), w = world.get(c), s = screen.get(c), m = manager.get(c);
        if (p == null || w == null || m == null || !(boolean) alive.invoke(p)) { cancel(f); return null; }
        Object h = currentHandler.get(p);
        boolean waitingClosed = f != CHEST && s == null && !allowClosed;
        if (f == CHEST) {
            if (!chestScreen.isInstance(s) || handler.get(s) != h) return null;
        } else if (h == null || !playerHandler.isInstance(h) || h != ownHandler.get(p) ||
            (s != null && (!inventoryScreen.isInstance(s) || handler.get(s) != h))) {
            cancel(f); return null;
        }
        if (h == null || !(slots.get(h) instanceof List<?> list) || list.size() > 4096 || list.size() < (f == CHEST ? 37 : 45)) return null;
        Object[] slotObjects = list.toArray(), copies = new Object[slotObjects.length];
        for (int i = 0; i < copies.length; ++i) {
            if (slotObjects[i] == null || slotId.getInt(slotObjects[i]) != i) return null;
            Object stack = getStack.invoke(slotObjects[i]);
            if (stack == null || (copies[i] = copy.invoke(stack)) == null) return null;
        }
        Object carried = cursor.invoke(h);
        if (carried == null) return null;
        carried = copy.invoke(carried);
        int stage = 0;
        if (swaps[f] != null) {
            if (waitingClosed || !swapMatches(swaps[f], p, w, s, h, m, slotObjects, copies, carried)) { cancel(f); stage = -1; }
            else stage = ((int[]) swaps[f][10])[2];
        } else if (waitingClosed) stage = -2;
        String[] keys = new String[40]; Arrays.fill(keys, "");
        List<Integer> candidates = new ArrayList<>();
        int containerCount = f == CHEST ? copies.length - 36 : 0;
        if (f == CHEST) {
            for (int i = 0; i < containerCount; ++i) if (!(boolean) empty.invoke(copies[i])) candidates.add(i);
        } else for (int i = 0; i < 40; ++i) {
            int index = i < 36 ? (i < 9 ? 36 + i : i) : 44 - i;
            if (!(boolean) empty.invoke(copies[index])) {
                Object type = item.invoke(copies[index]);
                if (type != null) keys[i] = (String) key.invoke(type);
            }
        }
        double[] data = new double[8 + candidates.size()];
        data[0] = 1; data[1] = f; data[2] = nonce[f] = ++sequence; data[3] = syncId.getInt(h);
        data[4] = (boolean) empty.invoke(carried) ? 1 : 0; data[5] = stage;
        data[6] = containerCount; data[7] = candidates.size();
        for (int i = 0; i < candidates.size(); ++i) data[8 + i] = candidates.get(i);
        if (sequence >= (1L << 53)) { stop(); return null; }
        return new Object[]{this, p, w, s, h, m, slotObjects, copies, carried, keys, data};
    }
    public boolean act(Object[] packet, int source, int target, int stage, boolean allowClosed)
            throws ReflectiveOperationException {
        if (client() == null || packet == null || packet.length != 11 || packet[0] != this ||
            !(packet[10] instanceof double[] before) || before.length < 8 || before[0] != 1) return false;
        int f = (int) before[1]; feature(f);
        if (before[2] <= 0 || before[2] != nonce[f]) return false;
        nonce[f] = 0;
        Object[] now = capture(f, allowClosed); nonce[f] = 0;
        if (now == null || before[3] != ((double[]) now[10])[3]) return false;
        for (int i = 1; i <= 5; ++i) if (now[i] != packet[i]) return false;
        Object[] slotObjects = (Object[]) now[6], stacks = (Object[]) now[7], oldStacks = (Object[]) packet[7];
        if (source < 0 || source >= stacks.length || source >= oldStacks.length ||
            !equalStack(now[8], packet[8])) return false;
        if (f == CHEST) {
            if (source >= (int) ((double[]) now[10])[6] || (boolean) empty.invoke(stacks[source]) ||
                !equalStack(oldStacks[source], stacks[source]) || ((Object[]) packet[6])[source] != slotObjects[source]) return false;
            click.invoke(now[5], (int) before[3], source, 0, quickMove.get(null), now[1]);
            return true;
        }
        if (target < 5 || target > 8 || (f == ELYTRA && target != 6) ||
            !(source >= 9 && source <= 44) || target >= stacks.length || target >= oldStacks.length ||
            !equalStack(oldStacks[source], stacks[source]) || !equalStack(oldStacks[target], stacks[target])) return false;
        Object[] tx = swaps[f];
        if (stage == 0) {
            if (tx != null || before[5] != 0 || !(boolean) empty.invoke(now[8])) return false;
        } else if (tx == null || ((int[]) tx[10])[0] != source || ((int[]) tx[10])[1] != target ||
            ((int[]) tx[10])[2] != stage || ((double[]) now[10])[5] != stage || stage < 1 || stage > 2) return false;
        int clicked = stage == 1 ? target : source;
        // Claim before mutating. If an overridable click throws, no fallback may replay it.
        swaps[f] = null;
        click.invoke(now[5], (int) before[3], clicked, 0, pickup.get(null), now[1]);
        Object h = now[4];
        Object a = copy.invoke(getStack.invoke(slotObjects[source]));
        Object b = copy.invoke(getStack.invoke(slotObjects[target]));
        Object carried = copy.invoke(cursor.invoke(h));
        swaps[f] = new Object[]{now[1], now[2], now[3], h, now[5], slotObjects[source], slotObjects[target], a, b, carried,
            new int[]{source, target, stage + 1, (int) before[3]}};
        return true;
    }
    private boolean health(Object stack) throws ReflectiveOperationException {
        if ((boolean) empty.invoke(stack) || !potionItem.isInstance(item.invoke(stack))) return false;
        Object contents = component.invoke(stack, potionComponent.get(null));
        return contents != null && ((boolean) matches.invoke(contents, healing.get(null)) ||
            (boolean) matches.invoke(contents, strongHealing.get(null)));
    }
    private boolean context(Object[] a, Object[] b) {
        for (int i = 1; i <= 6; ++i) if (a[i] != b[i]) return false;
        return ((double[]) a[10])[3] == ((double[]) b[10])[3];
    }
    /** {owner,p,w,screen,handler,manager,inventory,slots[],copies[],cursorCopy,double[10+9*n]}. */
    public Object[] captureContainer(int f) throws ReflectiveOperationException {
        if (f != REFILL && f != MOUSE) throw new IllegalArgumentException("Container feature");
        Object c = client(); if (c == null) return null;
        nonce[f] = 0; prepare(f);
        Object p = player.get(c), w = world.get(c), s = screen.get(c), m = manager.get(c);
        if (p == null || w == null || m == null || !(boolean) alive.invoke(p) || !handled.isInstance(s) ||
            (f == REFILL && creativeScreen.isInstance(s))) return null;
        Object h = handler.get(s), inv = playerInventory.invoke(p);
        if (h == null || h != currentHandler.get(p) || inv == null || !(slots.get(h) instanceof List<?> list) ||
            list.isEmpty() || list.size() > 4096) return null;
        Object carried = copy.invoke(cursor.invoke(h));
        if (carried == null) return null;
        boolean cursorEmpty = (boolean) empty.invoke(carried);
        Object[] objects = list.toArray(), copies = new Object[objects.length];
        double[] data = new double[10 + 9 * objects.length];
        Object hovered = focused.get(s);
        int hover = hovered == null ? -1 : slotId.getInt(hovered);
        if (hovered != null && (hover < 0 || hover >= objects.length || objects[hover] != hovered)) return null;
        data[0] = 1; data[1] = f; data[2] = nonce[f] = ++sequence; data[3] = syncId.getInt(h);
        data[4] = hover; data[5] = cursorEmpty ? 1 : 0;
        data[6] = (int) count.invoke(carried); data[7] = (int) maxCount.invoke(carried); data[8] = objects.length;
        for (int i = 0; i < objects.length; ++i) {
            Object slot = objects[i];
            if (slot == null || slotId.getInt(slot) != i) return null;
            Object stack = copies[i] = copy.invoke(getStack.invoke(slot));
            if (stack == null) return null;
            boolean isEmpty = (boolean) empty.invoke(stack);
            int offset = 10 + i * 9;
            data[offset] = i;
            data[offset + 1] = slotInventory.get(slot) == inv ? slotIndex.getInt(slot) : -1;
            data[offset + 2] = (int) count.invoke(stack); data[offset + 3] = isEmpty ? 1 : 0;
            data[offset + 4] = f == REFILL && !isEmpty && health(stack) ? 1 : 0;
            data[offset + 5] = (boolean) enabled.invoke(slot) ? 1 : 0;
            data[offset + 6] = !cursorEmpty && (boolean) insert.invoke(slot, carried) ? 1 : 0;
            data[offset + 7] = cursorEmpty ? 0 : (int) limit.invoke(slot, carried);
            data[offset + 8] = isEmpty || cursorEmpty || (boolean) same.invoke(null, stack, carried) ? 1 : 0;
        }
        if (refill != null && f == REFILL) {
            int[] indices = (int[]) refill[9];
            boolean sameContext = refill[0] == p && refill[1] == w && refill[2] == s && refill[3] == h &&
                refill[4] == m && refill[5] == inv && indices[2] == (int) data[3];
            if (!sameContext) refill = null;
            else if (indices[0] < objects.length && indices[1] < objects.length &&
                refill[6] == objects[indices[0]] && refill[7] == objects[indices[1]] &&
                (boolean) empty.invoke(copies[indices[0]]) && equalStack(copies[indices[1]], refill[8])) data[9] = 1;
        }
        if (sequence >= (1L << 53)) { stop(); return null; }
        return new Object[]{this, p, w, s, h, m, inv, objects, copies, carried, data};
    }
    /** action: 0=refill SWAP, 1=one RMB item, 2=QUICK_MOVE, 3=merge onto cursor. */
    public boolean containerAct(Object[] packet, int action, int source, int target,
                                boolean left, boolean right, boolean shift) throws ReflectiveOperationException {
        if (client() == null || packet == null || packet.length != 11 || packet[0] != this ||
            !(packet[10] instanceof double[] before) || before.length < 10 || before[0] != 1) return false;
        int f = (int) before[1];
        if ((f != REFILL && f != MOUSE) || before[2] <= 0 || before[2] != nonce[f]) return false;
        nonce[f] = 0;
        Object[] now = captureContainer(f); nonce[f] = 0;
        if (now == null || !context(packet, now) || !equalStack(packet[9], now[9])) return false;
        Object[] objects = (Object[]) now[7], stacks = (Object[]) now[8];
        Object[] oldObjects = (Object[]) packet[7], oldStacks = (Object[]) packet[8];
        if (source < 0 || source >= objects.length || source >= oldObjects.length || oldObjects[source] != objects[source] ||
            !equalStack(oldStacks[source], stacks[source])) return false;
        double[] data = (double[]) now[10]; int offset = 10 + source * 9;
        if (data[offset + 5] != 1) return false;
        if (action == 0) {
            if (f != REFILL || left || right || refill != null || data[5] != 1 || data[offset + 1] < 9 ||
                data[offset + 1] >= 36 || data[offset + 4] != 1 || target < 0 || target >= objects.length ||
                target >= oldObjects.length || objects[target] != oldObjects[target] || !equalStack(stacks[target], oldStacks[target])) return false;
            int to = 10 + target * 9, index = (int) data[to + 1];
            if (index < 0 || index > 8 || data[to + 3] != 1 || data[to + 5] != 1 ||
                !(boolean) insert.invoke(objects[target], stacks[source])) return false;
            // Reject ambiguous custom layouts instead of guessing the hotbar address.
            for (int i = 0; i < objects.length; ++i) if (i != target && data[10 + i * 9 + 1] == index) return false;
            refill = new Object[]{now[1], now[2], now[3], now[4], now[5], now[6], objects[source], objects[target],
                copy.invoke(stacks[source]), new int[]{source, target, (int) data[3]}};
            click.invoke(now[5], (int) data[3], source, index, swapAction.get(null), now[1]);
            return true;
        }
        if (f != MOUSE) return false;
        if (action == 1) {
            if (!right || data[5] == 1 || data[offset + 6] != 1 || data[offset + 8] != 1 ||
                (data[offset + 3] != 1 && data[offset + 2] >= Math.min(data[offset + 7], data[7]))) return false;
            click.invoke(now[5], (int) data[3], source, 1, pickup.get(null), now[1]); return true;
        }
        if (!left || source != (int) data[4] || data[offset + 3] == 1) return false;
        if (action == 2) {
            if (!shift || data[offset + 8] != 1) return false;
            click.invoke(now[5], (int) data[3], source, 0, quickMove.get(null), now[1]); return true;
        }
        if (action != 3 || shift || data[5] == 1 || data[offset + 8] != 1 || data[6] <= 0 ||
            data[offset + 2] <= 0 || data[6] + data[offset + 2] > data[7]) return false;
        boolean output = data[offset + 6] != 1;
        click.invoke(now[5], (int) data[3], source, 0, pickup.get(null), now[1]);
        if (output) return true;
        // No blindly repeated second click: require the predicted merged stack and empty cursor.
        Object[] after = captureContainer(f); nonce[f] = 0;
        if (after == null || !context(now, after) || ((Object[]) after[7])[source] != objects[source] ||
            !(boolean) empty.invoke(after[9])) return false;
        Object combined = ((Object[]) after[8])[source];
        if (!(boolean) same.invoke(null, combined, stacks[source]) ||
            (int) count.invoke(combined) != data[6] + data[offset + 2]) return false;
        click.invoke(after[5], (int) data[3], source, 0, pickup.get(null), after[1]);
        return true;
    }
    public void cancel(int f) { feature(f); swaps[f] = null; nonce[f] = 0; if (f == REFILL) refill = null; }
    public void stop() { stopped = true; refill = null; Arrays.fill(swaps, null); Arrays.fill(nonce, 0); }
}
