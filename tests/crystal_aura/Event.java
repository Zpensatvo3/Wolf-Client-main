package net.fabricmc.fabric.api.event;

public final class Event<T> {
    private final java.util.List<Object> listeners=new java.util.ArrayList<>();
    public void register(Object listener) { listeners.add(listener); }
    public void fire(java.util.function.Consumer<Object> call) {
        for (Object listener:java.util.List.copyOf(listeners)) call.accept(listener);
    }
    public void clear() { listeners.clear(); }
}
