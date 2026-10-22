# Inventory Walk input ordering (1.21.11)

Grim's MultiActionsC checks inventory clicks while sprinting, sneaking or moving input is still active; MultiActionsD applies the same conditions to closing the inventory. Cancelling sprint alone leaves the independent `input=true` condition active.

References (reviewed revision):
- https://github.com/GrimAnticheat/Grim/blob/8baa16f2b55d188eea81436aaa484d19b61085cd/common/src/main/java/ac/grim/grimac/checks/impl/multiactions/MultiActionsC.java
- https://github.com/GrimAnticheat/Grim/blob/8baa16f2b55d188eea81436aaa484d19b61085cd/common/src/main/java/ac/grim/grimac/checks/impl/multiactions/MultiActionsD.java

## Change

- Pause actual movement before the original GLFW inventory mouse event, action hotkey or close key is dispatched. Movement keys use the player's current bindings rather than hard-coded WASD.
- Clear the real movement bindings, Input record and movement vector, stop sprinting, and synchronize that stopped state over the same ordered connection before the original action. Vanilla's sprint sender updates its own sent-state cache. The input sent-state cache is updated only after a successful send.
- Also prepare inventory actions from Mouse Tweaks and native inventory services, which do not necessarily pass through window events.
- Keep movement paused for 200 ms after an action and while a mouse button or automatic inventory operation remains active. Resume from physical keys. Closing the screen restores a still-held movement key to vanilla; focus loss, remapping and world changes must not restore stale input.
- Do not queue, replay or synthesize inventory clicks. On synchronization failure, cancel that original window action, log the Java error and suspend Inventory Walk.
- Replace the stale sprint timer mapping with `field_3935` and use the exact typed inherited `method_52787(Packet)` sender for 1.21.11.

## Checks

ClientBridge fixtures cover packet ordering, actual local stopped state, duplicate mouse events, pause/resume, remapped movement scans, Escape, gameplay handback and partial-send failure. These run against both source classes and embedded classes. `Test-InventoryWalkBindings.py` reads a real client jar to validate 11 action-related fields, methods and constructor contracts without launching Minecraft.

These checks do not establish live server compatibility. No guarantee is made about other server checks, configuration or game modifications. Restart Minecraft before loading the rebuilt test DLL; an already loaded DLL does not acquire the new embedded Java classes.
