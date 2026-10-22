package net.minecraft;

import java.lang.reflect.*;
import java.util.*;
import java.nio.file.*;
import net.onestep.hybrid.WorldBridge;
import net.fabricmc.fabric.api.client.event.lifecycle.v1.ClientTickEvents;

/** Exercise the live, damage-based controller with the actual native bindings. */
public final class CrystalAuraBridgeTest {
    private static int checks;
    private static String[] nativeBindings;
    private static class Bridge extends WorldBridge {
        boolean allowed=true,triggerOwns,filter=true,revokeAtAction;
        Object friend;
        long generation=1;int actions;
        int[] config={0,50,500,100,100,6000,4500,3000,2000,12000,31};
        Bridge() throws Exception {this(CrystalAuraBridgeTest.class.getClassLoader());}
        Bridge(ClassLoader loader) throws Exception {super(loader,12111,nativeBindings);}
        @Override protected long auraService(int op,Object e,int id,long gen,long epoch) {
            if(op==2) return 1;
            if(!allowed || gen!=generation) return 0;
            if(op==0) return filter && e!=friend?1:0;
            if(op==1) return triggerOwns?0:1;
            if(op==4) return 1;
            if(op==5) return e==friend?1:0;
            if(op==3) {
                ++actions;if(revokeAtAction) allowed=false;
                try {return (boolean)method("executeAuraAction").invoke(this)?1:0;}
                catch(ReflectiveOperationException error) {throw new IllegalStateException(error);}
            }
            return 0;
        }
    }
    private static Field field(String name) throws Exception {Field f=WorldBridge.class.getDeclaredField(name);f.setAccessible(true);return f;}
    private static Method method(String name,Class<?>... args) throws ReflectiveOperationException {Method m=WorldBridge.class.getDeclaredMethod(name,args);m.setAccessible(true);return m;}
    private static void check(boolean value,String why) {++checks;if(!value) throw new AssertionError(why);}
    private static class_310 setup(boolean existingBase) {
        ClientTickEvents.START_CLIENT_TICK.clear();
        class_310 c=new class_310();class_310.field_1700=c;
        c.field_1724.inventory.stacks[3]=new class_1799(class_1802.field_8281);
        c.field_1724.inventory.stacks[4]=new class_1799(class_1802.field_8301);
        class_1657 target=new class_1657();target.id=7;target.move(new class_243(.5,0,2.5));
        c.field_1687.entities.add(target);
        if(existingBase) c.field_1687.blocks.put("0:0:1",new class_2680(class_2246.field_10540));
        return c;
    }
    private static void publish(Bridge b) throws Exception {b.tickAura(true,b.generation,b.config);}
    private static void step(Bridge b) throws Exception {
        field("auraPlaceReady").setLong(b,0);field("auraBreakReady").setLong(b,0);field("auraSwitchReady").setLong(b,0);
        ++class_310.field_1700.field_1724.field_6012;publish(b);ClientTickEvents.fire(class_310.field_1700);
    }
    private static void drive(Bridge b,int ticks) throws Exception {for(int i=0;i<ticks;i++) step(b);}
    private static String expectedFailure(Bridge b) throws Exception {
        var output=new java.io.ByteArrayOutputStream();var previous=System.err;
        try {System.setErr(new java.io.PrintStream(output));drive(b,5);}
        finally {System.setErr(previous);}
        return output.toString(java.nio.charset.StandardCharsets.UTF_8);
    }
    private static void until(Bridge b,java.util.function.BooleanSupplier done) throws Exception {
        for(int i=0;i<35 && !done.getAsBoolean();i++) step(b);
        check(done.getAsBoolean(),"controller reaches expected action; phase="+field("auraPhase").getInt(b)+" status="+field("auraStatus").getInt(b));
    }
    private static class_1511 existing(class_310 c) {
        class_1511 crystal=new class_1511(90,new class_243(.5,1,1.5));c.field_1687.entities.add(crystal);return crystal;
    }
    private static double damage(Bridge b,Object e,double[] explosion) throws Exception {
        ((Map<?,?>)field("auraDefenses").get(b)).clear();
        return (double)method("auraDamage",Object.class,double[].class,int[].class).invoke(b,e,explosion,null);
    }
    public static void main(String[] args) throws Exception {
        nativeBindings=Files.readAllLines(Path.of(args[1])).toArray(String[]::new);
        String[] duplicate=Arrays.copyOf(nativeBindings,nativeBindings.length+2);
        duplicate[nativeBindings.length]="method_6032";duplicate[nativeBindings.length+1]="method_6032";
        boolean rejected=false;try {new WorldBridge(CrystalAuraBridgeTest.class.getClassLoader(),12111,duplicate);} catch(IllegalArgumentException expected) {rejected=true;}
        check(rejected,"real constructor rejects duplicate bindings instead of test-side deduplication");

        class_310 c=setup(true);Bridge b=new Bridge();publish(b);for(int i=0;i<20;i++) publish(b);
        check(c.field_1761.places==0 && c.field_1761.attacks==0,"render calls only publish while tick hook is present");
        final class_310 first=c;until(b,()->first.field_1761.crystals==1);
        check(c.field_1761.bases==0,"existing base needs neither sword nor an obsidian item");
        check(c.field_1687.entities.get(0).ground,"grounded player does not block Crystal Aura");
        until(b,()->first.field_1761.attacks==1);check(c.field_1724.swings==2,"one placement and one crystal attack use vanilla swing");
        b.cancelAura();check(c.field_1724.inventory.field_7545==0,"disable restores original owned slot");

        c=setup(false);b=new Bridge();final class_310 support=c;until(b,()->support.field_1761.crystals==1);
        check(c.field_1761.bases==1,"optional support places obsidian and confirms it before crystal");
        c=setup(false);b=new Bridge();b.config[10]&=~16;drive(b,5);check(c.field_1761.places==0,"support off never places obsidian");
        c=setup(true);c.field_1724.inventory.stacks[3]=new class_1799(null);b=new Bridge();final class_310 noObsidian=c;
        until(b,()->noObsidian.field_1761.crystals==1);check(c.field_1761.bases==0,"no obsidian required when base already exists");
        c=setup(true);b=new Bridge();b.config[10]&=~16;c.field_1687.blocks.put("0:2:1",new class_2680(class_2246.SUPPORT));final class_310 ceiling=c;
        until(b,()->ceiling.field_1761.crystals==1);check(c.field_1761.crystals==1,"modern placement does not require legacy second air block");
        c=setup(true);c.field_1724.inventory.stacks[4]=new class_1799(null);c.field_1724.offhand=new class_1799(class_1802.field_8301);b=new Bridge();final class_310 offhand=c;
        until(b,()->offhand.field_1761.crystals==1);check(c.field_1761.syncCount==0 && c.field_1761.lastHand==class_1268.field_5810,"offhand crystal needs no hotbar swap");

        c=setup(true);existing(c);for(int i=0;i<9;i++) c.field_1724.inventory.stacks[i]=new class_1799(null);b=new Bridge();final class_310 breaking=c;
        until(b,()->breaking.field_1761.attacks==1);check(c.field_1761.places==0,"can break existing crystals without any placement materials");
        c=setup(true);existing(c);b=new Bridge();b.config[10]&=~2;drive(b,6);check(c.field_1761.attacks==0,"Break Crystals toggle respected");
        c=setup(true);b=new Bridge();b.config[10]&=~1;drive(b,6);check(c.field_1761.places==0,"Place Crystals toggle respected");
        c=setup(true);b=new Bridge();b.config[10]&=~4;drive(b,5);check(c.field_1761.places==0 && c.field_1761.syncCount==0,"Auto Switch off respects selected item");
        c=setup(true);b=new Bridge();b.config[10]&=~8;final class_310 noRestore=c;until(b,()->noRestore.field_1761.crystals==1);b.cancelAura();
        check(c.field_1724.inventory.field_7545==4,"Restore Slot off keeps selected crystal slot");

        c=setup(true);b=new Bridge();step(b);c.field_1724.inventory.field_7545=8;step(b);step(b);
        check(c.field_1761.places==0 && c.field_1724.inventory.field_7545==8,"manual slot cancels without restoring over user");
        c=setup(true);b=new Bridge();step(b);c.field_1755=new class_437();step(b);
        check(c.field_1761.places==0 && c.field_1724.inventory.field_7545==0,"menu cancels slot preparation");
        c=setup(true);b=new Bridge();step(b);c.field_1687=new class_638();step(b);
        check(c.field_1761.places==0 && c.field_1724.inventory.field_7545==4,"world replacement drops old slot ownership");
        c=setup(true);b=new Bridge();b.allowed=false;drive(b,4);check(c.field_1761.places==0,"permission guard blocks actions");
        c=setup(true);b=new Bridge();b.revokeAtAction=true;drive(b,4);check(b.actions==1 && c.field_1761.places==0,"final permission recheck occurs under action lock");
        c=setup(true);b=new Bridge();step(b);field("auraConfigTime").setLong(b,System.nanoTime()-600_000_000L);ClientTickEvents.fire(c);
        check(!field("auraActive").getBoolean(b) && c.field_1724.inventory.field_7545==0,"stale publishing cancels active tick ownership");
        c=setup(true);b=new Bridge();b.triggerOwns=true;drive(b,4);check(c.field_1761.syncCount==0,"Triggerbot ownership wins");
        c=setup(true);b=new Bridge();b.filter=false;drive(b,4);check(c.field_1761.places==0,"friends/bots excluded from targets");
        c=setup(true);b=new Bridge();c.field_1724.using=true;drive(b,4);check(c.field_1761.places==0,"item use pauses aura");
        for(int mode=0;mode<3;mode++) {
            c=setup(true);b=new Bridge();b.config[0]=mode;b.filter=false;
            class_1308 mob=new class_1308();mob.id=8;mob.move(new class_243(.5,0,2.5));c.field_1687.entities.clear();c.field_1687.entities.add(mob);
            drive(b,6);check((c.field_1761.places>0)==(mode!=0),"player/mob targeting mode "+mode);
        }
        c=setup(true);b=new Bridge();b.config[0]=2;b.config[10]&=~16;
        class_1308 movingMob=new class_1308();movingMob.id=8;
        c.field_1687.entities.clear();c.field_1687.entities.add(movingMob);
        c.field_1687.blocks.put("1:0:0",new class_2680(class_2246.field_10540));
        for(int i=0;i<32;i++) {
            movingMob.move(i%2==0?new class_243(.5,0,2.5):new class_243(2.5,0,.5));
            step(b);
        }
        check(c.field_1761.crystals>=3 && c.field_1761.attacks>=3,
            "moving mob keeps the crystal loop running when the highest-scoring base changes each tick; placed="+
            c.field_1761.crystals+" broken="+c.field_1761.attacks+" phase="+field("auraPhase").getInt(b));
        c=setup(true);b=new Bridge();b.config[0]=2;b.config[10]&=~16;
        movingMob=new class_1308();movingMob.id=8;
        c.field_1687.entities.clear();c.field_1687.entities.add(movingMob);
        c.field_1687.blocks.put("1:0:0",new class_2680(class_2246.field_10540));
        for(int i=0;i<48;i++) {
            movingMob.move(i%2==0?new class_243(.5,0,2.5):new class_243(2.5,0,.5));
            // Keep actual place/break/slot deadlines, unlike the fast fixture steps.
            Thread.sleep(52);++c.field_1724.field_6012;publish(b);ClientTickEvents.fire(c);
        }
        check(c.field_1761.crystals>=3 && c.field_1761.attacks>=3,
            "moving mob repeats placement and break with real timers, without forcing deadlines");
        c=setup(true);b=new Bridge();b.config[10]&=~16;
        c.field_1687.blocks.put("1:0:0",new class_2680(class_2246.field_10540));step(b);
        int[] pending=((int[])field("auraAimBase").get(b)).clone();
        c.field_1687.blocks.put(pending[0]+":"+(pending[1]+1)+":"+pending[2],new class_2680(class_2246.SUPPORT));
        final class_310 invalidSite=c;until(b,()->invalidSite.field_1761.crystals==1);
        check(c.field_1761.lastBase.x!=pending[0] || c.field_1761.lastBase.y!=pending[1] || c.field_1761.lastBase.z!=pending[2],
            "pending placement is discarded when its space becomes blocked");
        c=setup(true);b=new Bridge();b.config[10]=2;
        existing(c);class_1511 secondCrystal=new class_1511(91,new class_243(1.5,1,.5));
        c.field_1687.entities.add(secondCrystal);
        for(int i=0;i<8 && c.field_1761.attacks==0;i++) {
            c.field_1687.entities.get(0).move(i%2==0?new class_243(.5,0,2.5):new class_243(2.5,0,.5));step(b);
        }
        check(c.field_1761.attacks>0,"target movement cannot continually replace the pending crystal attack");
        c=setup(true);b=new Bridge();b.config[10]=(b.config[10]|32)&~16;b.config[9]=0;c.field_1724.abilities.field_7480=true;step(b);
        c.field_1724.abilities.field_7480=false;drive(b,4);
        check(c.field_1761.places==0 && field("auraStatus").getInt(b)==7,
            "pending placement still rechecks self damage before committing");
        check(field("auraSiteEvaluations").getInt(b)<=16,"rejected damage candidates also count toward the per-tick planning budget");
        c=setup(true);b=new Bridge();c.field_1687.entities.get(0).spectator=true;drive(b,4);check(c.field_1761.places==0,"spectator target rejected");
        c=setup(true);b=new Bridge();((class_1309)c.field_1687.entities.get(0)).health=Float.NaN;drive(b,4);check(c.field_1761.places==0,"nonfinite target health rejected");
        c=setup(true);b=new Bridge();c.field_1724.blockRange=.5;drive(b,4);check(c.field_1761.places==0,"actual block reach caps configured range");
        c=setup(true);existing(c);b=new Bridge();c.field_1724.entityRange=.1;b.config[10]=2;drive(b,4);check(c.field_1761.attacks==0,"actual entity reach caps break range");
        c=setup(true);b=new Bridge();c.field_1687.blocked=true;drive(b,4);check(c.field_1761.places==0,"wall prevents placement");

        c=setup(true);b=new Bridge();b.config[10]=(b.config[10]|32)&~16;b.config[9]=0;drive(b,4);check(c.field_1761.places==0 && field("auraStatus").getInt(b)==7,"self damage limit blocks unsafe actions");
        c=setup(true);b=new Bridge();b.config[10]|=32;c.field_1724.abilities.field_7480=true;final class_310 creative=c;
        until(b,()->creative.field_1761.crystals==1);check(c.field_1761.crystals==1,"creative invulnerability permits testing");
        c=setup(true);b=new Bridge();b.config[10]|=32;c.field_1724.abilities.field_7480=true;
        b.config[10]&=~16;
        class_1657 friend=new class_1657();friend.id=10;friend.move(new class_243(1.5,0,2.5));c.field_1687.entities.add(friend);b.friend=friend;
        drive(b,4);check(c.field_1761.places==0,"nearby friends receive self-damage/anti-suicide protection");

        c=setup(true);b=new Bridge();step(b);double[] blast={.5,1,2};double raw=damage(b,c.field_1724,blast);
        c.field_1724.armor=20;c.field_1724.toughness=8;double armor=damage(b,c.field_1724,blast);
        check(raw>0 && armor>0 && armor<raw,"damage estimate includes armor and toughness");
        c.field_1724.effects.put(class_1294.field_5907,new class_1293(0));double resistance=damage(b,c.field_1724,blast);
        check(resistance<armor,"resistance reduces damage estimate");
        class_1799 boots=new class_1799(new class_1792());boots.enchantments.entries.put(new class_6880("minecraft:blast_protection"),4);
        c.field_1724.equipment.put(class_1304.field_6166,boots);double enchanted=damage(b,c.field_1724,blast);
        check(enchanted<resistance,"blast protection read from actual item component contract");
        c.field_1687.blocked=true;check(damage(b,c.field_1724,blast)==0,"fully blocked exposure yields zero damage");

        c=setup(true);b=new Bridge();c.field_1761.spawn=false;final class_310 wait=c;until(b,()->wait.field_1761.crystals==1);
        drive(b,4);check(c.field_1761.crystals==1,"no duplicate placement while awaiting server spawn");
        field("auraDeadline").setLong(b,0);step(b);check(field("auraStatus").getInt(b)==10,"spawn timeout reports a reason");
        c=setup(true);existing(c);b=new Bridge();b.config[10]=2;c.field_1761.remove=false;final class_310 retry=c;
        until(b,()->retry.field_1761.attacks==1);
        for(int i=0;i<16;i++) {field("auraDeadline").setLong(b,0);step(b);}
        check(c.field_1761.attacks==3,"unconfirmed crystal break has three-attempt limit");
        c=setup(true);existing(c);b=new Bridge();c.field_1724.effects.put(class_1294.field_5911,new class_1293(0));b.config[10]=2;drive(b,4);
        check(c.field_1761.attacks==0 && field("auraStatus").getInt(b)==12,"weakness requires an effective weapon");
        c=setup(true);existing(c);b=new Bridge();c.field_1724.effects.put(class_1294.field_5911,new class_1293(0));
        c.field_1724.inventory.stacks[2]=new class_1799(new class_1792(true));final class_310 weakness=c;until(b,()->weakness.field_1761.attacks==1);
        check(c.field_1724.inventory.field_7545==2,"weakness switches to a sword without using Triggerbot controller");
        c=setup(true);existing(c);b=new Bridge();c.field_1761.failAttack=true;expectedFailure(b);
        check(c.field_1761.attacks==1 && field("auraFailed").getBoolean(b),"partially sent attack suspends without replay");
        c=setup(true);b=new Bridge();c.field_1761.failPlace=true;String failure=expectedFailure(b);
        check(c.field_1761.places==1 && field("auraFailed").getBoolean(b),"partially sent placement suspends without replay");
        check(failure.contains("Crystal Aura Java controller suspended") && failure.contains("after place"),"real failure produces a useful game log instead of silently disabling");
        c=setup(true);b=new Bridge();step(b);int switched=c.field_1761.syncCount;
        field("auraPlaceReady").setLong(b,System.nanoTime()+10_000_000_000L);++c.field_1724.field_6012;publish(b);ClientTickEvents.fire(c);
        check(c.field_1761.places==0 && c.field_1761.syncCount==switched,"independent placement timer gates action");
        b.stop();ClientTickEvents.fire(c);check(c.field_1724.inventory.field_7545==0,"stop restores owned slot and disables tick handler");

        c=setup(true);b=new Bridge(new ClassLoader(CrystalAuraBridgeTest.class.getClassLoader()) {
            @Override protected Class<?> loadClass(String name,boolean resolve) throws ClassNotFoundException {
                if(name.startsWith("net.fabricmc.fabric.api.")) throw new ClassNotFoundException(name);
                return super.loadClass(name,resolve);
            }
        });
        final class_310 fallback=c;until(b,()->fallback.field_1761.crystals==1);
        check(!field("clientTickHooked").getBoolean(b),"same Java logic works when Fabric API is absent");
        int sent=c.field_1761.places;for(int i=0;i<12;i++) publish(b);check(c.field_1761.places==sent,"fallback never repeats an action in the same player tick");
        c=setup(true);b=new Bridge();final Bridge workerBridge=b;
        Thread worker=new Thread(()->{try {publish(workerBridge);} catch(Exception error) {throw new RuntimeException(error);}});worker.start();worker.join();
        check(c.field_1761.syncCount==0,"worker thread cannot begin game work");
        step(b);final Bridge stoppedBridge=b;
        Thread shutdown=new Thread(stoppedBridge::stop);shutdown.start();shutdown.join();
        check(((List<?>)field("auraScene").get(b)).isEmpty() && field("auraPlayer").get(b)==null,
            "off-thread shutdown clears retained scene without mutating the game");
        System.out.println("PASS redesigned Crystal Aura: "+checks+" checks ("+args[0]+"; isolated game fixtures)");
    }
}

