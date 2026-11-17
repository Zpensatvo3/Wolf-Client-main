"""Execute shipping Triggerbot lifecycle/swing methods with inert Java objects.

These focused checks do not simulate target selection or game compatibility.
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
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


actions = source[source.index("            triggerAttack.invoke(m, p, e);"):]
actions = actions[:actions.index("            if (clickSimulation)")]
fixture = r'''
import java.lang.reflect.*;
public final class TriggerLifecycleTest {
    boolean triggerActive, fail;
    Object triggerPlayer, triggerWorld, triggerManager, triggerTarget, triggerInv;
    int triggerSlot, triggerPreparedSlot, triggerPreparationTicks, triggerLastTicks;
    float triggerLastCooldown;
    long triggerCritical, triggerReady;
    Field player, world;
    Method triggerAttack, triggerSwing;
    Object triggerHand = new Object();
    int releases, attacks, swings;
    Object client() { return null; }
    long triggerService(int operation, Object p, Object w, Object e, int value, long epoch)
            throws ReflectiveOperationException {
        if (operation != 5) throw new AssertionError("unexpected operation");
        releases++;
        if (fail) throw new ReflectiveOperationException("cleanup failure");
        return 1;
    }
    public void attack(Object p, Object e) { attacks++; }
    public void swing(Object hand) { swings++; }
    void perform(boolean swingHand) throws Exception {
        Object m=this, p=this, e=new Object();
        ACTIONS
    }
    CANCEL
    RESET
    void seed() {
        triggerActive=true;
        triggerPlayer=triggerWorld=triggerManager=triggerTarget=triggerInv=new Object();
        triggerSlot=triggerPreparedSlot=triggerPreparationTicks=triggerLastTicks=3;
        triggerReady=100;
    }
    void cleared() {
        if (triggerPlayer!=null || triggerWorld!=null || triggerManager!=null ||
            triggerTarget!=null || triggerInv!=null || triggerSlot!=-1 || triggerPreparedSlot!=-1 ||
            triggerPreparationTicks!=-1 || triggerLastTicks!=-1 || triggerReady!=100)
            throw new AssertionError("stale cycle retained");
    }
    public static void main(String[] args) throws Exception {
        TriggerLifecycleTest t=new TriggerLifecycleTest();
        t.seed(); t.resetTriggerCycle(); t.cleared();
        if (!t.triggerActive || t.releases!=1) throw new AssertionError("reacquisition stops ticks");
        t.seed(); t.cancelTrigger(); t.cleared();
        if (t.triggerActive) throw new AssertionError("cancel leaves ticks active");
        t.seed(); t.fail=true;
        try { t.cancelTrigger(); throw new AssertionError("failure swallowed"); }
        catch (ReflectiveOperationException expected) { t.cleared(); }
        if (t.triggerActive) throw new AssertionError("failed cancel active");
        t.fail=false; t.cancelTrigger(); t.cleared();
        t.triggerAttack=TriggerLifecycleTest.class.getMethod("attack",Object.class,Object.class);
        t.triggerSwing=TriggerLifecycleTest.class.getMethod("swing",Object.class);
        t.perform(false);
        if (t.attacks!=1 || t.swings!=0) throw new AssertionError("swing disabled");
        t.perform(true);
        if (t.attacks!=2 || t.swings!=1) throw new AssertionError("swing enabled");
        System.out.println("PASS Trigger lifecycle: target reset, disconnect, cleanup failure, cancellation, Swing Hand");
    }
}
'''.replace("ACTIONS", actions).replace("CANCEL", method("public void cancelTrigger()")) \
   .replace("RESET", method("private void resetTriggerCycle()"))

assert "resetTriggerCycle();" in method("private void executeTrigger(")
with tempfile.TemporaryDirectory(prefix="onestep-trigger-lifecycle-") as directory:
    path = Path(directory) / "TriggerLifecycleTest.java"
    path.write_text(fixture, encoding="utf-8")
    subprocess.run([shutil.which("javac") or "javac", "--release", "17", "-d", directory, str(path)], check=True)
    subprocess.run([shutil.which("java") or "java", "-cp", directory, "TriggerLifecycleTest"], check=True)
