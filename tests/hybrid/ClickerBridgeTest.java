package net.minecraft;

import net.onestep.hybrid.ItemBridge;

public final class ClickerBridgeTest {
    private static int checks;
    private static void check(boolean condition, String label) {
        ++checks; if (!condition) throw new AssertionError(label);
    }
    public static void main(String[] args) throws Exception {
        class_310 client = new class_310(); class_310.field_1700 = client;
        class_746 p = client.field_1724;
        ItemBridge bridge = new ItemBridge(ClickerBridgeTest.class.getClassLoader(), 12111);
        Object[] click = bridge.captureClick(3, false);
        check(bridge.click(click, false, true) && client.attackCount == 1, "one direct vanilla attack");
        check(!bridge.click(click, false, true) && client.attackCount == 1, "no duplicate attack");
        p.field_5965 = 89;
        check(bridge.jitter(click, 2, 5) && p.field_6031 == 2 && p.field_5965 == 90, "native deltas, pitch clamp");
        check(!bridge.jitter(click, 2, 5), "jitter single use");
        click = bridge.captureClick(3, false);
        check(bridge.click(click, true, true) && client.attackCount == 1, "shared Game Input validation adds no attack");
        click = bridge.captureClick(3, false);
        check(!bridge.click(click, false, false), "physical release");
        p.usingItem = true;
        check(bridge.captureClick(3, false) == null && bridge.captureClick(4, false) == null, "using-item gate");
        p.usingItem = false;
        check(bridge.captureClick(4, true) == null, "only blocks rejects empty main hand");
        p.mainHand = new class_1799(new class_1747());
        click = bridge.captureClick(4, true); client.field_1752 = 4;
        check(bridge.click(click, false, true) && client.useCount == 1, "one vanilla use");
        click = bridge.captureClick(4, true); client.field_1752 = 4;
        check(bridge.click(click, true, true) && client.useCount == 1 && client.field_1752 == 0, "callback gate clears delay only");
        click = bridge.captureClick(4, true); p.mainHand.heldItem = new class_1792();
        check(!bridge.click(click, false, true), "block changed in place");
        click = bridge.captureClick(4, false); p.mainHand = new class_1799(new class_1747());
        check(!bridge.click(click, false, true), "main stack changed");
        click = bridge.captureClick(3, false); client.field_1687 = new class_638();
        check(!bridge.click(click, false, true), "world change");
        click = bridge.captureClick(3, false); p.alive = false;
        check(!bridge.click(click, false, true), "dead player"); p.alive = true;

        class_465 screen = new class_465(); client.field_1755 = screen;
        p.field_7512 = screen.field_2797;
        class_1735 slot = new class_1735(0);
        screen.field_2787 = slot; screen.field_2797.field_7761.add(slot);
        check(bridge.captureClick(3, false) == null, "clickers disabled in inventory");
        Object[] inventory = bridge.captureInventory();
        check(bridge.pickup(inventory, true) && client.field_1761.inventoryClicks == 1, "one PICKUP transaction");
        check(!bridge.pickup(inventory, true), "inventory packet single use");
        inventory = bridge.captureInventory();
        check(!bridge.pickup(inventory, false), "inventory physical release");
        inventory = bridge.captureInventory(); screen.field_2787 = null;
        check(!bridge.pickup(inventory, true), "mouse left focused slot");
        inventory = bridge.captureInventory();
        check(((double[]) inventory[6])[4] == -1 && !bridge.pickup(inventory, true), "no slot keeps screen snapshot, no click");
        screen.field_2787 = slot;
        inventory = bridge.captureInventory(); ++screen.field_2797.field_7763;
        check(!bridge.pickup(inventory, true), "handler sync changed");
        inventory = bridge.captureInventory(); client.field_1755 = new class_465();
        check(!bridge.pickup(inventory, true), "screen replaced"); client.field_1755 = screen;
        p.field_7512 = new class_1703();
        check(bridge.captureInventory() == null, "player/screen handler mismatch"); p.field_7512 = screen.field_2797;
        screen.field_2787 = new class_1735(0);
        check(bridge.captureInventory() == null, "foreign slot with same numeric id rejected"); screen.field_2787 = slot;
        inventory = bridge.captureInventory(); Object[] heldInventory = inventory;
        Throwable[] error = {null};
        Thread worker = new Thread(() -> {
            try { check(bridge.captureInventory() == null && !bridge.pickup(heldInventory, true), "wrong thread"); }
            catch (Throwable failure) { error[0] = failure; }
        });
        worker.start(); worker.join(); if (error[0] != null) throw new AssertionError(error[0]);
        check(bridge.pickup(inventory, true), "worker didn't consume main-thread packet");
        inventory = bridge.captureInventory(); bridge.stop();
        check(!bridge.pickup(inventory, true) && bridge.captureClick(3, false) == null, "shutdown");
        System.out.println("PASS clicker bridge: " + checks + " checks (" + args[0] + ")");
    }
}
