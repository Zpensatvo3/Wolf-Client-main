package net.minecraft;

import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.*;
import java.util.regex.Pattern;
import net.onestep.hybrid.WorldBridge;

/** Isolated game fixtures: never loads Minecraft, Fabric or the embedded class. */
public final class HitCrystalBridgeTest {
    private static int checks;
    private static void check(boolean value, String message) { ++checks; if (!value) throw new AssertionError(message); }
    private static Field field(String name) throws Exception { Field f = WorldBridge.class.getDeclaredField(name); f.setAccessible(true); return f; }
    private static void due(WorldBridge b) throws Exception { field("crystalNext").setLong(b, System.nanoTime() - 1); }
    private static class_310 setup() {
        class_310 c = new class_310(); class_310.field_1700 = c;
        c.field_1724.inventory.stacks[3] = new class_1799(class_1802.field_8301);
        c.field_1765 = new class_3965(new class_243(.5, 1, 3.5), class_2350.field_11036, new class_2338(0, 0, 3), false);
        return c;
    }
    private static WorldBridge bridge() throws Exception {
        WorldBridge b = new WorldBridge(HitCrystalBridgeTest.class.getClassLoader(), 12111); b.prepare(3); return b;
    }
    private static void tick(WorldBridge b, boolean held, boolean autoSwitch, boolean restore) throws Exception {
        int[] input = b.crystalInput();
        b.tickCrystal(true, held, input == null ? -1 : input[0], input == null ? -1 : input[1], held, autoSwitch, restore, 0, 0);
    }
    private static void step(WorldBridge b) throws Exception { due(b); tick(b, true, true, true); }
    private static String[] bindings() throws Exception {
        String cpp = Files.readString(Path.of("src/services/game/world_bridge.cpp"));
        var matches = Pattern.compile("\\{\"((?:net/minecraft/class_|field_|method_)[^\"]+)\", ").matcher(cpp);
        Set<String> names = new TreeSet<>(); while (matches.find()) names.add(matches.group(1));
        List<String> pairs = new ArrayList<>(); for (String name : names) { pairs.add(name); pairs.add(name); }
        return pairs.toArray(String[]::new);
    }
    public static void main(String[] args) throws Exception {
        class_310 c = setup(); WorldBridge b = bridge();
        tick(b, false, true, true); check(c.field_1724.inventory.field_7545 == 0, "requires physical right activation");
        c.field_1690.field_1904.pressed = true; c.field_1690.field_1904.field_1661 = 1;
        tick(b, true, true, true);
        check(c.field_1724.inventory.field_7545 == 3 && c.field_1761.syncCount == 1, "switch and vanilla slot sync");
        check(!c.field_1690.field_1904.pressed && c.field_1690.field_1904.field_1661 == 0, "no duplicate vanilla held/queued use");
        check(c.field_1761.places == 0, "switch and placement occur on separate frames");
        step(b); check(c.field_1761.places == 1 && c.field_1761.attacks == 0, "one placement without same-frame attack");
        step(b); check(c.field_1761.attacks == 1 && c.field_1687.entities.isEmpty(), "one vanilla crystal attack");
        step(b); check(c.field_1724.inventory.field_7545 == 0 && c.field_1761.syncCount == 2, "confirmed removal restores owned slot");
        check(!c.field_1690.field_1904.pressed, "use gate spans repeat delay");
        tick(b, false, true, true); check(c.field_1761.places == 1, "release stops repeat");

        c = setup(); b = bridge(); tick(b, true, false, true);
        check(c.field_1724.inventory.field_7545 == 0 && c.field_1761.places == 0, "auto-switch off respects held item");
        c.field_1724.inventory.stacks[0] = new class_1799(class_1802.field_8301);
        tick(b, true, false, true); step(b); check(c.field_1761.places == 1 && c.field_1761.syncCount == 0, "already held crystal needs no switch");

        c = setup(); b = bridge(); tick(b, true, true, false); step(b); step(b);
        due(b); tick(b, true, true, false); check(c.field_1724.inventory.field_7545 == 3, "restore setting retained");
        c = setup(); b = bridge(); tick(b, true, true, true); c.field_1724.inventory.field_7545 = 6; step(b);
        check(c.field_1724.inventory.field_7545 == 6 && c.field_1761.places == 0, "manual slot cancels and is never overwritten");
        c = setup(); b = bridge(); tick(b, true, true, true); c.field_1687 = new class_638(); step(b);
        check(c.field_1724.inventory.field_7545 == 3 && c.field_1761.places == 0, "old world cannot restore into new world");
        c = setup(); b = bridge(); tick(b, true, true, true); c.field_1755 = new class_437(); step(b);
        check(c.field_1761.places == 0 && c.field_1724.inventory.field_7545 == 0, "menu cancels pending placement");

        c = setup(); c.field_1687.base = class_2246.field_9987; b = bridge(); tick(b, true, true, true); step(b);
        check(c.field_1761.places == 1, "bedrock is a valid base");
        c = setup(); c.field_1687.base = new class_2248(); b = bridge(); tick(b, true, true, true);
        check(c.field_1761.syncCount == 0, "invalid base does not claim inventory");
        c = setup(); c.field_1687.aboveAir = false; b = bridge(); tick(b, true, true, true);
        check(c.field_1761.places == 0 && c.field_1761.syncCount == 0, "air above base required");
        c = setup(); c.field_1687.entities.add(new class_1297(new class_243(.5, 1, 3.5))); b = bridge(); tick(b, true, true, true);
        check(c.field_1761.syncCount == 0, "occupied crystal volume is rejected");
        c = setup(); c.field_1724.blockRange = 1; b = bridge(); tick(b, true, true, true);
        check(c.field_1761.syncCount == 0, "actual block reach is enforced");
        c = setup(); b = bridge(); tick(b, true, true, true); c.field_1687.base = new class_2248(); step(b);
        check(c.field_1761.places == 0 && c.field_1724.inventory.field_7545 == 0, "base rechecked after switch delay");

        c = setup(); class_1511 existing = new class_1511(21); c.field_1687.entities.add(existing);
        c.field_1765 = new class_3966(existing); b = bridge(); tick(b, true, true, true); step(b);
        check(c.field_1761.places == 0 && c.field_1761.attacks == 1, "crosshair crystal from vanilla can start breaking");
        c = setup(); c.field_1687.blockedRay = true; b = bridge(); tick(b, true, true, true); step(b); step(b);
        check(c.field_1761.attacks == 0 && c.field_1724.inventory.field_7545 == 0, "ray obstruction blocks attack");
        c = setup(); c.field_1724.entityRange = .5; b = bridge(); tick(b, true, true, true); step(b); step(b);
        check(c.field_1761.attacks == 0, "actual entity reach is enforced");
        c = setup(); c.field_1761.removeOnAttack = false; b = bridge(); tick(b, true, true, true); step(b); step(b);
        class_1297 target = c.field_1687.entities.get(0); c.field_1687.entities.clear();
        class_1511 replacement = new class_1511(target.id); c.field_1687.entities.add(replacement); step(b);
        check(c.field_1761.attacks == 1 && c.field_1761.places == 1, "reused ID cannot redirect attack or fake confirmation");
        c = setup(); c.field_1761.removeOnAttack = false; b = bridge(); tick(b, true, true, true); step(b); step(b);
        c.field_1687.entities.get(0).move(new class_243(30, 1, 30)); step(b);
        check(c.field_1761.attacks == 1 && c.field_1761.places == 1, "out-of-reach crystal is not treated as removed");

        c = setup(); c.field_1761.spawnOnPlace = false; b = bridge(); tick(b, true, true, true); step(b);
        for (int i = 0; i < 4; ++i) step(b);
        check(c.field_1761.places == 1, "server spawn confirmation awaited instead of spamming placement");
        field("crystalStarted").setLong(b, System.nanoTime() - 500_000_000L); step(b); step(b);
        check(c.field_1761.places == 2, "bounded retry follows spawn timeout");
        c = setup(); c.field_1761.removeOnAttack = false; b = bridge(); tick(b, true, true, true); step(b); step(b);
        for (int i = 0; i < 7; ++i) step(b);
        check(c.field_1761.attacks == 4, "at most three attack retries");
        c = setup(); c.field_1761.failPlace = true; b = bridge(); tick(b, true, true, true);
        boolean failed = false; try { step(b); } catch (ReflectiveOperationException expected) { failed = true; }
        tick(b, false, true, true); step(b);
        check(failed && c.field_1761.places == 1 && c.field_1724.inventory.field_7545 == 0, "partially sent placement is not replayed");
        c = setup(); c.field_1761.failAttack = true; b = bridge(); tick(b, true, true, true); step(b);
        failed = false; try { step(b); } catch (ReflectiveOperationException expected) { failed = true; }
        tick(b, false, true, true); step(b);
        check(failed && c.field_1761.attacks == 1, "partially sent attack is not replayed");

        c = setup(); b = bridge(); tick(b, true, true, true);
        check(b.cancelCrystal(2, 1, true) && c.field_1724.inventory.field_7545 == 0 && c.field_1690.field_1904.pressed,
            "disable restores owned slot and actual manual held use");
        c = setup(); b = bridge(); tick(b, true, true, true);
        c.field_1690.field_1904.field_1655 = new class_3675$class_306(70, class_3675$class_307.field_1668);
        step(b); check(!c.field_1690.field_1904.pressed && c.field_1761.places == 0, "rebound key is never restored as old right button");
        c = setup(); b = bridge(); final WorldBridge threaded = b;
        final Throwable[] workerError = {null};
        Thread worker = new Thread(() -> { try { tick(threaded, true, true, true); } catch (Throwable e) { workerError[0] = e; } });
        worker.start(); worker.join(); check(workerError[0] == null && c.field_1761.syncCount == 0, "wrong thread cannot start a cycle");
        b.stop(); tick(b, true, true, true); check(c.field_1761.places == 0, "stop prevents future interaction");
        for (int targetId : new int[]{12101, 12104, 12108, 12111, 26100, 26101, 26102}) {
            c = setup(); WorldBridge mapped = new WorldBridge(HitCrystalBridgeTest.class.getClassLoader(), targetId, bindings());
            mapped.prepare(3); tick(mapped, true, true, true); step(mapped);
            check(c.field_1761.places == 1, "explicit supplied binding protocol for " + targetId);
        }
        failed = false;
        try { new WorldBridge(HitCrystalBridgeTest.class.getClassLoader(), 26103, bindings()); }
        catch (IllegalArgumentException expected) { failed = true; }
        check(failed, "unknown version is rejected");
        System.out.println("PASS Hit Crystal Java controller: " + checks + " checks (source fixtures; no game compatibility claim)");
    }
}

