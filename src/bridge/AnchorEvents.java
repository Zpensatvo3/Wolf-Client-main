package net.onestep.hybrid;

import java.util.function.Consumer;
import java.util.function.Predicate;

/** Loaded by the game loader; callbacks never require a native hot-path hook. */
public final class AnchorEvents {
    private static volatile Object owner;
    private static volatile Consumer<Object> tick;
    private static volatile Predicate<Object> use;
    public static synchronized void install(Object token, Consumer<Object> onTick, Predicate<Object> onUse) {
        if (token == null || onTick == null || onUse == null) throw new IllegalArgumentException("Anchor events");
        tick = null; use = null; owner = token; use = onUse; tick = onTick;
    }
    public static synchronized void stop(Object token) {
        if (owner != token) return;
        tick = null; use = null; owner = null;
    }
    public static void onTick(Object client) {
        Consumer<Object> listener = tick;
        if (listener != null) listener.accept(client);
    }
    public static boolean beforeUse(Object client) {
        Predicate<Object> listener = use;
        return listener != null && listener.test(client);
    }
}
