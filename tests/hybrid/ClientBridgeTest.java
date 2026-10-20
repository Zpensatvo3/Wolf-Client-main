package net.minecraft;

import java.util.Arrays;
import net.onestep.hybrid.ClientBridge;

public final class ClientBridgeTest {
    private static int checks;
    private static void check(boolean value, String message) { ++checks; if (!value) throw new AssertionError(message); }
    private static ClientBridge bridge() throws Exception { return new ClientBridge(ClientBridgeTest.class.getClassLoader(), 12111); }
    private static class_310 setup() {
        class_310 c = new class_310(); class_310.field_1700 = c; c.field_1755 = new class_490();
        class_304[] keys = {c.field_1690.field_1894, c.field_1690.field_1881, c.field_1690.field_1913,
            c.field_1690.field_1849, c.field_1690.field_1903, c.field_1690.field_1832};
        int[] codes = {87, 83, 65, 68, 32, 340};
        for (int i = 0; i < 6; ++i) keys[i].field_1655 = new class_3675$class_306(codes[i], class_3675$class_307.field_1668);
        return c;
    }
    private static boolean[] forward() { return new boolean[]{true, false, false, false, false, false}; }
    private static boolean release(ClientBridge b, boolean vanilla, boolean held) throws Exception {
        int[] descriptors = b.walkInputs(); if (descriptors == null) { descriptors = new int[12]; Arrays.fill(descriptors, -1); }
        return b.releaseWalk(vanilla, descriptors, new boolean[]{held, false, false, false, false, false});
    }
    private static double[] data(Object[] packet) { return (double[]) packet[7]; }
    public static void main(String[] args) throws Exception {
        class_310 c = setup(); ClientBridge b = bridge(); Object[] p = b.captureWalk();
        check(data(p)[3] == 1 && data(p)[4] == 0 && data(p)[6] == 87, "inventory kind and remappable forward key");
        check(b.applyWalk(p, forward()) && c.field_1690.field_1894.pressed, "owned forward press");
        check(!b.applyWalk(p, forward()), "single-use input decision");
        c.field_1755 = null;
        check(release(b, true, true) && c.field_1690.field_1894.pressed, "physical key handed back to vanilla");
        c.field_1690.field_1894.pressed = false; c.field_1755 = new class_490();
        b.applyWalk(b.captureWalk(), forward());
        check(release(b, false, true) && !c.field_1690.field_1894.pressed, "suspension releases even held owned key");
        c.field_1690.field_1894.pressed = true;
        b.applyWalk(b.captureWalk(), forward()); release(b, false, false);
        check(c.field_1690.field_1894.pressed && b.walkInputs() == null, "preexisting vanilla press not claimed");
        c.field_1690.field_1894.pressed = false;
        class_342 input = new class_342(); ((class_437) c.field_1755).children.add(input);
        check(data(b.captureWalk())[4] == 1 && !b.applyWalk(b.captureWalk(), forward()), "focused text blocks movement");
        input.visible = false; check(data(b.captureWalk())[4] == 0, "hidden text does not block");
        input.visible = true; input.active = false; check(data(b.captureWalk())[4] == 0, "inactive text does not block");
        input.active = true; input.focused = false; p = b.captureWalk(); input.focused = true;
        check(!b.applyWalk(p, forward()) && !c.field_1690.field_1894.pressed, "typing starts between capture and apply");
        c.field_1755 = new class_490();
        class_507 book = ((class_490) c.field_1755).field_54474; book.open = true; book.field_3089 = new class_342();
        check(data(b.captureWalk())[4] == 1, "recipe-book search pauses movement");
        class_481 creative = new class_481(); creative.field_2894 = new class_342(); c.field_1755 = creative;
        check(data(b.captureWalk())[3] == 1 && data(b.captureWalk())[4] == 1, "creative search pauses movement");
        c.field_1755 = new class_476(); check(data(b.captureWalk())[3] == 2, "container classified for native policy");
        c.field_1755 = new class_437(); check(data(b.captureWalk())[3] == 3, "other screen is not an inventory");
        c.field_1755 = null; check(data(b.captureWalk())[3] == 0, "gameplay classified for vanilla handoff");
        c.field_1755 = new class_490(); p = b.captureWalk();
        c.field_1690.field_1894.field_1655.code = 88;
        check(!b.applyWalk(p, forward()), "descriptor mutation rejected");
        c.field_1690.field_1903 = null;
        check(b.applyWalk(b.captureWalk(), forward()), "unbound optional jump does not disable WASD");
        c.field_1687 = new class_638();
        check(release(b, false, false) && !c.field_1690.field_1894.pressed, "same global key cleaned across world change");
        c.field_1690.field_1894.failWrite = true; boolean failed = false;
        try { b.applyWalk(b.captureWalk(), forward()); } catch (ReflectiveOperationException expected) { failed = true; }
        check(failed && release(b, false, false) && !c.field_1690.field_1894.pressed, "partial input write remains cleanable");
        class_437 root = new class_490(), current = root; c.field_1755 = root;
        for (int i = 0; i < 14; ++i) { class_437 next = new class_437(); current.children.add(next); current = next; }
        check(data(b.captureWalk())[4] == 1, "bounded deep widget traversal fails closed");

        c = setup(); b = bridge(); class_1657 target = new class_1657(), natural = new class_1657(), bot = new class_1657();
        natural.glowing = natural.glowingFlag = true;
        c.field_1687.entities.add(c.field_1724); c.field_1687.entities.add(target); c.field_1687.entities.add(natural);
        c.field_1687.entities.add(bot); c.field_1687.entities.add(new class_1297());
        p = b.captureGlow(); Object[] candidates = (Object[]) p[3];
        check(candidates.length == 3, "glow excludes self and nonplayers");
        boolean[] allowed = new boolean[candidates.length];
        for (int i = 0; i < candidates.length; ++i) allowed[i] = candidates[i] != bot;
        check(b.applyGlow(p, allowed) && target.glowing && target.glowingFlag && !bot.glowingFlag, "native AntiBot mask controls glow");
        check(!b.applyGlow(p, allowed), "single-use glow decision");
        check(b.releaseGlow() && !target.glowing && !target.glowingFlag && natural.glowing && natural.glowingFlag, "restore owned glow without wiping natural glow");
        p = b.captureGlow(); allowed = new boolean[((Object[]) p[3]).length]; Arrays.fill(allowed, true);
        b.applyGlow(p, allowed);
        p = b.captureGlow(); check(b.applyGlow(p, new boolean[((Object[]) p[3]).length]) && !target.glowingFlag, "new bot verdict releases previous own glow");
        p = b.captureGlow(); c.field_1687.entities.remove(target);
        allowed = new boolean[((Object[]) p[3]).length]; Arrays.fill(allowed, true);
        check(b.applyGlow(p, allowed) && !target.glowingFlag, "unloaded player not written through stale snapshot");
        p = b.captureGlow(); c.field_1687 = new class_638();
        check(!b.applyGlow(p, new boolean[((Object[]) p[3]).length]) && b.releaseGlow(), "world transition rejects old glow writes");
        c = setup(); b = bridge(); target = new class_1657(); target.failGlow = true; c.field_1687.entities.add(target);
        p = b.captureGlow(); failed = false;
        try { b.applyGlow(p, new boolean[]{true}); } catch (ReflectiveOperationException expected) { failed = true; }
        check(failed && !b.applyGlow(p, new boolean[]{true}) && b.releaseGlow() && !target.glowing, "partial glow mutation cleaned, packet cannot replay");
        final ClientBridge threaded = b; final boolean[] rejected = {false};
        Thread worker = new Thread(() -> { try { rejected[0] = threaded.captureWalk() == null && threaded.captureGlow() == null; }
            catch (Exception e) { throw new AssertionError(e); } });
        worker.start(); worker.join(); check(rejected[0], "client thread only");
        b.stop(); check(b.captureWalk() == null && b.captureGlow() == null, "stop rejects future work");
        c = setup();
        ClientBridge isolated = new ClientBridge(new ClassLoader(ClientBridgeTest.class.getClassLoader()) {
            @Override protected Class<?> loadClass(String name, boolean resolve) throws ClassNotFoundException {
                if (name.equals("net.minecraft.class_342")) throw new ClassNotFoundException(name);
                return super.loadClass(name, resolve);
            }
        }, 12111);
        failed = false;
        try { isolated.prepare(1); } catch (ClassNotFoundException expected) { failed = true; }
        check(failed && isolated.captureGlow() != null, "walk binding failure isolated from glow");
        System.out.println("PASS client bridge: " + checks + " checks (" + args[0] + ")");
    }
}