class class_310 extends class_1255 {
    static class_310 field_1700; class_746 field_1724 = new class_746(); class_638 field_1687 = new class_638();
    class_437 field_1755; class_315 field_1690 = new class_315(); class_636 field_1761 = new class_636(); class_239 field_1765;
}
class class_1255 { final Thread owner = Thread.currentThread(); public boolean method_18854() { return Thread.currentThread() == owner; } }
class class_437 {}
class class_315 { class_304 field_1904 = new class_304(); }
class class_304 {
    class_3675$class_306 field_1655 = new class_3675$class_306(1, class_3675$class_307.field_1672);
    int field_1661; boolean pressed;
    public boolean method_1434() { return pressed; } public void method_23481(boolean p) { pressed = p; }
}
class class_3675$class_307 { static final class_3675$class_307 field_1668 = new class_3675$class_307(), field_1671 = new class_3675$class_307(), field_1672 = new class_3675$class_307(); }
class class_3675$class_306 {
    final int code; final class_3675$class_307 type;
    class_3675$class_306(int code, class_3675$class_307 type) { this.code = code; this.type = type; }
    public int method_1444() { return code; } public class_3675$class_307 method_1442() { return type; }
}
class class_1297 {
    int id; boolean alive = true; class_243 field_22467; class_238 box;
    class_1297(class_243 p) { move(p); }
    void move(class_243 p) { field_22467 = p; box = new class_238(p.field_1352 - .5, p.field_1351, p.field_1350 - .5, p.field_1352 + .5, p.field_1351 + 2, p.field_1350 + .5); }
    public boolean method_5805() { return alive; } public int method_5628() { return id; }
    public double method_23320() { return field_22467.field_1351 + 1.62; } public class_238 method_5829() { return box; }
}
class class_1657 extends class_1297 {
    double blockRange = 4.5, entityRange = 3;
    class_1657() { super(new class_243(.5, 0, 0)); }
    public double method_55754() { return blockRange; } public double method_55755() { return entityRange; }
}
class class_746 extends class_1657 {
    class_1661 inventory = new class_1661(); boolean using;
    public class_1661 method_31548() { return inventory; } public boolean method_6115() { return using; }
    public void method_6104(class_1268 hand) { if (hand != class_1268.field_5808) throw new AssertionError("hand"); }
}
class class_1511 extends class_1297 { class_1511(int id) { super(new class_243(.5, 1, 3.5)); this.id = id; } }
class class_1661 {
    int field_7545;
    class_1799[] stacks = new class_1799[9];
    class_1661() { Arrays.setAll(stacks, i -> new class_1799(null)); }
    public class_1799 method_5438(int slot) { return stacks[slot]; }
}
class class_1792 {}
class class_1799 { final class_1792 item; class_1799(class_1792 i) { item = i; } public boolean method_7960() { return item == null; } public class_1792 method_7909() { return item; } }
class class_1802 { static final class_1792 field_8301 = new class_1792(); }
class class_2248 {}
class class_2246 { static final class_2248 field_10540 = new class_2248(), field_9987 = new class_2248(); }
class class_2680 { final class_2248 block; final boolean air; class_2680(class_2248 b, boolean a) { block = b; air = a; } public boolean method_27852(class_2248 b) { return block == b; } public boolean method_26215() { return air; } }
class class_2338 { final int x, y, z; class_2338(int x, int y, int z) { this.x = x; this.y = y; this.z = z; } public int method_10263() { return x; } public int method_10264() { return y; } public int method_10260() { return z; } }
class class_243 { final double field_1352, field_1351, field_1350; class_243(double x, double y, double z) { field_1352 = x; field_1351 = y; field_1350 = z; } }
class class_238 {
    final double field_1323, field_1322, field_1321, field_1320, field_1325, field_1324;
    class_238(double x, double y, double z, double X, double Y, double Z) { field_1323 = x; field_1322 = y; field_1321 = z; field_1320 = X; field_1325 = Y; field_1324 = Z; }
    boolean overlaps(class_238 b) { return field_1320 > b.field_1323 && field_1323 < b.field_1320 && field_1325 > b.field_1322 && field_1322 < b.field_1325 && field_1324 > b.field_1321 && field_1321 < b.field_1324; }
}
class class_239$class_240 { static final class_239$class_240 field_1332 = new class_239$class_240(), MISS = new class_239$class_240(); }
class class_239 { public class_239$class_240 method_17783() { return class_239$class_240.MISS; } }
class class_3965 extends class_239 {
    final class_2338 pos; boolean miss;
    class_3965(class_243 v, class_2350 side, class_2338 p, boolean inside) { pos = p; }
    public class_2338 method_17777() { return pos; } @Override public class_239$class_240 method_17783() { return miss ? class_239$class_240.MISS : class_239$class_240.field_1332; }
}
class class_3966 extends class_239 { final class_1297 field_17592; class_3966(class_1297 e) { field_17592 = e; } }
class class_2350 { static final class_2350 field_11036 = new class_2350(); }
class class_1268 { static final class_1268 field_5808 = new class_1268(); }
class class_1269 { public boolean method_23665() { return true; } }
class class_3959$class_3960 { static final class_3959$class_3960 field_17558 = new class_3959$class_3960(); }
class class_3959$class_242 { static final class_3959$class_242 field_1348 = new class_3959$class_242(); }
class class_3959 { class_3959(class_243 from, class_243 to, class_3959$class_3960 shape, class_3959$class_242 fluid, class_1297 player) {} }
interface FixtureRaycast {
    // Interface default must resolve, just like Minecraft BlockView's raycast.
    default class_3965 method_17742(class_3959 ray) { class_3965 hit = new class_3965(null, null, null, false); hit.miss = !((class_638)this).blockedRay; return hit; }
}
class class_638 implements FixtureRaycast {
    final List<class_1297> entities = new ArrayList<>(); class_2248 base = class_2246.field_10540; boolean aboveAir = true, blockedRay;
    public class_2680 method_8320(class_2338 p) { return new class_2680(p.y == 0 ? base : null, p.y != 0 && aboveAir); }
    public class_1297 method_8469(int id) { for (class_1297 e : entities) if (e.id == id) return e; return null; }
    public List<class_1297> method_8335(class_1297 except, class_238 volume) { List<class_1297> result = new ArrayList<>(); for (class_1297 e : entities) if (e != except && volume.overlaps(e.box)) result.add(e); return result; }
}
class class_636 {
    int syncCount, places, attacks; boolean spawnOnPlace = true, removeOnAttack = true, failPlace, failAttack;
    private void method_2911() { ++syncCount; }
    public class_1269 method_2896(class_746 p, class_1268 hand, class_3965 hit) {
        ++places; if (spawnOnPlace) class_310.field_1700.field_1687.entities.add(new class_1511(100));
        if (failPlace) throw new IllegalStateException("after placement"); return new class_1269();
    }
    public void method_2918(class_1657 p, class_1297 e) {
        if (!(e instanceof class_1511)) throw new AssertionError("non-crystal attack");
        ++attacks; if (removeOnAttack) class_310.field_1700.field_1687.entities.remove(e);
        if (failAttack) throw new IllegalStateException("after attack");
    }
}
