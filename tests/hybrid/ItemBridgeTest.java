package net.minecraft;

import java.lang.reflect.Field;
import net.onestep.hybrid.ItemBridge;

public final class ItemBridgeTest {
    private static int checks;
    private static void check(boolean condition, String label) {
        ++checks; if (!condition) throw new AssertionError(label);
    }
    private static ItemBridge bridge() throws Exception {
        return new ItemBridge(ItemBridgeTest.class.getClassLoader(), 12111);
    }
    public static void main(String[] args) throws Exception {
        class_310 client = new class_310(); class_310.field_1700 = client;
        class_746 p = client.field_1724;
        class_3965 hit = new class_3965(); client.field_1765 = hit;
        Field selected = class_1661.class.getDeclaredField("field_7545"); selected.setAccessible(true);
        Field main = class_1661.class.getDeclaredField("field_7547"); main.setAccessible(true);
        class_2371 stacks = (class_2371) main.get(p.inventory);
        class_1799 pick = new class_1799(new class_1792()); pick.miningSpeed = 8; pick.suitable = true;
        stacks.set(4, pick);
        ItemBridge bridge = bridge();
        check(bridge.captureTools(true, false) == null, "physical click gate");
        Object[] tools = bridge.captureTools(true, true);
        double[] scores = (double[]) tools[8];
        check(scores.length == 21 && scores[1] == 0 && scores[7] == 8 && scores[16] == 1, "score wire layout");
        check(bridge.selectTool(tools, 4, true, true) && selected.getInt(p.inventory) == 4 &&
            client.field_1761.syncCount == 1, "local selection and one vanilla sync");
        check(!bridge.selectTool(tools, 0, true, true), "single-use slot decision");
        tools = bridge.captureTools(false, false);
        check(bridge.selectTool(tools, 4, false, false) && client.field_1761.syncCount == 1, "same slot doesn't sync again");
        tools = bridge.captureTools(true, true);
        selected.setInt(p.inventory, 2);
        check(!bridge.selectTool(tools, 4, true, true) && selected.getInt(p.inventory) == 2, "manual slot preserved");
        tools = bridge.captureTools(true, true); hit.pos = new class_2338(2);
        check(!bridge.selectTool(tools, 4, true, true), "crosshair block changed");
        tools = bridge.captureTools(true, true); client.field_1687.state = new class_2680();
        check(!bridge.selectTool(tools, 4, true, true), "block state changed");
        tools = bridge.captureTools(true, true); pick.miningSpeed = 9;
        check(!bridge.selectTool(tools, 4, true, true), "in-place item mutation changes score");
        tools = bridge.captureTools(true, true); stacks.set(4, new class_1799(new class_1792()));
        check(!bridge.selectTool(tools, 4, true, true), "inventory stack changed"); stacks.set(4, pick);
        tools = bridge.captureTools(true, true);
        check(!bridge.selectTool(tools, 4, true, false), "released left button");
        hit.miss = true; check(bridge.captureTools(false, false) == null, "BlockHitResult MISS rejected"); hit.miss = false;
        client.field_1687.state.air = true;
        check(bridge.captureTools(false, false) == null, "air block"); client.field_1687.state.air = false;
        pick.miningSpeed = Float.NaN;
        check(bridge.captureTools(false, false) == null, "nonfinite score"); pick.miningSpeed = 8;
        tools = bridge.captureTools(false, false); client.field_1687 = new class_638();
        check(!bridge.selectTool(tools, 4, false, false), "world invalidates slot decision");
        tools = bridge.captureTools(false, false); client.field_1761 = new class_636();
        check(!bridge.selectTool(tools, 4, false, false), "manager replacement");
        tools = bridge.captureTools(false, false);
        check(!bridge().selectTool(tools, 4, false, false), "foreign bridge owner");
        Object[] old = bridge.captureTools(false, false); tools = bridge.captureTools(false, false);
        check(!bridge.selectTool(old, 4, false, false), "new capture supersedes old");
        check(!bridge.selectTool(tools, 9, false, false), "slot bounds");

        p.mainHand = new class_1799(class_1802.field_8287);
        Object[] exp = bridge.captureExp();
        check(((double[]) exp[5])[2] == 1, "main-hand EXP recognized");
        check(bridge.useExp(exp, true) && client.useCount == 1 && client.field_1752 == 4, "one vanilla item-use request");
        check(!bridge.useExp(exp, true) && client.useCount == 1, "no duplicate EXP request");
        exp = bridge.captureExp();
        check(!bridge.useExp(exp, false) && client.useCount == 1, "right button released");
        exp = bridge.captureExp(); p.mainHand.heldItem = new class_1792();
        check(!bridge.useExp(exp, true) && client.useCount == 1, "EXP ran out / item changed in place");
        p.mainHand = new class_1799(null); p.offHand = new class_1799(class_1802.field_8287);
        exp = bridge.captureExp();
        check(((double[]) exp[5])[3] == 1 && bridge.useExp(exp, true) && client.useCount == 2, "offhand detection");
        exp = bridge.captureExp(); p.offHand = new class_1799(class_1802.field_8287);
        check(!bridge.useExp(exp, true), "stack replacement invalidates EXP");
        exp = bridge.captureExp(); client.field_1755 = new class_437();
        check(!bridge.useExp(exp, true) && bridge.captureTools(false, false) == null, "menu blocks both operations");
        client.field_1755 = null; exp = bridge.captureExp(); p.alive = false;
        check(!bridge.useExp(exp, true), "dead player"); p.alive = true;
        exp = bridge.captureExp(); client.field_1724 = new class_746();
        check(!bridge.useExp(exp, true), "player replaced"); client.field_1724 = p;
        Object[] held = bridge.captureExp(); tools = bridge.captureTools(false, false);
        Object[] heldTools = tools;
        Throwable[] error = {null};
        Thread worker = new Thread(() -> {
            try {
                check(bridge.captureExp() == null && bridge.captureTools(false, false) == null &&
                    !bridge.useExp(held, true) && !bridge.selectTool(heldTools, 4, false, false), "wrong thread rejected");
            } catch (Throwable failure) { error[0] = failure; }
        });
        worker.start(); worker.join(); if (error[0] != null) throw new AssertionError(error[0]);
        check(bridge.useExp(held, true), "worker cannot consume render-thread nonce");
        exp = bridge.captureExp(); client.failUse = true;
        int previousUses = client.useCount;
        try { bridge.useExp(exp, true); throw new AssertionError("Expected item-use error"); }
        catch (ReflectiveOperationException expected) { ++checks; }
        check(!bridge.useExp(exp, true) && client.useCount == previousUses + 1, "failed mutator cannot replay");
        ItemBridge isolated = new ItemBridge(new ClassLoader(ItemBridgeTest.class.getClassLoader()) {
            @Override protected Class<?> loadClass(String name, boolean resolve) throws ClassNotFoundException {
                if (name.equals("net.minecraft.class_3965")) throw new ClassNotFoundException(name);
                return super.loadClass(name, resolve);
            }
        }, 12111);
        try { isolated.prepare(1); throw new AssertionError("Missing mining binding accepted"); }
        catch (ReflectiveOperationException expected) { ++checks; }
        check(isolated.captureExp() != null, "missing mining binding isolated from EXP");
        exp = bridge.captureExp(); tools = bridge.captureTools(false, false); bridge.stop();
        check(bridge.captureExp() == null && !bridge.useExp(exp, true) && !bridge.selectTool(tools, 4, false, false),
            "shutdown rejects pending decisions");
        System.out.println("PASS item bridge: " + checks + " checks (" + args[0] + ")");
    }
}