class class_310 {
    static class_310 field_1700; class_746 field_1724=new class_746(); class_638 field_1687=new class_638();
    class_437 field_1755; class_315 field_1690=new class_315(); class_636 field_1761=new class_636(); class_239 field_1765;
    class_1297 pick; class_2338 pickBlock; final Thread thread=Thread.currentThread();
    public boolean method_18854() { return Thread.currentThread()==thread; }
    public class_1041 method_22683() { return new class_1041(); } public class_1297 method_1560() { return field_1724; }
}
class class_1041 { public long method_4490() { return 1; } } class class_437 {}
class class_1297 {
    int id,field_6012=100; float field_6031,field_5965; double field_6017; boolean alive=true,ground=true,spectator;
    class_243 field_22467=new class_243(.5,0,0); class_238 box;
    class_1297() { move(field_22467); }
    void move(class_243 p) { field_22467=p; box=new class_238(p.field_1352-.5,p.field_1351,p.field_1350-.5,p.field_1352+.5,p.field_1351+2,p.field_1350+.5); }
    public boolean method_5805() { return alive; } public boolean method_7325() { return spectator; } public int method_5628() { return id; }
    public boolean method_5767() { return false; } public boolean method_24828() { return ground; }
    public boolean method_5799() { return false; } public boolean method_5681() { return false; }
    public boolean method_5765() { return false; } public boolean method_5624() { return false; }
    public class_238 method_5829() { return box; } public double method_23320() { return field_22467.field_1351+1.62; }
    public class_239 method_5745(double reach,float tick,boolean fluids) { return new class_3965(null,class_2350.field_11036,class_310.field_1700.pickBlock,false); }
}
class class_1309 extends class_1297 {
    public class_1799 method_6079() { return new class_1799(null); }
    int field_6273; float health=20; int armor; double toughness; float absorption;
    Map<class_6880,class_1293> effects=new HashMap<>(); Map<class_1304,class_1799> equipment=new HashMap<>();
    public int method_6096() { return armor; } public double method_45325(class_6880 attribute) { return toughness; }
    public float method_6067() { return absorption; } public class_1293 method_6112(class_6880 effect) { return effects.get(effect); }
    public class_1799 method_6118(class_1304 slot) { return equipment.getOrDefault(slot,new class_1799(null)); } public float method_6032() { return health; }
    public boolean method_6128() { return false; } public boolean method_6101() { return false; }
    public boolean method_6059(class_6880 effect) { return false; } public boolean method_6039() { return false; }
}
class class_1308 extends class_1309 {} class class_1429 extends class_1308 {}
class class_1657 extends class_1309 { class_1656 abilities=new class_1656(); public class_1656 method_31549() { return abilities; } double entityRange=3,blockRange=4.5; public double method_55754() { return blockRange; } public double method_55755() { return entityRange; } }
class class_746 extends class_1657 {
    class_1661 inventory=new class_1661(); class_1799 offhand=new class_1799(null); public class_1799 method_6079() { return offhand; } boolean using; float cooldown=1; int swings;
    public class_1661 method_31548() { return inventory; } public boolean method_6115() { return using; }
    public float method_7261(float partial) { return cooldown; } public void method_7350() { ++field_6273; }
    public void method_6104(class_1268 hand) { ++swings; }
    public static class_239 method_76763(class_1297 camera,double blocks,double entities,float partial) { return class_310.field_1700.pick==null?new class_239():new class_3966(class_310.field_1700.pick); }
}
class class_1511 extends class_1297 { class_1511(int id,class_243 pos) { this.id=id; move(pos); } }
class class_1661 { int field_7545; class_1799[] stacks=new class_1799[9]; class_1661() { Arrays.setAll(stacks,i->new class_1799(null)); } public class_1799 method_5438(int i) { return stacks[i]; } }
class class_1792 { final boolean sword; class_1792() { this(false); } class_1792(boolean sword) { this.sword=sword; } public class_6880$class_6883 method_40131() { return new class_6880$class_6883(sword); } }
class class_1743 extends class_1792 {}
class class_1799 { class_9304 enchantments=new class_9304(); public class_9304 method_58657() { return enchantments; } final class_1792 item; class_1799(class_1792 item) { this.item=item; } public boolean method_7960() { return item==null; } public class_1792 method_7909() { return item; } }
class class_1802 { static final class_1792 field_8281=new class_1792(),field_8301=new class_1792(); }
class class_6880 { String id; class_6880() {} class_6880(String name) { id=name; } public String method_55840() { return id; } } class class_6880$class_6883 extends class_6880 { final boolean sword; class_6880$class_6883(boolean value) { sword=value; } public boolean method_40220(class_6862 tag) { return sword; } }
class class_6862 {} class class_3489 { static final class_6862 field_42611=new class_6862(); } class class_1294 { static final class_6880 field_5919=new class_6880(),field_5907=new class_6880(),field_5911=new class_6880(),field_5910=new class_6880(); }
class class_2248 {} class class_2246 { static final class_2248 field_10540=new class_2248(),field_9987=new class_2248(),SUPPORT=new class_2248(); }
class class_2680 { final class_2248 block; class_2680(class_2248 b) { block=b; } public boolean method_27852(class_2248 b) { return block==b; } public boolean method_26215() { return block==null; } public boolean method_26225() { return block!=null; } }
class class_2338 { final int x,y,z; class_2338(int x,int y,int z) { this.x=x; this.y=y; this.z=z; } public int method_10263() { return x; } public int method_10264() { return y; } public int method_10260() { return z; } }
class class_243 { final double field_1352,field_1351,field_1350; class_243(double x,double y,double z) { field_1352=x; field_1351=y; field_1350=z; } }
class class_238 {
    final double field_1323,field_1322,field_1321,field_1320,field_1325,field_1324;
    class_238(double x,double y,double z,double X,double Y,double Z) { field_1323=x; field_1322=y; field_1321=z; field_1320=X; field_1325=Y; field_1324=Z; }
    boolean overlaps(class_238 b) { return field_1323<b.field_1320 && field_1320>b.field_1323 && field_1322<b.field_1325 && field_1325>b.field_1322 && field_1321<b.field_1324 && field_1324>b.field_1321; }
}
class class_239$class_240 { static final class_239$class_240 field_1332=new class_239$class_240(),MISS=new class_239$class_240(); }
class class_239 { public class_239$class_240 method_17783() { return class_239$class_240.MISS; } }
class class_3965 extends class_239 {
    final class_2338 pos; final class_2350 side; boolean miss;
    class_3965(class_243 point,class_2350 side,class_2338 pos,boolean inside) { this.pos=pos; this.side=side; }
    public class_2338 method_17777() { return pos; } public class_2350 method_17780() { return side; }
    @Override public class_239$class_240 method_17783() { return miss?class_239$class_240.MISS:class_239$class_240.field_1332; }
}
class class_3966 extends class_239 { final class_1297 field_17592; class_3966(class_1297 e) { field_17592=e; } }
class class_2350 { static final class_2350 field_11036=new class_2350(); } class class_1268 { static final class_1268 field_5808=new class_1268(),field_5810=new class_1268(); }
class class_1269 { boolean accepted; class_1269(boolean accepted) { this.accepted=accepted; } public boolean method_23665() { return accepted; } }
class class_3959$class_3960 { static final class_3959$class_3960 field_17558=new class_3959$class_3960(); }
class class_3959$class_242 { static final class_3959$class_242 field_1348=new class_3959$class_242(); }
class class_3959 { final class_243 from,end; class_3959(class_243 from,class_243 end,class_3959$class_3960 shape,class_3959$class_242 fluid,class_1297 p) { this.from=from; this.end=end; } }
class class_638 {
    List<class_1297> entities=new ArrayList<>(); Map<String,class_2680> blocks=new HashMap<>(); boolean blocked; int difficulty=2,queries,rays;
    public class_1267 method_8407() { return new class_1267(difficulty); }
    static String key(class_2338 p) { return p.x+":"+p.y+":"+p.z; }
    public class_2680 method_8320(class_2338 p) { return blocks.getOrDefault(key(p),new class_2680(p.y<0?class_2246.SUPPORT:null)); }
    public class_1297 method_8469(int id) { for (class_1297 e:entities) if (e.id==id) return e; return null; }
    public List<class_1297> method_8335(class_1297 except,class_238 box) { List<class_1297> out=new ArrayList<>(); for (class_1297 e:entities) if (e!=except && box.overlaps(e.box)) out.add(e); return out; }
    public class_3965 method_17742(class_3959 ray) {
        ++rays;
        double dx=ray.end.field_1352-ray.from.field_1352,dy=ray.end.field_1351-ray.from.field_1351,dz=ray.end.field_1350-ray.from.field_1350;
        int steps=Math.max(1,(int)Math.ceil(Math.sqrt(dx*dx+dy*dy+dz*dz)*40));
        class_2338 pos=null; boolean found=blocked;
        for(int i=1;i<=steps;i++) {
            double f=(double)i/steps;
            pos=new class_2338((int)Math.floor(ray.from.field_1352+f*dx),(int)Math.floor(ray.from.field_1351+f*dy),(int)Math.floor(ray.from.field_1350+f*dz));
            if(method_8320(pos).block!=null) { found=true;break; }
        }
        class_3965 hit=new class_3965(ray.end,class_2350.field_11036,pos,false);hit.miss=!found;return hit;
    }
}
class class_636 {
    int attacks,places,bases,crystals,syncCount; boolean spawn=true,remove=true,failAttack,failPlace,accept=true; class_1268 lastHand; class_2338 lastBase;
    private void method_2911() { ++syncCount; }
    public void method_2918(class_1657 p,class_1297 e) { ++attacks; if (!(e instanceof class_1511)) throw new AssertionError("Aura attacked a living target"); if (e instanceof class_1511 && remove) class_310.field_1700.field_1687.entities.remove(e); if (failAttack) throw new IllegalStateException("after send"); }
    public class_1269 method_2896(class_746 p,class_1268 hand,class_3965 hit) {
        ++places; lastHand=hand;lastBase=hit.pos; class_638 w=class_310.field_1700.field_1687;
        if ((hand==class_1268.field_5810?p.offhand:p.inventory.stacks[p.inventory.field_7545]).item==class_1802.field_8281) { ++bases; class_2338 b=new class_2338(hit.pos.x,hit.pos.y+1,hit.pos.z); w.blocks.put(class_638.key(b),new class_2680(class_2246.field_10540)); }
        else { ++crystals; if (spawn) w.entities.add(new class_1511(90+crystals,new class_243(hit.pos.x+.5,hit.pos.y+1.,hit.pos.z+.5))); }
        if(failPlace) throw new IllegalStateException("after place"); return new class_1269(accept);
    }
}
class class_315 { class_304 field_1904=new class_304(); }
class class_304 { class_3675$class_306 field_1655=new class_3675$class_306(); int field_1661; boolean held; public boolean method_1434() { return held; } public void method_23481(boolean value) { held=value; } }
class class_3675$class_306 { public int method_1444() { return 1; } public class_3675$class_307 method_1442() { return class_3675$class_307.field_1672; } }
class class_3675$class_307 { static final class_3675$class_307 field_1668=new class_3675$class_307(),field_1671=new class_3675$class_307(),field_1672=new class_3675$class_307(); }

class class_1656 { boolean field_7480; }
class class_1293 { int amplifier; class_1293(int value) { amplifier=value; } public int method_5578() { return amplifier; } }
class class_5134 { static final class_6880 field_23725=new class_6880(); }
class class_1304 { static final class_1304 field_6166=new class_1304(),field_6172=new class_1304(),field_6174=new class_1304(),field_6169=new class_1304(); }
class class_9304 { Map<class_6880,Integer> entries=new LinkedHashMap<>(); public Set<Map.Entry<class_6880,Integer>> method_57539() { return entries.entrySet(); } }
class class_1267 { int value; class_1267(int value) { this.value=value; } public int method_5461() { return value; } }
