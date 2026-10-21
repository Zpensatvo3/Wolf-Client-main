package net.minecraft;

import java.lang.reflect.Field;
import net.onestep.hybrid.WorldBridge;

public final class WorldBridgeTest {
    private static int checks;
    private static void check(boolean value, String message) { ++checks; if (!value) throw new AssertionError(message); }
    private static WorldBridge bridge() throws Exception {
        // Exercise the exact constructor used by the native connector, including
        // the class-name coverage needed by the original two capabilities.
        String cpp = java.nio.file.Files.readString(java.nio.file.Path.of("src/services/game/world_bridge.cpp"));
        var matches = java.util.regex.Pattern.compile("\\{\"((?:net/minecraft/class_|field_|method_)[^\"]+)\", ").matcher(cpp);
        var names = new java.util.TreeSet<String>(); while (matches.find()) names.add(matches.group(1));
        var pairs = new java.util.ArrayList<String>(); for (String name : names) { pairs.add(name); pairs.add(name); }
        return new WorldBridge(WorldBridgeTest.class.getClassLoader(), 12111, pairs.toArray(String[]::new));
    }
    private static Field field(String name) throws Exception { Field f = class_1661.class.getDeclaredField(name); f.setAccessible(true); return f; }
    private static class_310 setup() throws Exception {
        class_310 c = new class_310(); class_310.field_1700 = c;
        c.field_1724.mainHand = new class_1799(new class_1747());
        ((class_2371) field("field_7547").get(c.field_1724.inventory)).set(3, new class_1799(class_1802.field_8634));
        return c;
    }
    private static int selected(class_310 c) throws Exception { return field("field_7545").getInt(c.field_1724.inventory); }
    private static int[] points() { return new int[24]; }
    public static void main(String[] args) throws Exception {
        class_310 c = setup(); WorldBridge b = bridge();
        Object[] p = b.captureBridge(); double[] data = (double[]) p[7];
        check(data[9] == 1 && data[11] == 1 && data[12] == 0, "bridge flags and original bounds");
        check(b.support(p, points()).length == 8, "eight native-chosen support reads");
        c.field_1687.state.air = true;
        check(b.support(p, points())[0] == 1, "support reflects air");
        check(b.forceSneak(p) && c.field_1690.field_1832.pressed, "force actual sneak binding");
        check(!b.forceSneak(p), "single-use sneak write");
        int[] input = b.sneakInput(); check(input[0] == 0 && input[1] == 340, "mapped input descriptor");
        check(b.releaseSneak(input[0], input[1], true) && c.field_1690.field_1832.pressed, "physical manual hold preserved");
        c.field_1690.field_1832.pressed = false;
        check(b.forceSneak(b.captureBridge()), "reacquire released sneak");
        check(b.releaseSneak(0, 340, false) && !c.field_1690.field_1832.pressed, "release own sneak only");
        c.field_1690.field_1832.pressed = true;
        check(b.forceSneak(b.captureBridge()) && b.sneakInput() == null, "preexisting manual sneak never claimed");
        c.field_1690.field_1832.pressed = false;
        p = b.captureBridge(); c.field_1724.motion = new class_243(.2, 0, 0);
        check(b.support(p, points()) == null && !b.forceSneak(p), "movement invalidates stale geometry");
        p = b.captureBridge(); c.field_1724.bounds.field_1323++;
        check(!b.forceSneak(p), "bounds rechecked");
        p = b.captureBridge(); c.field_1724.mainHand.components++;
        check(!b.forceSneak(p), "block stack components rechecked");
        p = b.captureBridge(); c.field_1724.ground = false;
        check(!b.forceSneak(p), "airborne write rejected"); c.field_1724.ground = true;
        c.field_1690.field_1832.failWrite = true;
        boolean failed = false;
        try { b.forceSneak(b.captureBridge()); } catch (ReflectiveOperationException expected) { failed = true; }
        check(failed && b.releaseSneak(0, 340, false) && !c.field_1690.field_1832.pressed, "partial sneak mutation remains cleanable");
        b.forceSneak(b.captureBridge());
        c.field_1690.field_1832.field_1655 = new class_3675$class_306(20, class_3675$class_307.field_1671);
        input = b.sneakInput(); check(input[0] == 1 && input[1] == 20, "rebound scancode descriptor");
        check(!b.releaseSneak(0, 340, false), "descriptor changed before cleanup");
        check(b.releaseSneak(1, 20, true) && c.field_1690.field_1832.pressed, "new manual binding preserved");

        c = setup(); b = bridge(); p = b.capturePearl();
        check(((double[]) p[6])[7] == 1, "pearl snapshot identifies hotbar slot");
        check(b.beginPearl(p, 3) && selected(c) == 3 && c.field_1761.syncCount == 1, "select and vanilla sync");
        check(!b.beginPearl(p, 3), "single pending pearl transaction");
        check(b.usePearl() && c.useCount == 1 && !b.usePearl(), "one vanilla use without hold/replay");
        check(b.releasePearl() && selected(c) == 0 && c.field_1761.syncCount == 2, "restore owned slot");
        check(b.beginPearl(b.capturePearl(), 3), "new cycle");
        field("field_7545").setInt(c.field_1724.inventory, 6);
        check(b.capturePearl() == null && !b.usePearl(), "manual slot change cancels pending use");
        check(b.releasePearl() && selected(c) == 6, "manual slot is not restored over");
        c = setup(); b = bridge(); p = b.capturePearl();
        class_2371 stacks = (class_2371) field("field_7547").get(c.field_1724.inventory);
        ((class_1799) stacks.get(3)).components++;
        check(!b.beginPearl(p, 3) && selected(c) == 0, "pearl metadata checked before selection");
        check(b.beginPearl(b.capturePearl(), 3), "begin before failure");
        c.failUse = true; failed = false;
        try { b.usePearl(); } catch (ReflectiveOperationException expected) { failed = true; }
        check(failed && c.useCount == 1 && !b.usePearl(), "exception after use cannot cause second throw");
        check(b.releasePearl() && selected(c) == 0, "failed use still releases owned selection");
        c = setup(); b = bridge(); b.beginPearl(b.capturePearl(), 3);
        c.field_1687 = new class_638();
        check(!b.usePearl() && b.releasePearl() && selected(c) == 3, "new world never receives old slot restore");
        p = b.capturePearl(); c.field_1755 = new class_437();
        check(!b.beginPearl(p, 3) && b.captureBridge() == null, "screen blocks new writes");
        c = setup(); b = bridge(); final WorldBridge threaded = b;
        final boolean[] rejected = {false};
        Thread worker = new Thread(() -> { try { rejected[0] = threaded.capturePearl() == null && threaded.captureBridge() == null; }
            catch (Exception error) { throw new AssertionError(error); } });
        worker.start(); worker.join(); check(rejected[0], "client thread only");
        b.stop(); check(b.capturePearl() == null && b.captureBridge() == null, "stop rejects future work");
        c = setup();
        WorldBridge isolated = new WorldBridge(new ClassLoader(WorldBridgeTest.class.getClassLoader()) {
            @Override protected Class<?> loadClass(String name, boolean resolve) throws ClassNotFoundException {
                if (name.equals("net.minecraft.class_238")) throw new ClassNotFoundException(name);
                return super.loadClass(name, resolve);
            }
        }, 12111);
        failed = false;
        try { isolated.prepare(1); } catch (ClassNotFoundException expected) { failed = true; }
        check(failed && isolated.capturePearl() != null, "edge mapping failure isolated from pearl");
        System.out.println("PASS world bridge: " + checks + " checks (" + args[0] + ")");
    }
}
