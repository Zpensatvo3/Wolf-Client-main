package net.fabricmc.fabric.api.client.event.lifecycle.v1;

import net.fabricmc.fabric.api.event.Event;
public final class ClientTickEvents {
    public interface StartTick { void onStartTick(Object client); }
    public static final Event<StartTick> START_CLIENT_TICK=new Event<>();
    public static void fire(Object client) { START_CLIENT_TICK.fire(l -> ((StartTick)l).onStartTick(client)); }
}
