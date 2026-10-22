package net.onestep.hybrid;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;

/** Original OneStep client I/O. Screen policy, physical input and AntiBot stay native. */
public final class ClientBridge {
    private final ClassLoader loader;
    private final Class<?> playerType;
    private final Field instance, player, world, screen, options;
    private final Method onThread, alive;
    private final boolean[] prepared = new boolean[3];
    private Field[] keyFields;
    private Field bound, symbols, scans, mouse, search, book, bookSearch;
    private Method pressed, setPressed, code, category, focused, visible, active, children, bookOpen;
    private Class<?> handled, inventory, creative, text, parent, recipe, players;
    private Method entities, localGlow, flag, setGlow, setFlag;
    private Method isSprinting, setSprinting;
    private Field sprintKey, sprintTimer;
    private Field walkInput, walkFlags, walkVector, walkLastSent, walkNetwork;
    private Method walkSend, walkSendSprint, walkKeyScan;
    private java.lang.reflect.Constructor<?> walkInputPacket;
    private Object walkEmptyInput, walkZeroVector;
    private boolean walkActionsPrepared, walkActionFailed;
    private long walkActionPauseUntil;
    private static volatile boolean inventoryBusy = false;
    public void setInventoryBusy(boolean busy) { inventoryBusy = busy; }
    public boolean isInventoryBusy() { return inventoryBusy; }
    private Method getWindow, getHandle, glfwGetMouseButton;
    private final Object[][] walkOwners = new Object[6][];
    private final Object[][] walkActionOwners = new Object[6][];
    private final Map<Object, boolean[]> glowOwners = new IdentityHashMap<>();
    private Object glowPlayer, glowWorld;
    private long sequence, walkNonce, glowNonce;
    private volatile boolean stopped;

