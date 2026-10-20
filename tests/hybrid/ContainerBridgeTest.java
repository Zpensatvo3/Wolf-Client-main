package net.minecraft;

import net.onestep.hybrid.InventoryBridge;

public final class ContainerBridgeTest {
    private static int checks;
    private static void check(boolean value, String message) {
        ++checks; if (!value) throw new AssertionError(message);
    }
    private static InventoryBridge bridge() throws Exception {
        return new InventoryBridge(ContainerBridgeTest.class.getClassLoader(), 12111);
    }
    private static class_310 setup() {
        class_310 c = new class_310(); class_310.field_1700 = c;
        class_465 s = new class_465(); c.field_1755 = s;
        class_1703 h = s.field_2797; c.field_1724.field_7512 = h; h.simulate = h.detailed = true;
        // Non-PlayerScreenHandler layout: main indices 9..35 precede hotbar 0..8.
        for (int i = 0; i < 36; ++i) {
            class_1735 slot = new class_1735(i);
            slot.field_7871 = c.field_1724.inventory; slot.field_7875 = (i + 9) % 36;
            h.field_7761.add(slot);
        }
        s.field_2787 = slot(c, 0);
        class_1799 potion = new class_1799(new class_1812());
        potion.potion = new class_1844(class_1847.field_8963); potion.maximum = 1;
        slot(c, 0).stack = potion;
        return c;
    }
    private static class_1735 slot(class_310 c, int id) { return (class_1735) c.field_1724.field_7512.field_7761.get(id); }
    private static double[] data(Object[] packet) { return (double[]) packet[10]; }
    public static void main(String[] args) throws Exception {
        class_310 c = setup(); InventoryBridge b = bridge();
        Object[] p = b.captureContainer(4);
        check(p != null && data(p)[10 + 1] == 9 && data(p)[10 + 4] == 1, "component-based health potion");
        check(data(p)[10 + 27 * 9 + 1] == 0, "custom hotbar mapping");
        check(!b.containerAct(p, 0, 0, 27, true, false, false), "manual mouse pauses refill");
        p = b.captureContainer(4);
        check(b.containerAct(p, 0, 0, 27, false, false, false), "single vanilla SWAP");
        check(!b.containerAct(p, 0, 0, 27, false, false, false), "no repeat packet");
        check(slot(c, 0).stack.method_7960() && !slot(c, 27).stack.method_7960(), "moved to actual hotbar");
        check(data(b.captureContainer(4))[9] == 1, "subsequent local consumption observation");
        b.cancel(4); check(data(b.captureContainer(4))[9] == 0, "ack ownership released");

        c = setup(); b = bridge(); p = b.captureContainer(4);
        slot(c, 27).stack = new class_1799(new class_1792());
        check(!b.containerAct(p, 0, 0, 27, false, false, false), "occupied hotbar preserved");
        c = setup(); b = bridge(); p = b.captureContainer(4); slot(c, 0).stack.potion = new class_1844(new class_6880());
        check(!b.containerAct(p, 0, 0, 27, false, false, false), "potion component changed");
        check(data(b.captureContainer(4))[10 + 4] == 0, "nonhealing potion excluded");
        slot(c, 0).stack.potion = new class_1844(class_1847.field_8980);
        check(data(b.captureContainer(4))[10 + 4] == 1, "strong healing supported");
        slot(c, 28).field_7875 = 0; p = b.captureContainer(4);
        check(!b.containerAct(p, 0, 0, 27, false, false, false), "ambiguous hotbar rejected");
        c.field_1755 = new class_481(); check(b.captureContainer(4) == null, "refill excludes creative");

        c = setup(); b = bridge(); class_1703 h = c.field_1724.field_7512;
        class_1792 item = new class_1792();
        h.carried = new class_1799(item); h.carried.stackCount = 8;
        slot(c, 0).stack = new class_1799(null);
        p = b.captureContainer(5);
        check(b.containerAct(p, 1, 0, -1, false, true, false), "one right-click item");
        check(h.carried.stackCount == 7 && slot(c, 0).stack.stackCount == 1, "RMB places exactly one");
        p = b.captureContainer(5);
        check(!b.containerAct(p, 1, 0, -1, false, false, false), "right physical release");
        p = b.captureContainer(5); h.carried.components++;
        check(!b.containerAct(p, 1, 0, -1, false, true, false), "cursor component mutation");
        h.carried.components = 0; slot(c, 0).insertable = false;
        check(!b.containerAct(b.captureContainer(5), 1, 0, -1, false, true, false), "slot insertion rule");
        slot(c, 0).insertable = true; slot(c, 0).stack.stackCount = 64;
        check(!b.containerAct(b.captureContainer(5), 1, 0, -1, false, true, false), "stack capacity");
        slot(c, 0).stack.stackCount = 5;
        p = b.captureContainer(5); int before = c.field_1761.inventoryClicks;
        check(b.containerAct(p, 3, 0, -1, true, false, false), "LMB merged stack returns to cursor");
        check(c.field_1761.inventoryClicks == before + 2 && h.carried.stackCount == 12 && slot(c, 0).stack.method_7960(), "two validated merge clicks");
        slot(c, 0).stack = new class_1799(item); slot(c, 0).stack.stackCount = 3; slot(c, 0).insertable = false;
        before = c.field_1761.inventoryClicks;
        check(b.containerAct(b.captureContainer(5), 3, 0, -1, true, false, false), "output slot collection");
        check(c.field_1761.inventoryClicks == before + 1 && h.carried.stackCount == 15, "output slot one click");
        slot(c, 0).stack = new class_1799(item); slot(c, 0).stack.stackCount = 60;
        check(!b.containerAct(b.captureContainer(5), 3, 0, -1, true, false, false), "whole-stack merge capacity");
        slot(c, 0).stack.stackCount = 1;
        check(b.containerAct(b.captureContainer(5), 2, 0, -1, true, false, true), "shift quick move");
        slot(c, 0).stack = new class_1799(item); p = b.captureContainer(5);
        ((class_465) c.field_1755).field_2787 = slot(c, 1);
        check(!b.containerAct(p, 2, 0, -1, true, false, true), "hover rechecked");
        p = b.captureContainer(5); c.field_1687 = new class_638();
        check(!b.containerAct(p, 1, 0, -1, false, true, false), "world identity rechecked");
        c = setup(); b = bridge(); p = b.captureContainer(4); c.field_1761.failInventory = true;
        boolean failed = false;
        try { b.containerAct(p, 0, 0, 27, false, false, false); } catch (ReflectiveOperationException expected) { failed = true; }
        check(failed && !b.containerAct(p, 0, 0, 27, false, false, false) && c.field_1761.inventoryClicks == 1,
            "failure after mutation cannot replay");
        b.stop(); check(b.captureContainer(5) == null, "shutdown rejects work");
        c = setup();
        InventoryBridge isolated = new InventoryBridge(new ClassLoader(ContainerBridgeTest.class.getClassLoader()) {
            @Override protected Class<?> loadClass(String name, boolean resolve) throws ClassNotFoundException {
                if (name.equals("net.minecraft.class_9473")) throw new ClassNotFoundException(name);
                return super.loadClass(name, resolve);
            }
        }, 12111);
        failed = false;
        try { isolated.prepare(4); } catch (ClassNotFoundException expected) { failed = true; }
        check(failed && isolated.captureContainer(5) != null, "refill binding failure isolated from mouse");
        System.out.println("PASS container bridge: " + checks + " checks (" + args[0] + ")");
    }
}
