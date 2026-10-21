package org.lwjgl.glfw;

/** No native window is opened by the offline inventory bridge fixtures. */
public final class GLFW {
    public static final int[] buttons = new int[8], keys = new int[512];
    public static void reset() { java.util.Arrays.fill(buttons, 0); java.util.Arrays.fill(keys, 0); }
    public static int glfwGetMouseButton(long window, int button) { return buttons[button]; }
    public static int glfwGetKey(long window, int key) { return keys[key]; }
}
