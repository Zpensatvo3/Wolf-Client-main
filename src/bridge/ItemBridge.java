package net.onestep.hybrid;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.Arrays;
import java.util.List;

/** Original OneStep item I/O. Decisions/timers stay native; packets are frame-local and single-use. */
public final class ItemBridge {
    public static final int AUTO_TOOL = 1, FAST_EXP = 2, LEFT_CLICKER = 3, RIGHT_CLICKER = 4;
    private final ClassLoader loader;
    private final Class<?> clientType, playerType;
    private final Field instance, player, world, screen;
    private final Method onThread, alive;
    private final boolean[] prepared = new boolean[5];
    private final long[] clickSequence = new long[5], completedClick = new long[5];
    private Method usingItem, attack, count;
    private Field yaw, pitch;
    private Class<?> blockItemType;
    private Field manager, crosshair, blockType, selected, main, expItem, useCooldown;
    private Method inventory, hitType, blockPosition, blockState, air, speed, suitable, empty;
    private Method syncSlot, mainHand, offHand, getItem, useItem;
    private Class<?> blockHitType;
    private long sequence, toolSequence, expSequence;
    private volatile boolean stopped;

    public ItemBridge(ClassLoader loader, int target) throws ReflectiveOperationException {
        if (loader == null || target != 12111) throw new IllegalArgumentException("ItemBridge requires Fabric 1.21.11");
        this.loader = loader;
        clientType = load("class_310"); playerType = load("class_746");
        instance = field(clientType, "field_1700", clientType, true);
        player = field(clientType, "field_1724", playerType, false);
        world = field(clientType, "field_1687", load("class_638"), false);
        screen = field(clientType, "field_1755", load("class_437"), false);
        onThread = method(clientType, "method_18854", boolean.class);
        alive = method(playerType, "method_5805", boolean.class);
    }
    private Class<?> load(String name) throws ClassNotFoundException {
        return Class.forName("net.minecraft." + name, false, loader);
    }
    private static Field field(Class<?> owner, String name, Class<?> type, boolean isStatic)
            throws ReflectiveOperationException {
        for (Class<?> c = owner; c != null; c = c.getSuperclass()) {
            try {
                Field f = c.getDeclaredField(name);
                if (f.getType() != type || Modifier.isStatic(f.getModifiers()) != isStatic || !f.trySetAccessible())
                    throw new IllegalAccessException("ItemBridge field mismatch: " + name);
                return f;
            } catch (NoSuchFieldException missing) { /* Exact inherited member only. */ }
        }
        throw new NoSuchFieldException(name);
    }
    private static Method method(Class<?> owner, String name, Class<?> result, Class<?>... arguments)
            throws ReflectiveOperationException {
        for (Class<?> c = owner; c != null; c = c.getSuperclass()) {
            try {
                Method m = c.getDeclaredMethod(name, arguments);
                if (m.getReturnType() != result || Modifier.isStatic(m.getModifiers()) || !m.trySetAccessible())
                    throw new IllegalAccessException("ItemBridge method mismatch: " + name);
                return m;
            } catch (NoSuchMethodException missing) { /* Exact inherited member only. */ }
        }
        throw new NoSuchMethodException(name);
    }
    private static boolean yes(Method method, Object object) throws ReflectiveOperationException {
        return object != null && (boolean) method.invoke(object);
    }
    public void prepare(int feature) throws ReflectiveOperationException {
        if (stopped || feature < 1 || feature > 4) throw new IllegalArgumentException("Invalid item feature/lifecycle");
        if (prepared[feature]) return;
        Class<?> stack = load("class_1799");
        empty = method(stack, "method_7960", boolean.class);
        if (feature == AUTO_TOOL) {
            Class<?> inv = load("class_1661"), hit = load("class_239"), type = load("class_239$class_240");
            Class<?> state = load("class_2680"), pos = load("class_2338"), interaction = load("class_636");
            blockHitType = load("class_3965");
            manager = field(clientType, "field_1761", interaction, false);
            crosshair = field(clientType, "field_1765", hit, false);
            blockType = field(type, "field_1332", type, true);
            hitType = method(hit, "method_17783", type);
            blockPosition = method(blockHitType, "method_17777", pos);
            blockState = method(load("class_638"), "method_8320", state, pos);
            air = method(state, "method_26215", boolean.class);
            inventory = method(playerType, "method_31548", inv);
            selected = field(inv, "field_7545", int.class, false);
            main = field(inv, "field_7547", load("class_2371"), false);
            speed = method(stack, "method_7924", float.class, state);
            suitable = method(stack, "method_7951", boolean.class, state);
            syncSlot = method(interaction, "method_2911", void.class);
        } else if (feature == FAST_EXP) {
            mainHand = method(playerType, "method_6047", stack);
            offHand = method(playerType, "method_6079", stack);
            getItem = method(stack, "method_7909", load("class_1792"));
            expItem = field(load("class_1802"), "field_8287", load("class_1792"), true);
            useCooldown = field(clientType, "field_1752", int.class, false);
            useItem = method(clientType, "method_1583", void.class);
        } else if (feature == LEFT_CLICKER || feature == RIGHT_CLICKER) {
            usingItem = method(playerType, "method_6115", boolean.class);
            yaw = field(playerType, "field_6031", float.class, false);
            pitch = field(playerType, "field_5965", float.class, false);
            if (feature == LEFT_CLICKER) attack = method(clientType, "method_1536", boolean.class);
            else {
                mainHand = method(playerType, "method_6047", stack);
                getItem = method(stack, "method_7909", load("class_1792"));
                count = method(stack, "method_7947", int.class);
                blockItemType = load("class_1747");
                useCooldown = field(clientType, "field_1752", int.class, false);
                useItem = method(clientType, "method_1583", void.class);
            }
        }
        prepared[feature] = true;
    }
    private Object clientOnThread() throws ReflectiveOperationException {
        if (stopped) return null;
        Object client = instance.get(null);
        return client != null && yes(onThread, client) ? client : null;
    }
    private boolean valid(Object client, Object p, Object w) throws ReflectiveOperationException {
        return p != null && w != null && screen.get(client) == null && yes(alive, p);
    }
    private long nextSequence() {
        if (++sequence >= (1L << 53)) { stopped = true; return 0; }
        return sequence;
    }

