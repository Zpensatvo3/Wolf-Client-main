package net.onestep.privacy;

import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;

/** Display-only filter. No native calls, account changes, or packet interception. */
public final class NameProtectBridge {
    private static volatile String[] settings = {"", "Protected"};
    private static Class<?> ordered, characterVisitor, visitable, styledVisitor, plainVisitor;
    private static Method accept, acceptCharacter, visitStyled, styledAccept, plainAccept;
    private static Object emptyStyle;

    public static void prepare(ClassLoader loader, int target) throws ReflectiveOperationException {
        boolean named = switch (target) {
            case 12101, 12104, 12108, 12111 -> false;
            case 26100, 26101, 26102 -> true;
            default -> throw new IllegalArgumentException("Unsupported text target");
        };
        ordered = Class.forName(named ? "net.minecraft.util.FormattedCharSequence" : "net.minecraft.class_5481", true, loader);
        characterVisitor = Class.forName(named ? "net.minecraft.util.FormattedCharSink" : "net.minecraft.class_5224", true, loader);
        Class<?> style = Class.forName(named ? "net.minecraft.network.chat.Style" : "net.minecraft.class_2583", true, loader);
        visitable = Class.forName(named ? "net.minecraft.network.chat.FormattedText" : "net.minecraft.class_5348", true, loader);
        styledVisitor = Class.forName(named ? "net.minecraft.network.chat.FormattedText$StyledContentConsumer" : "net.minecraft.class_5348$class_5246", true, loader);
        plainVisitor = Class.forName(named ? "net.minecraft.network.chat.FormattedText$ContentConsumer" : "net.minecraft.class_5348$class_5245", true, loader);
        accept = ordered.getMethod("accept", characterVisitor);
        acceptCharacter = characterVisitor.getMethod("accept", int.class, style, int.class);
        visitStyled = visitable.getMethod(named ? "visit" : "method_27658", styledVisitor, style);
        styledAccept = styledVisitor.getMethod("accept", style, String.class);
        plainAccept = plainVisitor.getMethod("accept", String.class);
        emptyStyle = style.getField(named ? "EMPTY" : "field_24360").get(null);
    }

    public static void configure(String name, String alias) {
        settings = new String[]{name == null ? "" : name, alias == null ? "Protected" : alias};
    }

    private static boolean word(int c) {
        return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9' || c == '_';
    }
    private static boolean match(String text, int i, String name) {
        return i + name.length() <= text.length() && text.regionMatches(true, i, name, 0, name.length())
            && (i == 0 || !word(text.codePointBefore(i)))
            && (i + name.length() == text.length() || !word(text.codePointAt(i + name.length())));
    }

    private static String replace(String text, String[] config) {
        String name = config[0];
        StringBuilder result = null;
        for (int i = 0; i < text.length();) {
            if (match(text, i, name)) {
                if (result == null) result = new StringBuilder(text.substring(0, i));
                result.append(config[1]); i += name.length();
            } else {
                int cp = text.codePointAt(i);
                if (result != null) result.appendCodePoint(cp);
                i += Character.charCount(cp);
            }
        }
        return result == null ? text : result.toString();
    }

    private static String replaceFormatted(String text, String[] config) {
        if (text.indexOf('\u00a7') < 0) return replace(text, config);
        StringBuilder visible = new StringBuilder();
        int[] positions = new int[text.length()];
        for (int i = 0; i < text.length(); ++i) {
            if (text.charAt(i) == '\u00a7' && i + 1 < text.length()) { ++i; continue; }
            positions[visible.length()] = i; visible.append(text.charAt(i));
        }
        String plain = visible.toString();
        StringBuilder output = null;
        int cursor = 0;
        for (int i = 0; i < plain.length();) {
            if (match(plain, i, config[0])) {
                if (output == null) output = new StringBuilder();
                int start = positions[i], end = positions[i + config[0].length() - 1] + 1;
                output.append(text, cursor, start).append(config[1]);
                // Preserve formatting changes after the replacement, so following
                // text keeps its original color even if the name had multiple styles.
                for (int raw = start; raw + 1 < end;) {
                    if (text.charAt(raw) == '\u00a7') { output.append(text, raw, raw + 2); raw += 2; }
                    else ++raw;
                }
                cursor = end; i += config[0].length();
            } else ++i;
        }
        return output == null ? text : output.append(text, cursor, text.length()).toString();
    }

