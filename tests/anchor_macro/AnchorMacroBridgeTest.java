package net.minecraft;

import java.lang.reflect.Field;
import java.nio.file.*;
import java.util.*;
import java.util.regex.Pattern;
import org.lwjgl.glfw.GLFW;
import net.onestep.hybrid.WorldBridge;

/** Isolated game/input fixtures; never loads a game or sends OS events. */
public final class AnchorMacroBridgeTest {
    private static int checks;
    private static boolean[] flags;
    private static int[] values;
    private static final class Bridge extends WorldBridge {
        boolean allowed=true;
        Bridge() throws Exception { super(AnchorMacroBridgeTest.class.getClassLoader(),12111); }
        Bridge(int target,String[] bindings) throws Exception { super(AnchorMacroBridgeTest.class.getClassLoader(),target,bindings); }
        @Override protected boolean anchorPermitted(long generation) { return allowed; }
    }
    private static void check(boolean value,String message) { ++checks; if (!value) throw new AssertionError(message); }
    private static Field field(String key) throws Exception { Field f=WorldBridge.class.getDeclaredField(key); f.setAccessible(true); return f; }
    private static int stage(Bridge b) throws Exception { return field("anchorStage").getInt(b); }
    private static class_310 setup() {
        GLFW.reset(); GLFW.buttons[1]=1; class_304.field_1657.clear();
        flags=new boolean[]{true,false,false,false,false}; values=new int[]{100,0,100,0,100,0,100,1};
        class_310 c=new class_310(); class_310.field_1700=c;
        c.field_1724.inventory.stacks[0]=new class_1799(new class_1792());
        c.field_1724.inventory.stacks[3]=new class_1799(class_1802.field_23141);
        c.field_1724.inventory.stacks[4]=new class_1799(class_1802.field_8801);
        c.field_1687.blocks.put("0:0:3",new class_2680(class_2246.field_10540,false));
        aim(c,new class_2338(0,0,3),class_2350.field_11036); return c;
    }
    private static void aim(class_310 c,class_2338 pos,class_2350 side) { c.field_1765=new class_3965(null,side,pos,false); }
    private static void existing(class_310 c,int charge) {
        class_2680 state=new class_2680(class_2246.field_23152,false); state.charges=charge;
        c.field_1687.blocks.put("0:1:3",state); aim(c,new class_2338(0,1,3),class_2350.field_11043);
    }
    private static void tick(Bridge b) throws Exception { b.tickAnchor(true,1,flags,values); }
    private static void step(Bridge b) throws Exception { ++class_310.field_1700.field_1724.field_6012; tick(b); }
    private static void drive(Bridge b) throws Exception {
        for (int i=0;i<22;++i) {
            class_636 m=class_310.field_1700.field_1761; int before=m.places+m.charges+m.explosions;
            step(b); if (m.places+m.charges+m.explosions-before > 1) throw new AssertionError("multiple interactions in a tick");
        }
    }
    private static String[] bindings() throws Exception {
        var matches=Pattern.compile("\\{\"((?:net/minecraft/class_|field_|method_)[^\"]+)\", ").matcher(Files.readString(Path.of("src/services/game/world_bridge.cpp")));
        Set<String> names=new TreeSet<>(); while(matches.find()) names.add(matches.group(1));
        List<String> result=new ArrayList<>(); for(String name:names) { result.add(name); result.add(name); }
        return result.toArray(String[]::new);
    }
    public static void main(String[] args) throws Exception {
        class_310 c=setup(); Bridge b=new Bridge(); tick(b); drive(b);
        check(c.field_1761.places == 1 && c.field_1761.charges == 1 && c.field_1761.explosions == 1,"our complete place/charge/detonate sequence");
        check(c.field_1724.inventory.field_7545 == 0,"restores original slot after observed disappearance");
        check(!c.field_1690.field_1904.field_1653 && c.field_1690.field_1904.field_1661 == 0,"holds vanilla-use gate until physical release");
        drive(b); check(c.field_1761.places == 1,"one cycle per held-right activation");
        GLFW.buttons[1]=0; tick(b); check(!c.field_1690.field_1904.field_1653,"release leaves no stuck use binding");
        c=setup(); GLFW.buttons[1]=0; b=new Bridge(); tick(b); drive(b);
        check(c.field_1761.places == 0 && c.field_1761.syncCount == 0,"physical right is required");
        c=setup(); flags[4]=true; b=new Bridge(); tick(b); drive(b);
        check(c.field_1761.places == 1 && c.field_1761.charges == 1 && c.field_1761.explosions == 0,"Only Charge stops at observed charge");
        flags[4]=false; flags[3]=true; GLFW.buttons[1]=0; tick(b); GLFW.buttons[1]=1;
        aim(c,new class_2338(0,1,3),class_2350.field_11043); tick(b); drive(b);
        check(c.field_1761.explosions == 1,"Only Own accepts our previously confirmed anchor");
        c=setup(); flags[3]=true; existing(c,1); b=new Bridge(); tick(b); drive(b);
        check(c.field_1761.explosions == 0 && c.field_1761.syncCount <= 1,"Only Own rejects foreign anchors");
        c=setup(); existing(c,0); b=new Bridge(); tick(b); drive(b);
        check(c.field_1761.places == 0 && c.field_1761.charges == 1 && c.field_1761.explosions == 1,"existing uncharged anchor is supported");
        c=setup(); existing(c,1); b=new Bridge(); tick(b); drive(b);
        check(c.field_1761.charges == 0 && c.field_1761.explosions == 1,"existing charged anchor skips charging");
        c=setup(); existing(c,1); flags[4]=true; b=new Bridge(); tick(b); drive(b);
        check(c.field_1761.explosions == 0,"Only Charge never detonates an already charged anchor");
        c=setup(); existing(c,1); values[7]=5; b=new Bridge(); tick(b); drive(b);
        check(c.field_1761.explosions == 0,"glowstone explode slot cannot accidentally recharge");
        for(int gate:new int[]{0,2,4,6}) {
            c=setup(); values[gate]=0; b=new Bridge(); tick(b); drive(b);
            check(c.field_1761.explosions == 0,"zero chance stops stage "+gate);
            if(gate == 0 || gate == 2) check(c.field_1761.places == 0,"place/switch chance blocks initial action");
        }
        c=setup(); values[1]=3; b=new Bridge(); tick(b); step(b); step(b);
        check(c.field_1724.inventory.field_7545 == 0,"switch delay uses game ticks");
        step(b); check(c.field_1724.inventory.field_7545 == 3,"switch occurs at configured tick");
        tick(b); tick(b); check(c.field_1761.places == 0,"render frames never bypass one-mutation-per-tick guard");
        step(b); check(c.field_1761.places == 1,"placement follows switched game tick");
        c=setup(); existing(c,0); values[3]=3; b=new Bridge(); tick(b); step(b); step(b); step(b);
        check(c.field_1761.charges == 0,"Glowstone Delay waits configured ticks");
        drive(b); check(c.field_1761.charges == 1,"Glowstone Delay can complete");
        c=setup(); existing(c,1); values[5]=3; b=new Bridge(); tick(b); step(b); step(b); step(b);
        check(c.field_1761.explosions == 0,"Explode Delay waits configured ticks");
        drive(b); check(c.field_1761.explosions == 1,"Explode Delay can complete");
        c=setup(); flags[0]=false; c.field_1724.using=true; b=new Bridge(); tick(b); drive(b);
        check(c.field_1761.places == 0,"While Use off blocks during active item use");
        c=setup(); flags[0]=false; c.field_1724.offhand=new class_1799(new class_1792()); c.field_1724.offhand.food=true;
        b=new Bridge(); tick(b); drive(b); check(c.field_1761.places == 0,"While Use off respects offhand food before eating starts");
        c=setup(); flags[0]=false; c.field_1724.inventory.stacks[0]=new class_1799(new class_1819());
        b=new Bridge(); tick(b); drive(b); check(c.field_1761.places == 0,"While Use off respects held shield before blocking starts");
        c=setup(); c.field_1724.using=true; b=new Bridge(); tick(b); drive(b);
        check(c.field_1761.explosions == 1,"While Use on permits the sequence");
        c=setup(); flags[1]=true; class_1657 body=new class_1657(); body.alive=false;
        body.move(new class_243(.5,0,2)); c.field_1687.players.add(body); b=new Bridge(); tick(b); drive(b);
        check(c.field_1761.places == 0,"Stop on Kill stops for a dead player within six blocks");
        c=setup(); flags[1]=true; body=new class_1657(); body.alive=false; body.move(new class_243(.5,0,8));
        c.field_1687.players.add(body); b=new Bridge(); tick(b); drive(b);
        check(c.field_1761.explosions == 1,"distant dead player does not stop macro");
        c=setup(); b=new Bridge(); tick(b); step(b); c.field_1724.inventory.field_7545=6; step(b);
        check(c.field_1724.inventory.field_7545 == 6 && c.field_1761.places == 0,"manual slot change cancels without overwriting selection");
        c=setup(); b=new Bridge(); tick(b); step(b); c.field_1687=new class_638(); step(b);
        check(c.field_1724.inventory.field_7545 == 3 && c.field_1761.places == 0,"world change never restores into another session");
        c=setup(); b=new Bridge(); tick(b); step(b); c.field_1755=new class_437(); step(b);
        check(c.field_1724.inventory.field_7545 == 0 && c.field_1761.places == 0,"screen cancels pending use and restores owned slot");
        c=setup(); b=new Bridge(); tick(b); step(b); GLFW.focused=false; step(b);
        check(c.field_1761.places == 0 && !c.field_1690.field_1904.field_1653,"GLFW focus loss clears owned input");
        c=setup(); b=new Bridge(); tick(b); step(b); b.allowed=false; step(b);
        check(c.field_1761.places == 0 && c.field_1724.inventory.field_7545 == 0,"revoked runtime authority cancels slot/use ownership");
        c=setup(); b=new Bridge(); tick(b); step(b); flags[4]=true; step(b); drive(b);
        check(c.field_1761.places == 0,"config change under held right requires a new activation");
        c=setup(); c.field_1761.spawnOnPlace=false; b=new Bridge(); tick(b); step(b); step(b);
        field("anchorWaiting").setLong(b,System.nanoTime()-700_000_000L); step(b); drive(b);
        check(c.field_1761.places == 1 && c.field_1761.charges == 0,"unconfirmed placement times out without charging air or replay");
        c=setup(); c.field_1761.chargeOnUse=false; b=new Bridge(); tick(b);
        for(int i=0;i<8;++i) step(b);
        field("anchorWaiting").setLong(b,System.nanoTime()-700_000_000L); step(b); drive(b);
        check(c.field_1761.charges == 1 && c.field_1761.explosions == 0,"unknown charge never becomes permission to detonate");
        c=setup(); c.field_1687.blockedRay=true; b=new Bridge(); tick(b); drive(b);
        check(c.field_1761.places == 0,"ray obstruction rejects initial interaction");
        c=setup(); c.field_1724.blockRange=1; b=new Bridge(); tick(b); drive(b);
        check(c.field_1761.places == 0,"actual vanilla block range is respected");
        c=setup(); c.field_1724.inventory.stacks[3]=new class_1799(null); b=new Bridge(); tick(b); drive(b);
        check(c.field_1761.places == 0,"missing anchor item stops before placement");
        c=setup(); flags[2]=true; class_304 other=new class_304(); other.field_1653=true; other.field_1661=7;
        b=new Bridge(); tick(b); drive(b);
        check(c.field_1761.explosions == 1 && c.field_1729.presses == 3 && c.field_1729.releases == 3,"Click Simulation emits one Java right-mouse pulse per direct use");
        check(c.field_1690.field_1904.field_1661 == 0 && other.field_1661 == 7 && other.field_1653,"simulation preserves manual queues and adds no extra vanilla use");
        check(GLFW.buttons[1] == 1,"game click simulation never modifies physical GLFW state");
        c=setup(); flags[2]=true; c.field_1729.fail=true; b=new Bridge(); tick(b); step(b);
        boolean failed=false; try { step(b); } catch(ReflectiveOperationException expected) { failed=true; }
        c.field_1729.fail=false; drive(b);
        check(failed && c.field_1761.places == 1 && c.field_1729.releases == 1,"partial simulated press is released and not replayed");
        for(int failure:new int[]{0,1,2}) {
            c=setup(); c.field_1761.failPlace=failure == 0; c.field_1761.failCharge=failure == 1; c.field_1761.failExplode=failure == 2;
            b=new Bridge(); tick(b); failed=false;
            try { drive(b); } catch(ReflectiveOperationException expected) { failed=true; }
            c.field_1761.failPlace=c.field_1761.failCharge=c.field_1761.failExplode=false; drive(b);
            check(failed && c.field_1761.places == 1 && c.field_1761.charges <= 1 && c.field_1761.explosions <= 1,"partial game interaction suspends without replay stage "+failure);
        }
        c=setup(); b=new Bridge(); final Bridge threaded=b;
        Thread worker=new Thread(() -> { try { tick(threaded); } catch(Exception e) { throw new RuntimeException(e); } });
        worker.start(); worker.join(); check(c.field_1761.places == 0 && c.field_1761.syncCount == 0,"worker cannot begin game mutations");
        tick(b); step(b); b.tickAnchor(false,1,flags,values);
        check(c.field_1724.inventory.field_7545 == 0,"disable restores owned slot");
        b.stop(); drive(b); check(c.field_1761.places == 0,"shutdown cannot schedule later use");
        for(int target:new int[]{12101,12104,12108,12111,26100,26101,26102}) {
            c=setup(); b=new Bridge(target,bindings()); tick(b); drive(b);
            check(c.field_1761.explosions == 1,"exact supplied mapping protocol for "+target);
        }
        System.out.println("PASS Anchor Macro Java controller: "+checks+" checks ("+args[0]+"; isolated fixtures)");
    }
}
class class_310 extends class_1255 {
    static class_310 field_1700; class_746 field_1724 = new class_746(); class_638 field_1687 = new class_638();
    class_312 field_1729 = new class_312(); class_1041 window=new class_1041();
    public class_1041 method_22683() { return window; }
    class_437 field_1755; class_315 field_1690 = new class_315(); class_636 field_1761 = new class_636(); class_239 field_1765;
}
class class_1255 { final Thread owner = Thread.currentThread(); public boolean method_18854() { return Thread.currentThread() == owner; } }
class class_437 {}
class class_315 { class_304 field_1904 = new class_304(); }
class class_304 {
    static final Map<String,class_304> field_1657=new HashMap<>();
    class_3675$class_306 field_1655=new class_3675$class_306(1,class_3675$class_307.field_1672);
    int field_1661; boolean field_1653;
    class_304() { field_1657.put(UUID.randomUUID().toString(),this); }
    public boolean method_1434() { return field_1653; } public void method_23481(boolean p) { field_1653=p; }
}
class class_3675$class_307 { static final class_3675$class_307 field_1668 = new class_3675$class_307(), field_1671 = new class_3675$class_307(), field_1672 = new class_3675$class_307(); }
class class_3675$class_306 {
    final int code; final class_3675$class_307 type;
    class_3675$class_306(int code, class_3675$class_307 type) { this.code = code; this.type = type; }
    public int method_1444() { return code; } public class_3675$class_307 method_1442() { return type; }
}
class class_1297 {
    int id, field_6012=100; boolean alive = true; class_243 field_22467; class_238 box;
    class_1297(class_243 p) { move(p); }
    void move(class_243 p) { field_22467 = p; box = new class_238(p.field_1352 - .5, p.field_1351, p.field_1350 - .5, p.field_1352 + .5, p.field_1351 + 2, p.field_1350 + .5); }
    public boolean method_5805() { return alive; } public int method_5628() { return id; }
    public double method_23320() { return field_22467.field_1351 + 1.62; } public class_238 method_5829() { return box; }
}
class class_1309 extends class_1297 {
    class_1309(class_243 p) { super(p); } public boolean method_29504() { return !alive; }
}
class class_1657 extends class_1309 {
    double blockRange = 4.5, entityRange = 3;
    class_1657() { super(new class_243(.5, 0, 0)); }
    public double method_55754() { return blockRange; } public double method_55755() { return entityRange; }
}
class class_746 extends class_1657 {
    class_1661 inventory = new class_1661(); boolean using;
    class_1799 offhand=new class_1799(null);
    public class_1799 method_6047() { return inventory.stacks[inventory.field_7545]; }
    public class_1799 method_6079() { return offhand; }
    public class_1661 method_31548() { return inventory; } public boolean method_6115() { return using; }
    int swings;
    public void method_6104(class_1268 hand) { if (hand != class_1268.field_5808) throw new AssertionError("hand"); ++swings; }
}
class class_1511 extends class_1297 { class_1511(int id) { super(new class_243(.5, 1, 3.5)); this.id = id; } }
class class_1661 {
    int field_7545;
    class_1799[] stacks = new class_1799[9];
    class_1661() { Arrays.setAll(stacks, i -> new class_1799(null)); }
    public class_1799 method_5438(int slot) { return stacks[slot]; }
}
class class_9323 { final boolean food; class_9323(boolean f) { food=f; } public boolean method_57832(class_9331 type) { return food; } }
class class_1792 { boolean food; public class_9323 method_57347() { return new class_9323(food); } }
class class_9331 {} class class_9334 { static final class_9331 field_50075=new class_9331(); }
class class_1819 extends class_1792 {}
class class_1799 {
    boolean food;
    public Object method_57824(class_9331 component) { return food ? new Object() : null; }
    public Object method_58694(class_9331 component) { return method_57824(component); }
    final class_1792 item; class_1799(class_1792 i) { item = i; } public boolean method_7960() { return item == null; } public class_1792 method_7909() { return item; } }