    /** {owner, player, world, inventory, manager, position, state, stacks[9], double[21]}. */
    public Object[] captureTools(boolean requireClick, boolean physicalLeft) throws ReflectiveOperationException {
        Object client = clientOnThread();
        if (client == null) return null;
        toolSequence = 0;
        if (requireClick && !physicalLeft) return null;
        prepare(AUTO_TOOL);
        Object p = player.get(client), w = world.get(client);
        if (!valid(client, p, w)) return null;
        Object interaction = manager.get(client), hit = crosshair.get(client);
        if (interaction == null || !blockHitType.isInstance(hit) || hitType.invoke(hit) != blockType.get(null)) return null;
        Object pos = blockPosition.invoke(hit), state = pos == null ? null : blockState.invoke(w, pos);
        if (state == null || yes(air, state)) return null;
        Object inv = inventory.invoke(p);
        if (inv == null || !(main.get(inv) instanceof List<?> stacks) || stacks.size() < 9) return null;
        int slot = selected.getInt(inv);
        if (slot < 0 || slot > 8) return null;
        double[] values = new double[21];
        values[0] = 1; values[1] = slot;
        Object[] held = new Object[9];
        for (int i = 0; i < 9; ++i) {
            Object stack = held[i] = stacks.get(i);
            float multiplier = 1;
            boolean matching = false;
            if (stack != null && !yes(empty, stack)) {
                multiplier = (float) speed.invoke(stack, state);
                matching = (boolean) suitable.invoke(stack, state);
            }
            if (!Float.isFinite(multiplier) || multiplier < 0) return null;
            values[3 + i] = multiplier; values[12 + i] = matching ? 1 : 0;
        }
        values[2] = toolSequence = nextSequence();
        return stopped ? null : new Object[]{this, p, w, inv, interaction, pos, state, held, values};
    }
    private double[] consume(Object[] packet, int size, int dataIndex, int length, boolean tool)
            throws ReflectiveOperationException {
        if (clientOnThread() == null || packet == null || packet.length != size || packet[0] != this ||
            !(packet[dataIndex] instanceof double[] values) || values.length != length || values[0] != 1) return null;
        int sequenceIndex = tool ? 2 : 1;
        if (values[sequenceIndex] <= 0 || values[sequenceIndex] != (tool ? toolSequence : expSequence)) return null;
        if (tool) toolSequence = 0; else expSequence = 0;
        return values;
    }
    public boolean selectTool(Object[] packet, int slot, boolean requireClick, boolean physicalLeft)
            throws ReflectiveOperationException {
        double[] before = consume(packet, 9, 8, 21, true);
        if (before == null || slot < 0 || slot > 8 || !(packet[7] instanceof Object[] oldStacks)) return false;
        Object[] now = captureTools(requireClick, physicalLeft);
        toolSequence = 0;
        if (now == null || oldStacks.length != 9 || now[1] != packet[1] || now[2] != packet[2] ||
            now[3] != packet[3] || now[4] != packet[4] || !now[5].equals(packet[5]) || now[6] != packet[6]) return false;
        double[] current = (double[]) now[8];
        if (before[1] != current[1] || !Arrays.equals(before, 3, 21, current, 3, 21)) return false;
        Object[] stacks = (Object[]) now[7];
        for (int i = 0; i < 9; ++i) if (stacks[i] != oldStacks[i]) return false;
        if ((int) current[1] == slot) return true;
        // Same transaction as native hotbar::select: local slot then vanilla synchronizer.
        selected.setInt(now[3], slot);
        syncSlot.invoke(now[4]);
        return true;
    }