    public ClientBridge(ClassLoader loader, int target) throws ReflectiveOperationException {
        if (loader == null || target != 12111) throw new IllegalArgumentException("ClientBridge requires Fabric 1.21.11");
        this.loader = loader;
        Class<?> client = load("class_310"); playerType = load("class_746");
        instance = field(client, "field_1700", client, true);
        player = field(client, "field_1724", playerType, false);
        world = field(client, "field_1687", load("class_638"), false);
        screen = field(client, "field_1755", load("class_437"), false);
        options = field(client, "field_1690", load("class_315"), false);
        onThread = method(client, "method_18854", boolean.class);
        alive = method(playerType, "method_5805", boolean.class);
        getWindow = method(client, "method_22683", load("class_1041"));
        getHandle = method(load("class_1041"), "method_4490", long.class);
        Class<?> glfw = Class.forName("org.lwjgl.glfw.GLFW", false, loader);
        glfwGetMouseButton = method(glfw, "glfwGetMouseButton", int.class, true, long.class, int.class);
    }
    private Class<?> load(String name) throws ClassNotFoundException { return Class.forName("net.minecraft." + name, false, loader); }
    private static Field field(Class<?> owner, String name, Class<?> type, boolean stat) throws ReflectiveOperationException {
        for (Class<?> c = owner; c != null; c = c.getSuperclass()) try {
            Field f = c.getDeclaredField(name);
            if (f.getType() != type || Modifier.isStatic(f.getModifiers()) != stat || !f.trySetAccessible()) throw new IllegalAccessException(name);
            return f;
        } catch (NoSuchFieldException missing) { /* Exact inherited mapping only. */ }
        throw new NoSuchFieldException(name);
    }
    private static Method method(Class<?> owner, String name, Class<?> result, Class<?>... args) throws ReflectiveOperationException {
        return method(owner, name, result, false, args);
    }
    private static Method method(Class<?> owner, String name, Class<?> result, boolean stat, Class<?>... args) throws ReflectiveOperationException {
        for (Class<?> c = owner; c != null; c = c.getSuperclass()) try {
            Method m = c.getDeclaredMethod(name, args);
            if (m.getReturnType() != result || Modifier.isStatic(m.getModifiers()) != stat || !m.trySetAccessible()) throw new IllegalAccessException(name);
            return m;
        } catch (NoSuchMethodException missing) { /* Exact inherited mapping only. */ }
        throw new NoSuchMethodException(name);
    }
    public void prepare(int feature) throws ReflectiveOperationException {
        if (stopped || feature < 1 || feature > 2) throw new IllegalStateException("Client feature/lifecycle");
        if (prepared[feature]) return;
        if (feature == 1) {
            Class<?> opts = load("class_315"), binding = load("class_304"), input = load("class_3675$class_306"), type = load("class_3675$class_307");
            String[] names = {"field_1894", "field_1881", "field_1913", "field_1849", "field_1903", "field_1832"};
            keyFields = new Field[6];
            for (int i = 0; i < 6; ++i) keyFields[i] = field(opts, names[i], binding, false);
            bound = field(binding, "field_1655", input, false);
            pressed = method(binding, "method_1434", boolean.class); setPressed = method(binding, "method_23481", void.class, boolean.class);
            code = method(input, "method_1444", int.class); category = method(input, "method_1442", type);
            symbols = field(type, "field_1668", type, true); scans = field(type, "field_1671", type, true); mouse = field(type, "field_1672", type, true);
            handled = load("class_465"); inventory = load("class_490"); creative = load("class_481"); text = load("class_342"); parent = load("class_4069");
            focused = method(load("class_364"), "method_25370", boolean.class);
            visible = method(text, "method_1885", boolean.class); active = method(text, "method_20315", boolean.class);
            children = method(parent, "method_25396", List.class);
            search = field(creative, "field_2894", text, false);
            recipe = load("class_10260"); Class<?> widget = load("class_507");
            book = field(recipe, "field_54474", widget, false); bookSearch = field(widget, "field_3089", text, false);
            bookOpen = method(widget, "method_2605", boolean.class);
            Class<?> entity = load("class_1297");
            isSprinting = method(playerType, "method_5624", boolean.class);
            setSprinting = method(playerType, "method_5728", void.class, boolean.class);
            flag = method(entity, "method_5795", boolean.class, int.class);
            setFlag = method(entity, "method_5729", void.class, int.class, boolean.class);
            try { sprintKey = field(opts, "field_1867", binding, false); } catch (Throwable ignored) {}
            sprintTimer = field(playerType, "field_3935", int.class, false);
        } else {
            players = load("class_1657"); Class<?> entity = load("class_1297");
            entities = method(load("class_638"), "method_18112", Iterable.class);
            localGlow = method(entity, "method_36361", boolean.class); flag = method(entity, "method_5795", boolean.class, int.class);
            setGlow = method(entity, "method_5834", void.class, boolean.class); setFlag = method(entity, "method_5729", void.class, int.class, boolean.class);
        }
        prepared[feature] = true;
    }
    private void prepareWalkActions() throws ReflectiveOperationException {
        if (walkActionsPrepared) return;
        Class<?> input = load("class_744"), flags = load("class_10185"), vector = load("class_241");
        Class<?> network = load("class_634"), packet = load("class_2596");
        walkInput = field(playerType,"field_3913",input,false);
        walkFlags = field(input,"field_54155",flags,false);
        walkVector = field(input,"field_55868",vector,false);
        walkLastSent = field(playerType,"field_54156",flags,false);
        walkNetwork = field(playerType,"field_3944",network,false);
        walkEmptyInput = field(flags,"field_54098",flags,true).get(null);
        walkZeroVector = field(vector,"field_1340",vector,true).get(null);
        walkSend = method(network,"method_52787",void.class,packet);
        walkSendSprint = method(playerType,"method_46742",void.class);
        walkInputPacket = load("class_2851").getDeclaredConstructor(flags);
        if (!walkInputPacket.trySetAccessible() || walkEmptyInput == null || walkZeroVector == null)
            throw new IllegalStateException("Inventory Walk action bindings");
        walkKeyScan = Class.forName("org.lwjgl.glfw.GLFW",false,loader).getMethod("glfwGetKeyScancode",int.class);
        walkActionsPrepared = true;
    }
    /** Runs before the original window event / module inventory action, never after its click. */
    public boolean prepareWalkAction(int kind, int value) throws ReflectiveOperationException {
        if(walkActionFailed) return false;
        try {return executeWalkAction(kind,value);}
        catch(ReflectiveOperationException | RuntimeException error) {walkActionFailed=true;throw error;}
    }
    private boolean executeWalkAction(int kind, int value) throws ReflectiveOperationException {
        if (!prepared[1] || stopped) return true;
        Object c=client(); if(c==null) return false;
        Object p=player.get(c),w=world.get(c),s=screen.get(c),opts=options.get(c);
        if(p==null || w==null || opts==null || !handled.isInstance(s)) return true;
        prepareWalkActions();
        // Movement key presses are not inventory actions. Compare real remapped scan codes.
        if(kind==0) for(Field keyField:keyFields) {
            Object key=keyField.get(opts);int[] d=descriptor(key==null?null:bound.get(key));
            int scan=d[0]==0?(int)walkKeyScan.invoke(null,d[1]):d[0]==1?d[1]:-1;
            if(scan>=0 && scan==value) return true;
        }
        Object input=walkInput.get(p),network=walkNetwork.get(p);
        if(input==null || network==null || !(boolean)alive.invoke(p)) return false;
        boolean changed=!walkEmptyInput.equals(walkFlags.get(input)) || !walkEmptyInput.equals(walkLastSent.get(p));
        walkActionPauseUntil=System.nanoTime()+200_000_000L;
        for(int i=0;i<keyFields.length;i++) {
            Object key=keyFields[i].get(opts);
            if(key!=null && ((boolean)pressed.invoke(key) || walkOwners[i]!=null) && walkActionOwners[i]==null)
                walkActionOwners[i]=new Object[]{p,w,opts,key,bound.get(key)};
            if(key!=null) setPressed.invoke(key,false);
            walkOwners[i]=null;
        }
        if(sprintKey!=null) setPressed.invoke(sprintKey.get(opts),false);
        if(sprintTimer!=null) sprintTimer.setInt(p,0);
        setSprinting.invoke(p,false);
        walkFlags.set(input,walkEmptyInput);walkVector.set(input,walkZeroVector);
        // This is the actual stopped local input, sent before the click/close on the same connection.
        // Vanilla's sprint sender also updates its own last-sent sprint tracking.
        walkSendSprint.invoke(p);
        if(changed) {
            walkSend.invoke(network,walkInputPacket.newInstance(walkEmptyInput));
            walkLastSent.set(p,walkEmptyInput);
        }
        return client()==c && player.get(c)==p && world.get(c)==w && screen.get(c)==s && options.get(c)==opts;
    }
    private Object client() throws ReflectiveOperationException {
        if (stopped) return null;
        Object c = instance.get(null); return c != null && (boolean) onThread.invoke(c) ? c : null;
    }
    private long next() { if (++sequence >= (1L << 53)) { stop(); return 0; } return sequence; }
    private boolean focusedText(Object object) throws ReflectiveOperationException {
        return object != null && (boolean) focused.invoke(object) && (boolean) visible.invoke(object) && (boolean) active.invoke(object);
    }
    private boolean typingTree(Object object, int depth, int[] budget) throws ReflectiveOperationException {
        if (object == null) return false;
        if (depth > 12 || --budget[0] < 0) return true;
        if (text.isInstance(object)) return focusedText(object);
        if (!parent.isInstance(object)) return false;
        Object values = children.invoke(object);
        if (!(values instanceof List<?> list) || list.size() > budget[0]) return true;
        for (Object child : list) if (typingTree(child, depth + 1, budget)) return true;
        return false;
    }
    private boolean typing(Object s) throws ReflectiveOperationException {
        if (typingTree(s, 0, new int[]{512})) return true;
        if (creative.isInstance(s) && focusedText(search.get(s))) return true;
        if (recipe.isInstance(s)) {
            Object widget = book.get(s);
            if (widget != null && (boolean) bookOpen.invoke(widget) && focusedText(bookSearch.get(widget))) return true;
        }
        return false;
    }
    private int[] descriptor(Object input) throws ReflectiveOperationException {
        if (input == null) return new int[]{-1, -1};
        Object type = category.invoke(input);
        return new int[]{type == symbols.get(null) ? 0 : type == scans.get(null) ? 1 : type == mouse.get(null) ? 2 : -1, (int) code.invoke(input)};
    }
    /** {owner,p,w,screen,options,keys[6],boundKeys[6],double[17]:schema,nonce,alive,screenKind,typing,descriptors[12]}. */
    public Object[] captureWalk() throws ReflectiveOperationException {
        Object c = client(); if (c == null) return null;
        walkNonce = 0; prepare(1);
        Object p = player.get(c), w = world.get(c), s = screen.get(c), opts = options.get(c);
        if (p == null || w == null || opts == null) return null;
        int kind = s == null ? 0 : handled.isInstance(s) ? (inventory.isInstance(s) || creative.isInstance(s) ? 1 : 2) : 3;
        Object[] keys = new Object[6], inputs = new Object[6];
        double[] data = new double[17]; data[0] = 1; data[1] = walkNonce = next();
        data[2] = (boolean) alive.invoke(p) ? 1 : 0; data[3] = kind; data[4] = kind == 1 || kind == 2 ? (typing(s) ? 1 : 0) : 0;
        for (int i = 0; i < 6; ++i) {
            keys[i] = keyFields[i].get(opts); inputs[i] = keys[i] == null ? null : bound.get(keys[i]);
            int[] input = descriptor(inputs[i]); data[5 + i * 2] = input[0]; data[6 + i * 2] = input[1];
        }
        return stopped ? null : new Object[]{this, p, w, s, opts, keys, inputs, data};
    }
    private boolean releaseKey(int i, Object c, boolean vanilla, boolean physical, int kind, int keyCode) throws ReflectiveOperationException {
        Object[] owner = walkOwners[i]; if (owner == null) return true;
        Object opts = options.get(c), key = opts == null ? null : keyFields[i].get(opts);
        // KeyBinding belongs to GameOptions, not a world; clear our same global key across disconnects too.
        if (opts == owner[2] && key == owner[3]) {
            int[] input = descriptor(bound.get(key));
            if (input[0] != kind || input[1] != keyCode) return false;
            if ((boolean) pressed.invoke(key) && !(vanilla && physical)) setPressed.invoke(key, false);
        }
        walkOwners[i] = null; return true;
    }
    public int[] walkInputs() throws ReflectiveOperationException {
        boolean owned = false; for (Object[] owner : walkOwners) owned |= owner != null;
        for(Object[] owner:walkActionOwners) owned |= owner!=null;
        if (!owned) return null;
        Object c = client(); if (c == null) return null;
        Object opts = options.get(c); int[] result = new int[12]; Arrays.fill(result, -1);
        if (opts != null) for (int i = 0; i < 6; ++i) {
            Object key = keyFields[i].get(opts); int[] input = descriptor(key == null ? null : bound.get(key));
            result[i * 2] = input[0]; result[i * 2 + 1] = input[1];
        }
        return result;
    }
    public boolean releaseWalk(boolean vanilla, int[] descriptors, boolean[] physical) throws ReflectiveOperationException {
        walkNonce = 0;
        walkActionPauseUntil = 0;
        boolean owned = false; for (Object[] owner : walkOwners) owned |= owner != null;
        for(Object[] owner:walkActionOwners) owned |= owner!=null;
        if (!owned) return true;
        Object c = client(); if (c == null || descriptors == null || descriptors.length != 12 || physical == null || physical.length != 6) return false;
        boolean success = true;
        for (int i = 0; i < 6; ++i) if (!releaseKey(i, c, vanilla, physical[i], descriptors[i * 2], descriptors[i * 2 + 1])) success = false;
        for(int i=0;i<6;i++) {
            Object[] owner=walkActionOwners[i];if(owner==null) continue;
            if(vanilla && screen.get(c)==null && player.get(c)==owner[0] && world.get(c)==owner[1] && options.get(c)==owner[2] &&
                keyFields[i].get(owner[2])==owner[3] && bound.get(owner[3])==owner[4]) {
                int[] d=descriptor(owner[4]);
                if(d[0]==descriptors[2*i] && d[1]==descriptors[2*i+1]) setPressed.invoke(owner[3],physical[i]);
            }
            walkActionOwners[i]=null;
        }
        return success;
    }
    public boolean applyWalk(Object[] packet, boolean[] desired) throws ReflectiveOperationException {
        if (client() == null || packet == null || packet.length != 8 || packet[0] != this || desired == null || desired.length != 6 ||
            !(packet[7] instanceof double[] before) || before.length != 17 || before[0] != 1 || before[1] <= 0 || before[1] != walkNonce) return false;
        walkNonce = 0; Object[] now = captureWalk(); walkNonce = 0;
        if (now == null) return false;
        for (int i = 1; i <= 4; ++i) if (now[i] != packet[i]) return false;
        double[] data = (double[]) now[7];
        if (data[2] != 1 || data[4] != 0 || (data[3] != 1 && data[3] != 2)) return false;
        if (!Arrays.equals(before, 2, 17, data, 2, 17)) return false;
        Object[] keys = (Object[]) now[5], inputs = (Object[]) now[6];
        for (int i = 0; i < 6; ++i) if (keys[i] != ((Object[]) packet[5])[i] || inputs[i] != ((Object[]) packet[6])[i]) return false;
        Object c = client();
        Object p = now[1];
        if (p != null) {
            try {
                if (isSprinting != null && (boolean) isSprinting.invoke(p)) {
                    setSprinting.invoke(p, false);
                }
                if (flag != null && (boolean) flag.invoke(p, 3)) {
                    setFlag.invoke(p, 3, false);
                }
            } catch (Throwable ignored) {}
        }
        if (sprintKey != null && now[4] != null) {
            try {
                Object sk = sprintKey.get(now[4]);
                if (sk != null && (boolean) pressed.invoke(sk)) {
                    setPressed.invoke(sk, false);
                }
            } catch (Throwable ignored) {}
        }
        if (sprintTimer != null && p != null) {
            try { sprintTimer.setInt(p, 0); } catch (Throwable ignored) {}
        }
        boolean mouseInteracting = false;
        if (data[3] == 1 || data[3] == 2) {
            try {
                long handle = (long) getHandle.invoke(getWindow.invoke(c));
                if ((int) glfwGetMouseButton.invoke(null, handle, 0) == 1 || (int) glfwGetMouseButton.invoke(null, handle, 1) == 1) {
                    mouseInteracting = true;
                }
            } catch (Throwable ignored) {}
        }
        boolean busy = inventoryBusy || mouseInteracting || System.nanoTime()<walkActionPauseUntil;
        for (int i = 0; i < 6; ++i) {
            Object[] owner = walkOwners[i];
            if (owner != null && (owner[0] != now[1] || owner[1] != now[2] || owner[2] != now[4] || owner[3] != keys[i] || owner[4] != inputs[i]))
                if (!releaseKey(i, c, false, false, (int) data[5 + i * 2], (int) data[6 + i * 2])) return false;
            boolean wantKey = desired[i] && !busy;
            if (!wantKey || keys[i] == null || inputs[i] == null || data[5 + i * 2] < 0) {
                if (!releaseKey(i, c, false, false, (int) data[5 + i * 2], (int) data[6 + i * 2])) return false;
            } else if (!(boolean) pressed.invoke(keys[i])) {
                walkActionOwners[i]=null;
                walkOwners[i] = new Object[]{now[1], now[2], now[4], keys[i], inputs[i]};
                setPressed.invoke(keys[i], true);
            }
        }
        return true;
    }
    private IdentityHashMap<Object, Boolean> members(Object w) throws ReflectiveOperationException {
        Object iterable = entities.invoke(w); if (!(iterable instanceof Iterable<?> list)) return null;
        IdentityHashMap<Object, Boolean> result = new IdentityHashMap<>(); int count = 0;
        for (Object entity : list) { if (++count > 16384) return null; if (entity != null) result.put(entity, true); }
        return result;
    }
    /** {owner,p,w,players[],double[2]:schema,nonce}. */
    public Object[] captureGlow() throws ReflectiveOperationException {
        Object c = client(); if (c == null) return null;
        glowNonce = 0; prepare(2);
        Object p = player.get(c), w = world.get(c);
        if (p == null || w == null) return null;
        IdentityHashMap<Object, Boolean> current = members(w); if (current == null) return null;
        List<Object> candidates = new ArrayList<>();
        for (Object e : current.keySet()) if (e != p && players.isInstance(e) && (boolean) alive.invoke(e)) {
            if (candidates.size() >= 4096) return null; candidates.add(e);
        }
        return new Object[]{this, p, w, candidates.toArray(), new double[]{1, glowNonce = next()}};
    }
    private void restoreGlow(Object e) throws ReflectiveOperationException {
        boolean[] old = glowOwners.get(e); if (old == null) return;
        // Restore only values we changed and that still have our last value.
        if (!old[0] && (boolean) localGlow.invoke(e)) setGlow.invoke(e, false);
        if (!old[1] && (boolean) flag.invoke(e, 6)) setFlag.invoke(e, 6, false);
        glowOwners.remove(e);
    }
    public boolean applyGlow(Object[] packet, boolean[] allowed) throws ReflectiveOperationException {
        Object c = client();
        if (c == null || packet == null || packet.length != 5 || packet[0] != this || !(packet[3] instanceof Object[] candidates) ||
            allowed == null || allowed.length != candidates.length || !(packet[4] instanceof double[] data) || data.length != 2 ||
            data[0] != 1 || data[1] <= 0 || data[1] != glowNonce) return false;
        glowNonce = 0;
        Object p = player.get(c), w = world.get(c); if (p != packet[1] || w != packet[2] || p == null || w == null) return false;
        IdentityHashMap<Object, Boolean> current = members(w); if (current == null) return false;
        if (glowWorld != w || glowPlayer != p) glowOwners.clear();
        glowWorld = w; glowPlayer = p;
        glowOwners.keySet().removeIf(e -> !current.containsKey(e));
        IdentityHashMap<Object, Boolean> wanted = new IdentityHashMap<>();
        for (int i = 0; i < candidates.length; ++i) {
            Object e = candidates[i];
            if (!allowed[i] || e == p || !players.isInstance(e) || !current.containsKey(e) || !(boolean) alive.invoke(e)) continue;
            wanted.put(e, true);
            boolean local = (boolean) localGlow.invoke(e), marked = (boolean) flag.invoke(e, 6);
            if (!glowOwners.containsKey(e) && (!local || !marked)) glowOwners.put(e, new boolean[]{local, marked});
            if (!local) setGlow.invoke(e, true);
            if (!marked) setFlag.invoke(e, 6, true);
        }
        for (Object e : new ArrayList<>(glowOwners.keySet())) if (!wanted.containsKey(e)) restoreGlow(e);
        return true;
    }
    public boolean releaseGlow() throws ReflectiveOperationException {
        glowNonce = 0;
        if (glowOwners.isEmpty()) { glowPlayer = glowWorld = null; return true; }
        Object c = client(); if (c == null) return false;
        if (player.get(c) != glowPlayer || world.get(c) != glowWorld) { glowOwners.clear(); glowPlayer = glowWorld = null; return true; }
        IdentityHashMap<Object, Boolean> current = members(glowWorld); if (current == null) return false;
        for (Object e : new ArrayList<>(glowOwners.keySet())) {
            if (current.containsKey(e)) restoreGlow(e); else glowOwners.remove(e);
        }
        glowPlayer = glowWorld = null; return true;
    }
    public void stop() { stopped = true; Arrays.fill(walkOwners, null); Arrays.fill(walkActionOwners,null); glowOwners.clear(); glowPlayer = glowWorld = null; walkNonce = glowNonce = 0; }
}
