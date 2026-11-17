"""Run the shipping Triggerbot decision loop against deterministic game doubles.

Only the monotonic clock is substituted in executeTrigger. Reflection reads,
target eligibility, fresh raycast validation, cycle/reset logic and attack gates
come from WorldBridge.java. This does not establish in-game hit/server behavior.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
source = (ROOT / "src/bridge/WorldBridge.java").read_text(encoding="utf-8")


def method(signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


fixture = r'''
import java.lang.reflect.*;
public final class TriggerTimingTest {
    public static class Entity {
        boolean alive=true, invisible;
        public boolean alive() { return alive; }
        public boolean invisible() { return invisible; }
        public int id() { return 1; }
    }
    public static class Player extends Entity {
        public int ticks=30;
        public double fall=0;
        float cooldown=1;
        boolean using;
        final Inventory inv=new Inventory();
        public Inventory inventory() { return inv; }
        public float cooldown(float delta) { return cooldown; }
        public boolean using() { return using; }
        public boolean ground() { return true; }
        public boolean no() { return false; }
        public boolean blind(Object effect) { return false; }
        public double range() { return 3; }
        public void swing(Object hand) {}
    }
    public static class Mob extends Entity {}
    public static class Animal extends Mob {}
    public static class Inventory {
        public int slot;
        public Object stack(int slot) { return this; }
    }
    public static class Hit { public Entity entity; Hit(Entity e) { entity=e; } }
    public static class Client {
        public Player player=new Player();
        public Object world=new Object();
        public Client manager=this;
        public Hit hit=new Hit(new Player());
        Entity fresh=hit.entity;
        boolean menu, friend, switchOnPrepare;
        int attacks, raycasts;
        public Entity camera() { return player; }
        public Hit pick(Entity p, double blocks, double entities, float delta) {
            raycasts++; return new Hit(fresh);
        }
        public void attack(Player p, Entity e) { attacks++; p.cooldown=0; p.ticks=0; }
    }
    Client c=new Client();
    long now=1_000_000_000L;
    int target=12108;
    boolean triggerActive=true, triggerFailed, triggerCycleSeeded;
    Object triggerPlayer, triggerWorld, triggerManager, triggerTarget, triggerInv;
    long triggerEpoch, triggerGeneration, triggerManual, triggerReady, triggerCritical=-1, triggerNotBefore;
    int triggerSlot=-1, triggerLastTicks=-1, triggerPreparedSlot=-1, triggerPreparationTicks=-1;
    float triggerThreshold, triggerLastCooldown;
    Class<?> triggerLiving=Entity.class, triggerPlayerType=Player.class, triggerMob=Mob.class,
        triggerAnimal=Animal.class, triggerHitType=Hit.class;
    Field player, world, manager, triggerCrosshair, triggerHitEntity, triggerSelected, triggerTicks,
        triggerFall, triggerRenderer;
    Method triggerInventory, triggerStack, triggerCooldown, triggerGround, triggerWater, triggerSwimming,
        triggerFlying, triggerClimbing, triggerRiding, triggerSprinting, triggerBlind, triggerId,
        triggerAttack, triggerSwing, triggerAlive, triggerInvisible, triggerUsing, triggerCamera,
        triggerBlockRange, triggerEntityRange, triggerPick;
    Object triggerBlindness=new Object(), triggerHand=new Object();
    TriggerTimingTest() throws Exception {
        player=Client.class.getField("player"); world=Client.class.getField("world");
        manager=Client.class.getField("manager"); triggerRenderer=manager;
        triggerCrosshair=Client.class.getField("hit"); triggerHitEntity=Hit.class.getField("entity");
        triggerSelected=Inventory.class.getField("slot"); triggerTicks=Player.class.getField("ticks");
        triggerFall=Player.class.getField("fall");
        triggerInventory=Player.class.getMethod("inventory"); triggerStack=Inventory.class.getMethod("stack",int.class);
        triggerCooldown=Player.class.getMethod("cooldown",float.class);
        triggerGround=Player.class.getMethod("ground");
        triggerWater=triggerSwimming=triggerFlying=triggerClimbing=triggerRiding=triggerSprinting=Player.class.getMethod("no");
        triggerBlind=Player.class.getMethod("blind",Object.class); triggerId=Entity.class.getMethod("id");
        triggerAttack=Client.class.getMethod("attack",Player.class,Entity.class);
        triggerSwing=Player.class.getMethod("swing",Object.class);
        triggerAlive=Entity.class.getMethod("alive"); triggerInvisible=Entity.class.getMethod("invisible");
        triggerUsing=Player.class.getMethod("using"); triggerCamera=Client.class.getMethod("camera");
        triggerBlockRange=triggerEntityRange=Player.class.getMethod("range");
        triggerPick=Client.class.getMethod("pick",Entity.class,double.class,double.class,float.class);
    }
    Object client() { return c; }
    boolean live(Object client, Object p, Object w) { return !c.menu && p==c.player && w==c.world; }
    long triggerService(int op, Object p, Object w, Object e, int value, long epoch)
            throws ReflectiveOperationException {
        if (op==0) return c.friend ? 0 : 1;
        if (op==8) return 0;
        if (op==2 || op==3 || op==10) {
            if (c.switchOnPrepare) c.fresh=new Player();
            return -1;
        }
        return 1;
    }
    boolean triggerLeftDown(Object c) { return true; }
    void prepareTriggerSimulation() {}
    void simulateTriggerClick(Object c, Object p, Object w, long generation, long manual) {}
    boolean triggerWeapon(Object p, Object w, Object inv, int slot, int weapon) { return true; }
    boolean triggerShield(Object p, Object e) { return false; }
    boolean triggerAxe(Object s) { return false; }
    METHODS
    void run() throws Exception {
        executeTrigger(c, 1, 0, .5f, 1.f, 0, 0, 0, 0, 0, false, false, false);
    }
    void pending(float threshold, long ready) {
        triggerCycleSeeded=true; triggerThreshold=threshold; triggerReady=ready;
    }
    void leave() throws Exception { c.hit=new Hit(null); c.fresh=null; run(); }
    void enter(Entity e) { c.hit=new Hit(e); c.fresh=e; }
    static void check(boolean ok, String message) { if (!ok) throw new AssertionError(message); }
    public static void main(String[] args) throws Exception {
        TriggerTimingTest t=new TriggerTimingTest();
        Entity original=t.c.hit.entity;
        long ready=t.now+100_000_000L;
        t.pending(.98f, ready); t.run();
        for (int i=0;i<20;i++) {
            t.leave(); t.now+=1_000_000L; t.enter(original); t.run();
            check(t.triggerReady==ready && t.triggerThreshold==.98f, "reentry rerolled pending cycle");
        }
        check(t.c.attacks==0, "reentry bypassed reaction deadline");
        t.enter(new Player()); t.run();
        t.c.player.inv.slot=2; t.run();
        check(t.c.attacks==0 && t.triggerReady==ready && t.triggerThreshold==.98f,
            "direct target/slot change rerolled pending cycle");
        t.now=ready; t.run();
        check(t.c.attacks==1 && t.c.raycasts>=2, "ready attack or final raycast missing");
        // Actual successful attack has started another cycle. Preserve it through loss.
        float next=t.triggerThreshold; long nextReady=t.triggerReady, guard=t.triggerNotBefore;
        t.leave(); t.enter(new Player()); t.run();
        check(t.c.attacks==1 && t.triggerThreshold==next && t.triggerReady==nextReady &&
            t.triggerNotBefore==guard, "new target reset post-attack timing");
        t.pending(.98f,t.now); t.now+=60_000_000L;
        t.c.player.cooldown=.7f; t.c.player.ticks=7; t.run();
        t.leave(); t.enter(original); t.c.player.inv.slot=1; t.run();
        check(t.c.attacks==1 && t.triggerThreshold==.98f, "low-charge reentry/slot switch attacked");
        t.c.player.cooldown=1; t.c.player.ticks=30; t.run();
        check(t.c.attacks==2, "recovered cooldown unnecessarily delayed");
        // Existing target validation still rejects missing/stale/invalid targets.
        for (int mode=0;mode<6;mode++) {
            TriggerTimingTest invalid=new TriggerTimingTest(); invalid.pending(.98f,0);
            if (mode==0) invalid.c.fresh=null;
            if (mode==1) invalid.c.menu=true;
            if (mode==2) invalid.c.friend=true;
            if (mode==3) invalid.c.hit.entity.alive=false;
            if (mode==4) invalid.c.switchOnPrepare=true;
            if (mode==5) invalid.c.player.using=true;
            invalid.run(); check(invalid.c.attacks==0, "invalid/finally changed target attacked: "+mode);
        }
        TriggerTimingTest full=new TriggerTimingTest(); full.pending(.98f,0);
        full.triggerNotBefore=full.now+50_000_000L; full.run();
        check(full.c.attacks==0,"post-attack minimum guard bypassed");
        full.now=full.triggerNotBefore; full.run();
        check(full.c.attacks==1,"guard adds an extra cycle");
        System.out.println("PASS Trigger timing: repeated reentry, target/slot change, partial/full cooldown, deadlines, final target validation");
    }
}
'''
methods = "\n".join(method(signature) for signature in (
    "private void triggerCycle(", "private void ensureTriggerCycle(",
    "public void cancelTrigger()", "private void resetTriggerCycle()",
    "private Object triggerFresh(", "private boolean triggerEligible(",
    "private void executeTrigger(",
))
assert methods.count("System.nanoTime()") == 1
fixture = fixture.replace("METHODS", methods.replace("System.nanoTime()", "this.now"))
with tempfile.TemporaryDirectory(prefix="onestep-trigger-timing-") as directory:
    path = Path(directory) / "TriggerTimingTest.java"
    path.write_text(fixture, encoding="utf-8")
    subprocess.run([shutil.which("javac") or "javac", "--release", "17", "-d", directory, str(path)], check=True)
    subprocess.run([shutil.which("java") or "java", "-cp", directory, "TriggerTimingTest"], check=True)
