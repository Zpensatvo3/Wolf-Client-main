package net.minecraft;

import net.onestep.hybrid.InventoryBridge;

public final class InventoryBridgeTest {
    private static int checks;
    private static void check(boolean condition, String message) {
        ++checks; if (!condition) throw new AssertionError(message);
    }
    private static InventoryBridge bridge() throws Exception {
        return new InventoryBridge(InventoryBridgeTest.class.getClassLoader(), 12111);
    }
    private static class_310 setup() {
        class_310 c = new class_310(); class_310.field_1700 = c;
        class_1723 h = c.field_1724.field_7498;
        for (int i = 0; i < 46; ++i) h.field_7761.add(new class_1735(i));
        h.simulate = true; c.field_1724.field_7512 = h;
        class_490 s = new class_490(); s.field_2797 = h; c.field_1755 = s;
        slot(h, 9).stack = new class_1799(new class_1792("item.minecraft.elytra"));
        slot(h, 6).stack = new class_1799(new class_1792("item.minecraft.diamond_chestplate"));
        return c;
    }
    private static class_1735 slot(class_1703 h, int index) { return (class_1735) h.field_7761.get(index); }
    private static int stage(Object[] packet) { return (int) ((double[]) packet[10])[5]; }
    public static void main(String[] args) throws Exception {
        class_310 c = setup(); class_1703 h = c.field_1724.field_7512;
        InventoryBridge b = bridge();
        Object[] p = b.capture(3, false);
        check(p != null && stage(p) == 0, "idle player handler");
        String[] keys = (String[]) p[9];
        check(keys[9].endsWith(".elytra") && keys[38].endsWith("_chestplate"), "native inventory mapping");
        check(b.act(p, 9, 6, 0, false), "pickup source");
        check(!b.act(p, 9, 6, 0, false), "single-use packet");
        p = b.capture(3, false);
        check(stage(p) == 1 && !h.carried.method_7960(), "owned cursor");
        check(b.act(p, 9, 6, 1, false), "equip chest");
        p = b.capture(3, false);
        check(stage(p) == 2 && b.act(p, 9, 6, 2, false), "return old armor");
        check(stage(b.capture(3, false)) == 3 && h.carried.method_7960(), "completed three clicks");
        check(slot(h, 6).stack.method_7909().method_7876().endsWith(".elytra") &&
            slot(h, 9).stack.method_7909().method_7876().endsWith("_chestplate"), "no lost items");
        b.cancel(3); check(stage(b.capture(3, false)) == 0, "cancel releases ownership");

        c = setup(); h = c.field_1724.field_7512; b = bridge();
        p = b.capture(2, false); slot(h, 9).stack.components++;
        check(!b.act(p, 9, 6, 0, false) && c.field_1761.inventoryClicks == 0, "component mutation rejected");
        p = b.capture(2, false); slot(h, 9).stack.stackCount++;
        check(!b.act(p, 9, 6, 0, false), "count mutation rejected");
        p = b.capture(2, false); h.field_7761.set(9, new class_1735(9));
        check(!b.act(p, 9, 6, 0, false), "slot replacement rejected");

        c = setup(); h = c.field_1724.field_7512; b = bridge();
        check(b.act(b.capture(3, false), 9, 6, 0, false), "new swap");
        h.carried.components++;
        check(stage(b.capture(3, false)) == -1, "manual cursor change invalidates transaction");
        int clicks = c.field_1761.inventoryClicks; b.cancel(3);
        check(clicks == c.field_1761.inventoryClicks && !h.carried.method_7960(), "cancel never clicks user's cursor");

        c = setup(); h = c.field_1724.field_7512; b = bridge();
        c.field_1755 = null;
        p = b.capture(3, false);
        check(stage(p) == -2 && !b.act(p, 9, 6, 0, false), "closed inventory waits when disabled");
        check(b.act(b.capture(3, true), 9, 6, 0, true), "closed inventory permitted mode");
        class_490 changedScreen = new class_490(); changedScreen.field_2797 = h; c.field_1755 = changedScreen;
        check(stage(b.capture(3, true)) == -1, "screen transition cancels owned swap");

        c = setup(); b = bridge(); p = b.capture(2, true);
        c.field_1687 = new class_638();
        check(!b.act(p, 9, 6, 0, true), "world identity rechecked");
        p = b.capture(2, true); c.field_1724.field_7512.field_7763++;
        check(!b.act(p, 9, 6, 0, true), "sync id rechecked");
        c.field_1724.field_7512 = new class_1723();
        check(b.capture(2, true) == null, "only player's own handler");

        c = setup(); b = bridge();
        class_476 chest = new class_476(); h = chest.field_2797;
        h.simulate = true;
        for (int i = 0; i < 45; ++i) h.field_7761.add(new class_1735(i));
        slot(h, 2).stack = new class_1799(new class_1792());
        slot(h, 40).stack = new class_1799(new class_1792());
        c.field_1755 = chest; c.field_1724.field_7512 = h;
        check(b.capture(2, true) == null, "armor rejects chest handler");
        p = b.capture(1, false);
        double[] data = (double[]) p[10];
        check(data[6] == 9 && data[7] == 1 && data[8] == 2, "chest candidates exclude player inventory");
        check(!b.act(p, 40, -1, 0, false), "chest action bounds");
        p = b.capture(1, false);
        check(b.act(p, 2, -1, 0, false) && slot(h, 2).stack.method_7960(), "quick move once");
        check(!b.act(p, 2, -1, 0, false), "quick move no replay");

        c = setup(); b = bridge(); final InventoryBridge threaded = b;
        final boolean[] wrongThread = {false};
        Thread thread = new Thread(() -> {
            try { wrongThread[0] = threaded.capture(2, true) == null; }
            catch (Exception e) { throw new AssertionError(e); }
        });
        thread.start(); thread.join(); check(wrongThread[0], "client thread only");
        p = b.capture(2, true); c.field_1761.failInventory = true;
        boolean failed = false;
        try { b.act(p, 9, 6, 0, true); } catch (ReflectiveOperationException expected) { failed = true; }
        check(failed && c.field_1761.inventoryClicks == 1, "exception after mutation not reported as success");
        check(!b.act(p, 9, 6, 0, true) && c.field_1761.inventoryClicks == 1, "partial operation cannot replay");
        b.stop(); check(b.capture(2, true) == null, "shutdown rejects captures");

        c = setup();
        InventoryBridge isolated = new InventoryBridge(new ClassLoader(InventoryBridgeTest.class.getClassLoader()) {
            @Override protected Class<?> loadClass(String name, boolean resolve) throws ClassNotFoundException {
                if (name.equals("net.minecraft.class_476")) throw new ClassNotFoundException(name);
                return super.loadClass(name, resolve);
            }
        }, 12111);
        boolean bindingFailed = false;
        try { isolated.prepare(1); } catch (ClassNotFoundException expected) { bindingFailed = true; }
        check(bindingFailed && isolated.capture(2, true) != null, "capability binding isolation");
        System.out.println("PASS inventory bridge: " + checks + " checks (" + args[0] + ")");
    }
}