    private static Object identity(Object proxy, Method method, Object[] args) {
        if (method.getName().equals("hashCode")) return System.identityHashCode(proxy);
        if (method.getName().equals("equals")) return proxy == args[0];
        return "NameProtectText";
    }

    private static Object proxy(Class<?> type, java.lang.reflect.InvocationHandler handler) {
        return Proxy.newProxyInstance(NameProtectBridge.class.getClassLoader(), new Class<?>[]{type}, handler);
    }

    private static List<Object[]> masked(List<Object[]> glyphs, String[] config) {
        StringBuilder builder = new StringBuilder();
        for (Object[] glyph : glyphs) builder.appendCodePoint((Integer)glyph[1]);
        String text = builder.toString();
        if (replace(text, config) == text) return null;
        List<Object[]> result = new ArrayList<>();
        int offset = 0;
        for (int g = 0; g < glyphs.size();) {
            if (match(text, offset, config[0])) {
                Object style = glyphs.get(g)[0];
                for (int cp : config[1].codePoints().toArray()) result.add(new Object[]{style, cp});
                // Minecraft names are ASCII, hence glyph count equals UTF-16 length.
                g += config[0].length(); offset += config[0].length();
            } else {
                Object[] glyph = glyphs.get(g++);
                result.add(glyph); offset += Character.charCount((Integer)glyph[1]);
            }
        }
        return result;
    }

    public static Object filter(Object text) {
        String[] config = settings;
        if (text == null || config[0].isEmpty()) return text;
        try {
            if (text instanceof String) return replaceFormatted((String)text, config);
            List<Object[]> glyphs = new ArrayList<>();
            if (ordered.isInstance(text)) {
                Object collector = proxy(characterVisitor, (p, m, a) -> {
                    if (m.getDeclaringClass() == Object.class) return identity(p, m, a);
                    if (glyphs.size() >= 65536) throw new IllegalArgumentException("Text limit");
                    glyphs.add(new Object[]{a[1], a[2]}); return true;
                });
                accept.invoke(text, collector);
                List<Object[]> output = masked(glyphs, config);
                if (output == null) return text;
                return proxy(ordered, (p, m, a) -> {
                    if (m.getDeclaringClass() == Object.class) return identity(p, m, a);
                    int index = 0;
                    for (Object[] glyph : output) {
                        if (!Boolean.TRUE.equals(acceptCharacter.invoke(a[0], index, glyph[0], glyph[1]))) return false;
                        index += Character.charCount((Integer)glyph[1]);
                    }
                    return true;
                });
            }
            if (visitable.isInstance(text)) {
                Object collector = proxy(styledVisitor, (p, m, a) -> {
                    if (m.getDeclaringClass() == Object.class) return identity(p, m, a);
                    String run = (String)a[1];
                    for (int offset = 0; offset < run.length();) {
                        if (glyphs.size() >= 65536) throw new IllegalArgumentException("Text limit");
                        int cp = run.codePointAt(offset); offset += Character.charCount(cp);
                        glyphs.add(new Object[]{a[0], cp});
                    }
                    return Optional.empty();
                });
                visitStyled.invoke(text, collector, emptyStyle);
                List<Object[]> output = masked(glyphs, config);
                if (output == null) return text;
                return proxy(visitable, (p, m, a) -> {
                    if (m.getDeclaringClass() == Object.class) return identity(p, m, a);
                    if (a == null) {
                        StringBuilder value = new StringBuilder();
                        for (Object[] glyph : output) value.appendCodePoint((Integer)glyph[1]);
                        return value.toString();
                    }
                    for (Object[] glyph : output) {
                        String character = new String(Character.toChars((Integer)glyph[1]));
                        Object result = a.length == 2 ? styledAccept.invoke(a[0], glyph[0], character)
                            : plainAccept.invoke(a[0], character);
                        if (((Optional<?>)result).isPresent()) return result;
                    }
                    return Optional.empty();
                });
            }
        } catch (Throwable ignored) {
            // Keep the game's renderer alive if a mod supplies an incompatible text.
        }
        return text;
    }
}