    /** {owner, player, world, mainStack, offStack, double[4]: schema/sequence/mainEXP/offEXP}. */
    public Object[] captureExp() throws ReflectiveOperationException {
        Object client = clientOnThread();
        if (client == null) return null;
        expSequence = 0;
        prepare(FAST_EXP);
        Object p = player.get(client), w = world.get(client);
        if (!valid(client, p, w)) return null;
        Object mainStack = mainHand.invoke(p), offStack = offHand.invoke(p), exp = expItem.get(null);
        if (mainStack == null || offStack == null || exp == null) return null;
        boolean mainExp = !yes(empty, mainStack) && getItem.invoke(mainStack) == exp;
        boolean offExp = !yes(empty, offStack) && getItem.invoke(offStack) == exp;
        double[] values = {1, expSequence = nextSequence(), mainExp ? 1 : 0, offExp ? 1 : 0};
        return stopped ? null : new Object[]{this, p, w, mainStack, offStack, values};
    }
    public boolean useExp(Object[] packet, boolean physicalRight) throws ReflectiveOperationException {
        double[] before = consume(packet, 6, 5, 4, false);
        if (before == null || !physicalRight || (before[2] != 1 && before[3] != 1)) return false;
        Object[] now = captureExp();
        expSequence = 0;
        if (now == null || now[1] != packet[1] || now[2] != packet[2] || now[3] != packet[3] || now[4] != packet[4])
            return false;
        double[] current = (double[]) now[5];
        if (before[2] != current[2] || before[3] != current[3]) return false;
        Object client = clientOnThread();
        if (client == null) return false;
        useCooldown.setInt(client, 0);
        useItem.invoke(client); // Exactly one request; no second SendInput click.
        return true;
    }
    /** {owner, player, world, mainStack (right only), double[4]: schema/feature/nonce/onlyBlocks}. */
    public Object[] captureClick(int feature, boolean onlyBlocks) throws ReflectiveOperationException {
        if (feature != LEFT_CLICKER && feature != RIGHT_CLICKER) return null;
        Object client = clientOnThread();
        if (client == null) return null;
        clickSequence[feature] = completedClick[feature] = 0;
        prepare(feature);
        Object p = player.get(client), w = world.get(client);
        if (!valid(client, p, w) || yes(usingItem, p)) return null;
        Object held = feature == RIGHT_CLICKER ? mainHand.invoke(p) : null;
        if (feature == RIGHT_CLICKER && (held == null || (onlyBlocks && !holdingBlock(held)))) return null;
        double[] values = {1, feature, clickSequence[feature] = nextSequence(), onlyBlocks ? 1 : 0};
        return stopped ? null : new Object[]{this, p, w, held, values};
    }
    private boolean holdingBlock(Object held) throws ReflectiveOperationException {
        return !yes(empty, held) && (int) count.invoke(held) > 0 && blockItemType.isInstance(getItem.invoke(held));
    }
    private boolean validClick(Object[] packet, double[] values) throws ReflectiveOperationException {
        Object client = clientOnThread();
        if (client == null || player.get(client) != packet[1] || world.get(client) != packet[2] ||
            !valid(client, packet[1], packet[2]) || yes(usingItem, packet[1])) return false;
        if ((int) values[1] == RIGHT_CLICKER) {
            Object held = mainHand.invoke(packet[1]);
            if (held == null || held != packet[3] || (values[3] == 1 && !holdingBlock(held))) return false;
        }
        return true;
    }
    public boolean click(Object[] packet, boolean gameInput, boolean physicalHeld) throws ReflectiveOperationException {
        if (clientOnThread() == null || packet == null || packet.length != 5 || packet[0] != this ||
            !(packet[4] instanceof double[] values) || values.length != 4 || values[0] != 1) return false;
        int feature = (int) values[1];
        if ((feature != LEFT_CLICKER && feature != RIGHT_CLICKER) || values[2] <= 0 ||
            values[2] != clickSequence[feature]) return false;
        clickSequence[feature] = 0;
        if (!physicalHeld || !validClick(packet, values)) return false;
        Object client = clientOnThread();
        if (feature == RIGHT_CLICKER) useCooldown.setInt(client, 0);
        // Game Input remains the shared native Mouse callback service. This call
        // only validates/consumes its decision; it never adds a second click.
        if (!gameInput) {
            if (feature == LEFT_CLICKER) attack.invoke(client); else useItem.invoke(client);
        }
        completedClick[feature] = (long) values[2];
        return true;
    }
    public boolean jitter(Object[] packet, float yawDelta, float pitchDelta) throws ReflectiveOperationException {
        if (clientOnThread() == null || packet == null || packet.length != 5 || packet[0] != this ||
            !(packet[4] instanceof double[] values) || values.length != 4 || values[0] != 1 ||
            !Float.isFinite(yawDelta) || !Float.isFinite(pitchDelta)) return false;
        int feature = (int) values[1];
        if ((feature != LEFT_CLICKER && feature != RIGHT_CLICKER) || values[2] <= 0 ||
            values[2] != completedClick[feature]) return false;
        completedClick[feature] = 0;
        if (!validClick(packet, values)) return false;
        float y = yaw.getFloat(packet[1]), p = pitch.getFloat(packet[1]);
        if (!Float.isFinite(y) || !Float.isFinite(p) || !Float.isFinite(y + yawDelta)) return false;
        yaw.setFloat(packet[1], y + yawDelta);
        pitch.setFloat(packet[1], Math.max(-90, Math.min(90, p + pitchDelta)));
        return true;
    }
    public void stop() {
        stopped = true; toolSequence = expSequence = 0;
        Arrays.fill(clickSequence, 0); Arrays.fill(completedClick, 0);
    }
}
