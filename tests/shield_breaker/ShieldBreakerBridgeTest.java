package net.minecraft;
import java.lang.reflect.*;
import java.util.*;
import net.onestep.hybrid.WorldBridge;
import net.fabricmc.fabric.api.client.event.lifecycle.v1.ClientTickEvents;

public final class ShieldBreakerBridgeTest {
    static int checks;
    static Field f(String name) throws Exception { Field f=WorldBridge.class.getDeclaredField(name);f.setAccessible(true);return f; }
    static void set(Object b,String name,Object value) throws Exception {f(name).set(b,value);}
    static Method m(Class<?> type,String name,Class<?>... args) throws Exception {Method m=type.getDeclaredMethod(name,args);m.setAccessible(true);return m;}
    static Field member(Class<?> type,String name) throws Exception {Field f=type.getDeclaredField(name);f.setAccessible(true);return f;}
    static void check(boolean value,String why) {++checks;if(!value)throw new AssertionError(why);}
    static class Bridge extends WorldBridge {
        boolean allowed=true,filter=true,owned,revokeAtAction,changeAtAction;
        int switches,hits;boolean back=true,stun,simulation,hold;
        long generation=1,manual;
        Bridge() throws Exception {super(ShieldBreakerBridgeTest.class.getClassLoader(),12111);inject(this);}
        @Override protected long shieldService(int op,Object p,Object w,Object e,long generation) {
            if(!allowed || generation!=this.generation)return op==3?-1:0;
            if(op==0)return e==null || filter?1:0;
            if(op==3)return manual;
            if(op==1){
                if(owned)return 0;
                if(revokeAtAction)allowed=false;
                if(changeAtAction)class_310.field_1700.fresh=new class_1657();
                try{return executeShieldAction()?1:0;}catch(Exception x){throw new IllegalStateException(x);}
            }
            return 0;
        }
        @Override protected long triggerService(int op,Object p,Object w,Object e,int value,long epoch) {
            if(op==4)return 1;
            if(op==5){owned=false;return 1;}
            if(!allowed)return op==8?-1:0;
            if(op==0)return filter?1:0;
            if(op==1){owned=true;return 1;}
            if(op==8)return manual;
            return -1;
        }
    }
    static void inject(Bridge b) throws Exception {
        boolean[] prepared=(boolean[])f("prepared").get(b);prepared[4]=prepared[9]=true;
        set(b,"shieldSync",m(class_636.class,"sync"));
        set(b,"triggerLiving",class_1309.class);set(b,"triggerPlayerType",class_1657.class);
        set(b,"triggerMob",Mob.class);set(b,"triggerAnimal",Animal.class);set(b,"triggerAxe",Axe.class);set(b,"triggerSword",Sword.class);
        set(b,"triggerHitType",class_3966.class);set(b,"triggerCrosshair",member(class_310.class,"field_1765"));set(b,"triggerHitEntity",member(class_3966.class,"entity"));
        set(b,"triggerSelected",member(Inventory.class,"slot"));set(b,"triggerFall",member(class_1297.class,"fall"));set(b,"triggerTicks",member(class_1309.class,"ticks"));
        set(b,"triggerYaw",member(class_1297.class,"yaw"));set(b,"triggerPos",member(class_1297.class,"pos"));
        set(b,"triggerX",member(Vec.class,"x"));set(b,"triggerZ",member(Vec.class,"z"));
        set(b,"triggerAlive",m(class_1297.class,"alive"));set(b,"triggerInvisible",m(class_1297.class,"invisible"));
        set(b,"triggerUsing",m(class_1309.class,"using"));set(b,"triggerInventory",m(class_1657.class,"inventory"));
        set(b,"triggerStack",m(Inventory.class,"stack",int.class));set(b,"triggerEmpty",m(Stack.class,"empty"));set(b,"triggerItem",m(Stack.class,"item"));
        set(b,"triggerCooldown",m(class_1309.class,"cooldown",float.class));set(b,"triggerGround",m(class_1297.class,"ground"));
        for(String name:new String[]{"triggerWater","triggerSwimming","triggerFlying","triggerClimbing","triggerRiding","triggerSprinting"})set(b,name,m(class_1297.class,"no"));
        set(b,"triggerBlind",m(class_1309.class,"blind",Object.class));set(b,"triggerBlindness",new Object());
        set(b,"triggerBlocking",m(class_1309.class,"blocking"));set(b,"triggerId",m(class_1297.class,"id"));
        set(b,"triggerCamera",m(class_310.class,"camera"));set(b,"triggerBlockRange",m(class_1657.class,"range"));set(b,"triggerEntityRange",m(class_1657.class,"range"));
        set(b,"triggerPick",m(class_746.class,"pick",class_1297.class,double.class,double.class,float.class));
        set(b,"triggerAttack",m(class_636.class,"attack",class_1657.class,class_1297.class));set(b,"triggerSwing",m(class_1657.class,"swing",Object.class));set(b,"triggerHand",new Object());
        set(b,"triggerSimulationPrepared",true);set(b,"triggerMouse",member(class_310.class,"mouse"));set(b,"triggerMouseLeft",member(Mouse.class,"left"));
        set(b,"triggerBindings",member(Binding.class,"all"));set(b,"triggerBindingPressed",member(Binding.class,"pressed"));set(b,"triggerBindingCount",member(Binding.class,"queued"));
        set(b,"triggerBindingKey",member(Binding.class,"key"));set(b,"triggerKeyCode",m(Key.class,"code"));set(b,"triggerKeyCategory",m(Key.class,"category"));set(b,"triggerMouseKeyType",Key.MOUSE);
        Constructor<?> ctor=Input.class.getDeclaredConstructor(int.class,int.class);ctor.setAccessible(true);set(b,"triggerMouseInputConstructor",ctor);
        set(b,"triggerMouseCallback",m(Mouse.class,"callback",long.class,Input.class,int.class));
        set(b,"triggerGlfwMouse",m(InputApi.class,"poll",long.class,int.class));
        set(b,"triggerGetWindow",m(class_310.class,"window"));set(b,"triggerGetHandle",m(Window.class,"handle"));
        Method hook=WorldBridge.class.getDeclaredMethod("prepareTickLifecycle");hook.setAccessible(true);hook.invoke(b);
    }
    static class_310 setup() {
        ClientTickEvents.START_CLIENT_TICK.clear();Binding.all.clear();Binding.all.put("attack",new Binding());Binding.all.put("other",new Binding());
        class_310 c=new class_310();class_310.field_1700=c;
        c.field_1724.inv.stacks[0]=new Stack(new Sword());c.field_1724.inv.stacks[3]=new Stack(new Axe());
        class_1657 e=new class_1657();e.pos=new Vec(0,2);e.yaw=180;e.blocking=true;
        c.field_1765=new class_3966(e);c.fresh=e;return c;
    }
    static void publish(Bridge b) throws Exception {b.tickShield(true,b.generation,b.switches,b.hits,3,75,b.back,b.stun,b.simulation,b.hold);}
    static void step(Bridge b) throws Exception {publish(b);ClientTickEvents.fire(class_310.field_1700);}
    static void recover(class_310 c){c.field_1724.cd=1;c.field_1724.ticks+=40;}
    static void trigger(Bridge b) throws Exception {b.tickTrigger(true,1,b.manual,.98f,.98f,0,0,0,1,0,false,true,false);}
    public static void main(String[] args) throws Exception {
        class_310 c=setup();Bridge b=new Bridge();step(b);
        check(c.field_1724.inv.slot==3 && c.field_1761.syncs==1 && c.field_1761.attacks==0,"select axe before cooldown-ready hit");
        recover(c);step(b);check(c.field_1761.attacks==1 && c.field_1761.slots.equals(List.of(3)),"one standalone axe attack");
        f("shieldReturnAt").setLong(b,1);step(b);check(c.field_1724.inv.slot==0 && c.field_1761.syncs==2,"delayed switch back");
        c=setup();b=new Bridge();b.switches=2;b.hits=2;step(b);step(b);check(c.field_1724.inv.slot==0,"switch delay in game ticks");
        step(b);check(c.field_1724.inv.slot==3,"switch delay completes");recover(c);step(b);check(c.field_1761.attacks==0,"hit delay after switch");
        for(int i=0;i<20;i++)publish(b);check(c.field_1761.attacks==0,"render publications do not consume tick delays");step(b);check(c.field_1761.attacks==1,"hit delay completes");
        c=setup();b=new Bridge();c.field_1724.inv.stacks[3]=new Stack(null);step(b);check(c.field_1761.attacks==0 && c.field_1724.inv.slot==0,"no axe does not attack/swap");
        c=setup();b=new Bridge();b.back=false;step(b);b.cancelShield();check(c.field_1724.inv.slot==0,"unconsumed preparation rolls back even with Switch Back off");
        c=setup();b=new Bridge();b.hold=true;publish(b);check(!b.prepareShieldManual(),"Hold Axe does not claim an unrelated vanilla sword click");step(b);check(c.field_1724.inv.slot==0,"Hold Axe prevents sword auto swap");
        c.field_1724.inv.slot=3;step(b);check(c.field_1761.attacks==1,"Hold Axe accepts existing axe");
        c=setup();b=new Bridge();b.back=false;step(b);recover(c);step(b);b.cancelShield();check(c.field_1724.inv.slot==3,"Switch Back off keeps axe");
        c=setup();b=new Bridge();step(b);b.cancelShield();check(c.field_1724.inv.slot==0,"cancel pending switch restores ownership");
        c=setup();b=new Bridge();step(b);c.field_1724.inv.slot=5;step(b);recover(c);step(b);check(c.field_1724.inv.slot==5 && c.field_1761.attacks==0,"manual slot override persists on same target");
        c=setup();b=new Bridge();step(b);c.field_1755=new class_437();step(b);check(c.field_1724.inv.slot==0,"menu restores owned slot");
        c=setup();b=new Bridge();step(b);c.field_1724=new class_746();c.field_1687=new class_638();step(b);check(c.field_1724.inv.slot==0,"new session never receives old restore");
        for(int invalid=0;invalid<5;invalid++) {
            c=setup();b=new Bridge();if(invalid==0)((class_1657)c.fresh).yaw=0;
            if(invalid==1)b.filter=false;if(invalid==2)c.field_1724.using=true;if(invalid==3)c.fresh=null;if(invalid==4)((class_1657)c.fresh).invisible=true;
            step(b);check(c.field_1761.attacks==0 && c.field_1724.inv.slot==0,"target/facing/using/filter/ray guard "+invalid);
        }
        c=setup();b=new Bridge();c.field_1724.inv.slot=3;b.changeAtAction=true;step(b);check(c.field_1761.attacks==0,"fresh final target after ownership acquisition");
        c=setup();b=new Bridge();c.field_1724.inv.slot=3;b.revokeAtAction=true;step(b);check(c.field_1761.attacks==0,"authorization revoked at action");
        c=setup();b=new Bridge();c.field_1724.inv.slot=3;b.stun=b.simulation=true;Binding.all.get("attack").pressed=true;Binding.all.get("attack").queued=3;c.mouse.left=true;
        step(b);check(c.field_1761.attacks==2 && c.mouse.downs==2 && c.mouse.ups==2,"standalone opt-in stun and callback pulses");
        check(Binding.all.get("attack").queued==3 && Binding.all.get("attack").pressed && c.mouse.left,"simulation preserves manual binding queue/hold");
        c=setup();b=new Bridge();step(b);c.field_1761.failSync=true;
        try{b.cancelShield();throw new AssertionError("sync failure expected");}catch(ReflectiveOperationException expected){}
        check((int)f("shieldOldSlot").get(b)==0,"failed restore keeps lease");b.cancelShield();check((int)f("shieldOldSlot").get(b)==-1,"restore retry finishes slot sync");
        // Actual Java Triggerbot + Shield Breaker, including sword-only policy.
        c=setup();b=new Bridge();b.stun=true;publish(b);trigger(b);ClientTickEvents.fire(c);
        check(c.field_1724.inv.slot==3 && c.field_1761.attacks==0,"Triggerbot prepares axe through Java");
        recover(c);f("triggerNotBefore").setLong(b,1);publish(b);trigger(b);ClientTickEvents.fire(c);
        check(c.field_1761.attacks==1 && c.field_1761.slots.equals(List.of(3)),"Trigger owns a single hit, no standalone stun duplicate");
        recover(c);publish(b);trigger(b);ClientTickEvents.fire(c);check(c.field_1761.attacks==1,"preserve Trigger post-hit timing");
        c=setup();b=new Bridge();b.switches=4;publish(b);trigger(b);ClientTickEvents.fire(c);
        check(f("shieldTarget").get(b)!=null && (int)f("shieldOldSlot").get(b)==-1,"waiting switch delay tracks a Java candidate");
        c.fresh=null;ClientTickEvents.fire(c);check(f("shieldTarget").get(b)==null && f("shieldPlayer").get(b)==null,"Trigger target loss clears pre-switch candidate references");
        c=setup();b=new Bridge();publish(b);trigger(b);ClientTickEvents.fire(c);c.field_1724.inv.slot=5;ClientTickEvents.fire(c);
        check(f("shieldManualOverride").get(b)!=null,"manual override tracks current target");
        c.field_1687=new class_638();c.field_1724=new class_746();c.fresh=null;ClientTickEvents.fire(c);
        check(f("shieldManualOverride").get(b)==null && f("shieldTarget").get(b)==null,"new world drops manual override and candidate references");
        c=setup();b=new Bridge();publish(b);trigger(b);ClientTickEvents.fire(c);b.cancelTrigger();check(c.field_1724.inv.slot==0,"Trigger cancellation rolls Java axe preparation back");
        c=setup();b=new Bridge();publish(b);trigger(b);ClientTickEvents.fire(c);c.field_1761.failSync=true;
        try{b.cancelTrigger();throw new AssertionError("trigger restore failure expected");}catch(ReflectiveOperationException expected){}
        int synced=c.field_1761.syncs;publish(b);ClientTickEvents.fire(c);
        check(c.field_1761.syncs==synced+1 && (int)f("shieldOldSlot").get(b)==-1,"tick retries failed Trigger rollback rather than treating it as manual slot change");
        c=setup();b=new Bridge();publish(b);trigger(b);ClientTickEvents.fire(c);
        check(b.prepareShieldManual(),"physical click takes Java slot lease");b.cancelTrigger();
        check(c.field_1724.inv.slot==3,"Trigger cancel does not steal the pending vanilla axe click");
        c.field_1761.attack(c.field_1724,c.fresh);f("shieldReturnAt").setLong(b,1);publish(b);ClientTickEvents.fire(c);
        check(c.field_1724.inv.slot==0 && c.field_1761.attacks==1,"manual takeover still restores slot without an extra attack");
        c=setup();b=new Bridge();publish(b);
        b.tickTrigger(true,1,0,.98f,.98f,0,0,0,1,0,true,true,false);ClientTickEvents.fire(c);
        publish(b);ClientTickEvents.fire(c);check(c.field_1761.attacks==0 && c.field_1724.inv.slot==0,"Trigger On Click cancellation never enables standalone Shield attack");
        c=setup();b=new Bridge();publish(b);trigger(b);c.field_1761.failSync=true;ClientTickEvents.fire(c);
        check((boolean)f("triggerFailed").get(b) && c.field_1724.inv.slot==0,"failed Java trigger switch restores partial preparation");
        recover(c);publish(b);ClientTickEvents.fire(c);check(c.field_1761.attacks==0,"failed Trigger controller does not replay through standalone Shield");
        c=setup();b=new Bridge();step(b);b.generation=2;ClientTickEvents.fire(c);check(c.field_1724.inv.slot==0,"generation invalidation restores before another hit");
        c=setup();b=new Bridge();set(b,"clientTickHooked",false);publish(b);long ticks=(long)f("shieldTick").get(b);publish(b);
        check((long)f("shieldTick").get(b)==ticks,"render fallback capped rather than frame-rate tick clock");
        b.stop();check(!(boolean)f("shieldActive").get(b) && f("shieldPlayer").get(b)==null,"stop clears Java references and activity");
        System.out.println("PASS Shield Breaker Java: "+checks+" checks; delays, leases, failures, simulation, final target, Triggerbot ownership");
    }
}
class class_310 {
    static class_310 field_1700;class_746 field_1724=new class_746();class_638 field_1687=new class_638();class_437 field_1755;class_315 field_1690=new class_315();class_636 field_1761=new class_636();class_239 field_1765;class_1297 fresh;Mouse mouse=new Mouse();Window window=new Window();
    public boolean method_18854(){return true;}public class_1297 camera(){return field_1724;}public Window window(){return window;}
}
class class_638 {} class class_437 {} class class_315 {} class class_239 {}
class class_3966 extends class_239 {class_1297 entity;class_3966(class_1297 e){entity=e;}}
class Vec {double x,z;Vec(double x,double z){this.x=x;this.z=z;}}
class class_1297 {boolean alive=true,invisible;float yaw;double fall;Vec pos=new Vec(0,0);public boolean method_5805(){return alive;}public boolean alive(){return alive;}public boolean invisible(){return invisible;}public boolean ground(){return true;}public boolean no(){return false;}public int id(){return 7;}}
class class_1309 extends class_1297 {float cd=1;int ticks=30;boolean using,blocking;public boolean using(){return using;}public boolean blocking(){return blocking;}public float cooldown(float delta){return cd;}public boolean blind(Object x){return false;}}
class class_1657 extends class_1309 {Inventory inv=new Inventory();public Inventory inventory(){return inv;}public double range(){return 3;}public void swing(Object hand){}}
class class_746 extends class_1657 {public static class_239 pick(class_1297 p,double b,double e,float delta){return new class_3966(class_310.field_1700.fresh);}}
class Mob extends class_1309 {} class Animal extends Mob {} class Axe {} class Sword {}
class Stack {Object item;Stack(Object i){item=i;}public boolean empty(){return item==null;}public Object item(){return item;}}
class Inventory {int slot;Stack[] stacks=new Stack[9];Inventory(){Arrays.setAll(stacks,i->new Stack(null));}public Stack stack(int i){return stacks[i];}}
class class_636 {int syncs,attacks;boolean failSync;List<Integer> slots=new ArrayList<>();public void sync(){if(failSync){failSync=false;throw new IllegalStateException("slot sync failure");}++syncs;class_310.field_1700.field_1724.cd=0;class_310.field_1700.field_1724.ticks=0;}public void attack(class_1657 p,class_1297 e){++attacks;slots.add(p.inv.slot);p.cd=0;p.ticks=0;}}
class Window {public long handle(){return 7;}}
class Input {Input(int button,int mods){}}
class Key {static final Object MOUSE=new Object();public int code(){return 0;}public Object category(){return MOUSE;}}
class Binding {static Map<String,Binding> all=new HashMap<>();Key key=new Key();boolean pressed;int queued;}
class Mouse {boolean left;int downs,ups;public void callback(long handle,Input input,int action){if(action==1)++downs;else ++ups;left=action==1;for(Binding b:Binding.all.values()){b.pressed=left;if(left)++b.queued;}}}

class InputApi {public static int poll(long handle,int button){return 0;}}
