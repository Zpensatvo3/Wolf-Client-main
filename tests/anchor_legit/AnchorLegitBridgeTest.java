package net.minecraft;

import java.lang.reflect.Field;
import java.nio.file.*;
import java.util.*;
import java.util.regex.Pattern;
import org.lwjgl.glfw.GLFW;
import net.onestep.hybrid.WorldBridge;
import net.onestep.hybrid.AnchorEvents;

/** Reference-behavior scenarios with exact tick/use events, not render-frame approximations. */
public final class AnchorLegitBridgeTest {
    private static int checks;
    private static boolean[] flags;
    private static int[] values;
    private static final class Bridge extends WorldBridge {
        boolean allowed=true;
        final ArrayDeque<Integer> rolls=new ArrayDeque<>(); int randomCalls;
        Bridge() throws Exception { super(AnchorLegitBridgeTest.class.getClassLoader(),12111); }
        Bridge(int target,String[] names) throws Exception { super(AnchorLegitBridgeTest.class.getClassLoader(),target,names); }
        @Override protected boolean anchorPermitted(long generation) { return allowed; }
        @Override protected int anchorLegitPercent() { ++randomCalls; return rolls.isEmpty() ? 1 : rolls.remove(); }
    }
    private static void check(boolean value,String message) { ++checks; if (!value) throw new AssertionError(message); }
    private static Field field(String key) throws Exception { Field f=WorldBridge.class.getDeclaredField(key); f.setAccessible(true); return f; }
    private static int clock(Bridge b,String key) throws Exception { return field(key).getInt(b); }
    private static class_310 setup() {
        GLFW.reset(); GLFW.buttons[1]=1; class_304.field_1657.clear();
        flags=new boolean[]{true,false,false,false,false}; values=new int[]{100,0,100,0,100,0,100,1};
        class_310 c=new class_310(); class_310.field_1700=c;
        c.field_1724.inventory.stacks[0]=new class_1799(new class_1792());
        c.field_1724.inventory.stacks[3]=new class_1799(class_1802.field_23141);
        c.field_1724.inventory.stacks[4]=new class_1799(class_1802.field_8801);
        c.field_1687.blocks.put("0:0:3",new class_2680(class_2246.field_10540,false));
        aim(c,new class_2338(0,0,3)); return c;
    }
    private static void aim(class_310 c,class_2338 pos) { c.field_1765=new class_3965(null,class_2350.field_11036,pos,false); }
    private static void anchor(class_310 c,int x,int charge) {
        class_2680 state=new class_2680(class_2246.field_23152,false); state.charges=charge;
        c.field_1687.blocks.put(x+":1:3",state); aim(c,new class_2338(x,1,3));
    }
    private static Bridge bridge() throws Exception {
        Bridge b=new Bridge(); b.installAnchorEvents(); publish(b); return b;
    }
    private static void publish(Bridge b) throws Exception { b.tickAnchor(true,1,1,flags,values); }
    private static void tick() { AnchorEvents.onTick(class_310.field_1700); }
    private static boolean use() { return AnchorEvents.beforeUse(class_310.field_1700); }
    private static String[] bindings() throws Exception {
        var matches=Pattern.compile("\\{\"((?:net/minecraft/class_|field_|method_|comp_)[^\"]+)\", ").matcher(Files.readString(Path.of("src/services/game/world_bridge.cpp")));
        Set<String> names=new TreeSet<>(); while(matches.find()) names.add(matches.group(1));
        List<String> result=new ArrayList<>(); for(String name:names) { result.add(name); result.add(name); }
        return result.toArray(String[]::new);
    }
    public static void main(String[] args) throws Exception {
        class_310 c=setup(); c.field_1724.inventory.field_7545=3; Bridge b=bridge();
        tick(); tick(); check(c.field_1761.places == 0 && c.field_1761.charges == 0,"Legit never automatically places an anchor");
        check(!use(),"manual placement event proceeds to vanilla");
        check(((Set<?>)field("legitOwned").get(b)).contains("0:1:3"),"manual-use HEAD records the predicted adjacent position");
        c.field_1761.method_2896(c.field_1724,class_1268.field_5808,(class_3965)c.field_1765);
        aim(c,new class_2338(0,1,3)); tick();
        check(c.field_1761.places == 1 && c.field_1761.charges == 1 && c.field_1761.explosions == 1,"zero-delay charge and detonation can both run in one tick");
        check(c.field_1724.inventory.field_7545 == 0 && c.field_1761.syncCount == 0,"Legit uses selectedSlot directly and never restores original anchor slot");
        anchor(c,1,0); tick(); check(c.field_1761.explosions == 2,"holding right continues onto the next current target without release");
        check(c.field_1724.swings == 4,"accepted client-swing results swing for the two automated cycles plus manual call excluded");

        c=setup(); anchor(c,0,0); flags[4]=true; values[3]=1; b=bridge(); tick();
        check(clock(b,"legitGlowClock") == 1 && c.field_1761.charges == 0,"Glowstone Delay advances its event counter");
        anchor(c,2,0); tick(); check(c.field_1761.charges == 1,"counter carries to the newly aimed anchor");
        check(c.field_1687.method_8320(new class_2338(0,1,3)).charges == 0 && c.field_1687.method_8320(new class_2338(2,1,3)).charges == 1,"Legit uses the current crosshair, not a saved target");
        check(c.field_1724.inventory.field_7545 == 0 && c.field_1761.explosions == 0,"Only Charge still selects Explode Slot");
        values[7]=5; publish(b); tick(); check(c.field_1724.inventory.field_7545 == 4,"Legit accepts a glowstone Explode Slot just as the reference does");

        c=setup(); anchor(c,0,0); values[0]=50; b=bridge(); b.rolls.addAll(List.of(1,100)); tick();
        check(c.field_1761.charges == 0 && c.field_1724.inventory.field_7545 == 0,"Place Chance failure leaves the operation available");
        tick(); check(c.field_1761.explosions == 1,"failed chance is retried on the next tick");
        c=setup(); anchor(c,0,0); flags[4]=true; values[0]=0; values[3]=3; b=bridge(); tick(); tick();
        check(clock(b,"legitGlowClock") == 0 && clock(b,"legitSwitchClock") == 0,"Place Chance gates switch and glowstone counters");
        values[0]=100; values[4]=0; publish(b); tick(); tick(); tick(); tick();
        check(clock(b,"legitGlowClock") == 3 && c.field_1761.charges == 0,"failed glowstone chance holds the elapsed counter");
        values[4]=100; publish(b); tick(); check(c.field_1761.charges == 1,"setting changes preserve the counter and allow a later chance");
        c=setup(); anchor(c,0,0); values[1]=2; flags[4]=true; b=bridge(); tick();
        for(int i=0;i<5;++i) publish(b);
        check(clock(b,"legitSwitchClock") == 1,"render/config updates never count as extra ticks");
        GLFW.buttons[1]=0; tick(); check(clock(b,"legitSwitchClock") == 1,"right release pauses rather than resets a delay");
        GLFW.buttons[1]=1; tick(); tick(); check(c.field_1761.charges == 1,"counter resumes after right is pressed again");
        c=setup(); anchor(c,0,0); values[1]=3; b=bridge(); tick(); tick(); values[1]=1; publish(b); tick();
        check(clock(b,"legitSwitchClock") == 3 && c.field_1761.charges == 0,"counter equality and mid-delay edits match the source");

        c=setup(); anchor(c,0,0); flags[3]=true; b=bridge(); tick();
        check(c.field_1761.charges == 0,"Only Own rejects an unrecorded anchor");
        c.field_1724.inventory.field_7545=3; aim(c,new class_2338(0,0,3)); use(); aim(c,new class_2338(0,1,3)); tick();
        check(c.field_1761.explosions == 1,"Only Own accepts a normal manually placed anchor");
        c=setup(); flags[3]=true; c.field_1724.inventory.field_7545=3; b=bridge();
        aim(c,new class_2338(2,1,3)); use();
        check(((Set<?>)field("legitOwned").get(b)).contains("2:1:3"),"replaceable target records its own cell without a face offset");
        anchor(c,2,0); tick(); check(c.field_1761.explosions == 1,"reference ownership records do not require placement confirmation");
        c=setup(); c.field_1724.inventory.field_7545=3; b=bridge(); use();
        b.tickAnchor(false,1,1,flags,values); publish(b);
        check(((Set<?>)field("legitOwned").get(b)).contains("0:1:3"),"owned positions persist across disable/enable");
        check(clock(b,"legitSwitchClock") == 0,"enable resets the three clocks");
        c.field_1687=new class_638(); anchor(c,0,1); flags[3]=true; publish(b); tick();
        check(c.field_1761.explosions == 1,"coordinate ownership persistence across worlds matches the reference");

        c=setup(); anchor(c,0,0); GLFW.buttons[1]=0; b=bridge(); tick(); check(c.field_1761.charges == 0,"physical right is required");
        c=setup(); anchor(c,0,0); flags[0]=false; c.field_1724.inventory.stacks[0].item.food=true; b=bridge(); tick();
        check(c.field_1761.charges == 0,"While Use reads held item's default food components");
        c=setup(); anchor(c,0,0); flags[0]=false; c.field_1724.using=true; b=bridge(); tick();
        check(c.field_1761.explosions == 1,"source does not add a generic isUsingItem restriction");
        c=setup(); anchor(c,0,0); flags[1]=true; class_1657 body=new class_1657(); body.alive=false; body.move(new class_243(.5,0,2));
        c.field_1687.players.add(body); b=bridge(); tick(); check(c.field_1761.charges == 0,"Stop on Kill uses a dead living player within six blocks");
        c=setup(); anchor(c,0,0); c.field_1761.serverSwing=true; b=bridge(); tick();
        check(c.field_1761.explosions == 1 && c.field_1724.swings == 0,"server/no-swing results do not force a client hand swing");
        c=setup(); anchor(c,0,0); b=bridge(); b.allowed=false; tick(); check(c.field_1761.charges == 0,"authority guard still applies to injected ticks");

        c=setup(); anchor(c,0,0); flags[2]=true; b=bridge(); tick();
        check(c.field_1729.presses == 2 && c.field_1729.releases == 0,"Legit starts nominal 35ms mouse pulses, not synchronous pairs");
        check(use() && !use(),"item-use hook consumes one fake-use flag as in the reference");
        for(Object pulse:(List<?>)field("legitPulses").get(b)) ((Object[])pulse)[3]=0L;
        publish(b); check(c.field_1729.releases == 2,"render-side servicing releases expired pulses on the game thread");
        c=setup(); anchor(c,0,0); flags[2]=true; b=bridge(); tick(); b.allowed=false;
        check(use() && !use(),"a queued fake use is cancelled even after authority/disable generation changes");
        b.tickAnchor(false,1,1,flags,values);
        c=setup(); anchor(c,0,0); flags[2]=true; b=bridge(); tick(); b.tickAnchor(false,1,1,flags,values);
        check(c.field_1729.releases == 2 && !use(),"disable releases pending pulses and removes the fake-use cancellation");
        c=setup(); anchor(c,0,0); values[1]=2; b=bridge(); tick(); b.tickAnchor(true,1,0,flags,values);
        for(int i=0;i<20;++i) { ++c.field_1724.field_6012; b.tickAnchor(true,1,0,flags,values); }
        check(c.field_1761.explosions == 1,"switching to Normal uses its independent existing controller");
        c=setup(); anchor(c,0,0); b=bridge(); final Bridge threadBridge=b; final class_310 threadClient=c;
        Thread worker=new Thread(() -> AnchorEvents.onTick(threadClient)); worker.start(); worker.join();
        check(c.field_1761.charges == 0,"injected callbacks cannot run game mutations from a worker");
        b.stop(); tick(); check(c.field_1761.charges == 0,"stop detaches Java callbacks");
        for(int target:new int[]{12101,12104,12108,12111,26100,26101,26102}) {
            c=setup(); anchor(c,0,0); b=new Bridge(target,bindings()); b.installAnchorEvents(); publish(b); tick();
            check(c.field_1761.explosions == 1,"Legit exact-name protocol for "+target); b.stop();
        }
        System.out.println("PASS Anchor Legit reference behavior: "+checks+" checks ("+args[0]+"; isolated event fixtures)");
    }
}