class class_1802 { static final class_1792 field_8301=new class_1792(),field_23141=new class_1792(),field_8801=new class_1792(); }
class class_2248 {}
class class_2246 { static final class_2248 field_10540 = new class_2248(), field_9987 = new class_2248(), field_23152=new class_2248(); }
class class_2769 {} class class_2758 extends class_2769 {}
class class_4969 { static final class_2758 field_23153=new class_2758(); }
class class_2680 {
    final class_2248 block; final boolean air; int charges;
    class_2680(class_2248 b,boolean air) { block=b; this.air=air; }
    public boolean method_27852(class_2248 b) { return block == b; }
    public boolean method_27852(Object b) { return block == b; }
    public boolean method_26215() { return air; }
    public boolean method_45474() { return air; }
    public Comparable<?> method_11654(class_2769 property) { return charges; }
}
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
    final class_2338 pos; final class_2350 side; boolean miss;
    class_3965(class_243 v,class_2350 side,class_2338 p,boolean inside) { pos=p; this.side=side; }
    public class_2338 method_17777() { return pos; } public class_2350 method_17780() { return side; }
    @Override public class_239$class_240 method_17783() { return miss ? class_239$class_240.MISS : class_239$class_240.field_1332; }
}
class class_3966 extends class_239 { final class_1297 field_17592; class_3966(class_1297 e) { field_17592 = e; } }
class class_2350 {
    static final class_2350 field_11033=new class_2350(0,-1,0),field_11036=new class_2350(0,1,0),
        field_11043=new class_2350(0,0,-1),field_11035=new class_2350(0,0,1),field_11039=new class_2350(-1,0,0),field_11034=new class_2350(1,0,0);
    final int x,y,z; class_2350(int x,int y,int z) { this.x=x; this.y=y; this.z=z; }
    public int method_10148() { return x; } public int method_10164() { return y; } public int method_10165() { return z; }
}
class class_1268 { static final class_1268 field_5808 = new class_1268(); }
class class_1269 {
    boolean accepted=true,wantsSwing=true;
    public boolean method_23665() { return accepted; } public boolean method_23666() { return wantsSwing; }
}
class class_1269$class_9861 {
    static final class_1269$class_9861 field_52427=new class_1269$class_9861(),SERVER=new class_1269$class_9861();
}
class class_1269$class_9860 extends class_1269 {
    public class_1269$class_9861 comp_2909() { return wantsSwing ? class_1269$class_9861.field_52427 : class_1269$class_9861.SERVER; }
}
class class_3959$class_3960 { static final class_3959$class_3960 field_17558 = new class_3959$class_3960(); }
class class_3959$class_242 { static final class_3959$class_242 field_1348 = new class_3959$class_242(); }
class class_3959 {
    final class_243 end;
    class_3959(class_243 from,class_243 to,class_3959$class_3960 shape,class_3959$class_242 fluid,class_1297 player) { end=to; }
}
class class_638 {
    final Map<String,class_2680> blocks=new HashMap<>(); final List<class_1297> players=new ArrayList<>(); boolean blockedRay;
    static String key(class_2338 p) { return p.x+":"+p.y+":"+p.z; }
    public class_2680 method_8320(class_2338 p) { return blocks.getOrDefault(key(p),new class_2680(null,true)); }
    public List<class_1297> method_18456() { return players; }
    public class_3965 method_17742(class_3959 ray) {
        class_2338 pos=new class_2338((int)Math.floor(ray.end.field_1352),(int)Math.floor(ray.end.field_1351),(int)Math.floor(ray.end.field_1350));
        class_3965 hit=new class_3965(ray.end,class_2350.field_11036,pos,false);
        hit.miss=blockedRay || method_8320(pos).air; return hit;
    }
}
class class_636 {
    int syncCount,places,charges,explosions; boolean spawnOnPlace=true,chargeOnUse=true,removeOnExplode=true,failPlace,failCharge,failExplode,serverSwing;
    private void method_2911() { ++syncCount; }
    public class_1269 method_2896(class_746 p,class_1268 hand,class_3965 hit) {
        class_638 w=class_310.field_1700.field_1687; class_1792 item=p.inventory.stacks[p.inventory.field_7545].item;
        if (item == class_1802.field_23141) {
            ++places;
            class_2338 target=new class_2338(hit.pos.x+hit.side.x,hit.pos.y+hit.side.y,hit.pos.z+hit.side.z);
            if (spawnOnPlace) w.blocks.put(class_638.key(target),new class_2680(class_2246.field_23152,false));
            if (failPlace) throw new IllegalStateException("partial place");
        } else if (item == class_1802.field_8801) {
            ++charges; if (chargeOnUse) w.method_8320(hit.pos).charges++;
            if (failCharge) throw new IllegalStateException("partial charge");
        } else {
            if (w.method_8320(hit.pos).charges < 1) throw new AssertionError("detonation without observed charge");
            ++explosions; if (removeOnExplode) w.blocks.remove(class_638.key(hit.pos));
            if (failExplode) throw new IllegalStateException("partial explosion");
        }
        class_1269 result=new class_1269$class_9860(); result.wantsSwing=!serverSwing; return result;
    }
}
class class_1041 { long handle=17; public long method_4490() { return handle; } }
class class_11910 { final int button; class_11910(int b,int mods) { button=b; } }
class class_312 {
    boolean field_1791,field_1788,fail; int presses,releases;
    public void method_1601(long handle,class_11910 input,int action) { method_1601(handle,input.button,action,0); }
    public void method_1601(long handle,int button,int action,int mods) {
        if (button != 1) throw new AssertionError("Anchor simulated left click");
        if (action == 1) ++presses; else ++releases;
        field_1788=action == 1;
        for (class_304 key:class_304.field_1657.values()) if (key.field_1655.type == class_3675$class_307.field_1672 && key.field_1655.code == 1) {
            key.field_1653=action == 1; if (action == 1) key.field_1661++;
        }
        if (fail && action == 1) throw new IllegalStateException("partial click callback");
    }
}
