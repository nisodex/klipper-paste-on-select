# Design Document: Klipper Native "Paste on Select" (Wayland & X11)

**Date:** 2026-11-05  
**Topic:** Native implementation of Paste on Select for KDE Klipper  
**Target:** Upstream KDE `plasma-workspace/klipper` (KDE Bug #427214)

---

## 1. Intent & Context

Many users coming from environments like GNOME (via the *Pano* extension) or Windows (*Ditto*) expect that selecting an item from the clipboard history automatically pastes it into their active application window without needing an extra manual `Ctrl+V` keypress.

In KDE Plasma, this request is tracked under **Bug #427214**. Past community attempts relied on fragile third-party hacks—such as spawning `ydotool` via legacy `Plasma5Support.DataSource` in QML and requiring background daemons with `/dev/uinput` privileges.

The intent of this project is to implement **Paste on Select** 100% natively in `plasma-workspace` (Klipper) using KDE Frameworks 6 and Qt 6, using native Wayland (`org_kde_kwin_fake_input`) and X11 (`XTest`) mechanisms, with fully configurable shortcut keys, structured cleanly for upstream submission to KDE.

---

## 2. Goals, Non-Goals & Anti-Goals

### Goals
* **Native & Dependency-Free:** Zero external CLI tools (no `ydotool`, `wtype`, or `xdotool`).
* **Multi-Platform Support:** Seamless operation under both Wayland (via `org_kde_kwin_fake_input`) and X11 (via `XTest`).
* **Configurable Keystroke:** Provide a checkbox *"Paste selected item automatically"* and an associated `KKeySequenceWidget` in Klipper's configuration dialog (General tab), defaulting to `Ctrl+V`.
* **Transient Scope Only:** Activated exclusively when Klipper is invoked as a transient popup (via `Meta+V` shortcut or system tray icon click), where dismissing the popup naturally returns focus to the active target window.
* **Safe Timing & Focus Transition:** Ensure key injection occurs only after the Klipper popup has unmapped and the target application window has regained active input focus.
* **Upstream Quality:** Follow KDE coding standards (LGPL/GPL license headers, modern C++17/20, ECM conventions) suitable for an upstream Merge Request.

### Non-Goals
* Modifying KWin compositor internals (the feature must work with existing KWin releases).
* Triggering auto-paste when the widget is permanently embedded/pinned on the desktop wallpaper (where there is no underlying active window).
* Attempting to introspect or guess terminal state automatically (addressed instead by user-configurable key combination).

### Anti-Goals
* Hardcoding `Ctrl+V` without user configurability.
* Executing external shell processes or data engines from QML.
* Introducing privileged kernel device requirements (`/dev/uinput`).

---

## 3. Approaches Considered & Trade-Offs

1. **Approach 1: In-process Klipper Helper using `org_kde_kwin_fake_input` (Wayland) + `XTest` (X11) [SELECTED]**
   * *Architecture:* Autocontained inside `libklipper` (`plasma-workspace/klipper`). Compiles a helper class `SyntheticKeyboard`. In Wayland, it binds to `org_kde_kwin_fake_input` via `plasma-wayland-protocols`; in X11, it uses `XTestFakeKeyEvent`.
   * *Trade-Off:* Self-contained inside a single repository (`plasma-workspace`), clean to review and test. Avoids creating unnecessary external D-Bus daemons.

2. **Approach 2: KWin Compositor DBus Action**
   * *Architecture:* Add a DBus method to KWin to paste into active window.
   * *Trade-Off:* Requires modifying two repositories simultaneously (`kwin` and `plasma-workspace`), making review and upstream acceptance significantly harder.

3. **Approach 3: Generic `libei` / Portal Session**
   * *Architecture:* Use generic freedesktop RemoteDesktop portal or EIS socket.
   * *Trade-Off:* Heavy overhead and complex session negotiation intended for sandboxed Flatpaks; unnecessary for core Plasma workspace daemons.

---

## 4. Technical Architecture & Components

### 4.1 Configuration Schema (`klipper.kcfg` & `configdialog.cpp`)
* **`klipper.kcfg`:**
  * `PasteOnSelect` (Bool, default: `false`, label: *"Paste selected item automatically"*).
  * `PasteKeySequence` (KeySequence, default: `Qt::CTRL | Qt::Key_V`, label: *"Key combination to paste"*).
* **`configdialog.cpp`:**
  * Add a checkbox for `PasteOnSelect` under the General settings group.
  * Add a `KKeySequenceWidget` next to/under it, disabled when `PasteOnSelect` is unchecked.

### 4.2 Wayland Protocol Generation (`CMakeLists.txt`)
* In `klipper/CMakeLists.txt`:
  ```cmake
  qt_generate_wayland_protocol_client_sources(klipper
      PRIVATE_CODE
      FILES
          "${PLASMA_WAYLAND_PROTOCOLS_DIR}/plasma-shell.xml"
          "${PLASMA_WAYLAND_PROTOCOLS_DIR}/fake-input.xml"
  )
  ```
* In `org.kde.klipper.desktop` and `org.kde.plasmashell.desktop`:
  * Ensure `org_kde_kwin_fake_input` is included in `X-KDE-Wayland-Interfaces`.

### 4.3 Synthetic Input Engine (`SyntheticKeyboard`)
A dedicated helper class compiled into `libklipper`:
* **Interface:**
  ```cpp
  class SyntheticKeyboard : public QObject {
      Q_OBJECT
  public:
      static SyntheticKeyboard *self();
      void sendKeySequence(const QKeySequence &seq);
  };
  ```
* **Wayland Implementation:**
  * Wraps `QtWayland::org_kde_kwin_fake_input`.
  * Authenticates as `"klipper"`, reason: `"Paste on select"`.
  * Converts `QKeySequence` keys and modifiers to Linux keycodes (`linux/input-event-codes.h`, e.g., `KEY_LEFTCTRL`, `KEY_LEFTSHIFT`, `KEY_V`).
  * Emits press events for modifiers, press event for key, release event for key, release events for modifiers.
* **X11 Implementation:**
  * Converts keys/modifiers to X11 keycodes via `XKeysymToKeycode`.
  * Emits events using `XTestFakeKeyEvent`.

### 4.4 Triggering and Timing
* In `DeclarativeHistoryModel` (`declarativehistorymodel.cpp`):
  * Expose an invokable or parameter in `moveToTop` (or `selectAndPaste(uuid)`) that marks whether the selection came from a popup.
  * In `KlipperPopup.qml`, `onItemSelected` already invokes `dialogItem.requestHidePopup()`.
  * When `moveToTop()` is invoked with `pasteOnSelect` enabled:
    1. Update the system clipboard immediately.
    2. Start a single-shot timer (150ms) to allow the popup window to unmap and KWin to restore input focus to the target window.
    3. On timer expiration, dispatch `SyntheticKeyboard::self()->sendKeySequence(KlipperSettings::pasteKeySequence())`.

---

## 5. Summary of Architectural Decisions
1. **In-process Architecture:** Implemented directly within `libklipper` / `plasmashell` without extra D-Bus daemons.
2. **QML/C++ Boundary:** `KlipperPopup.qml` continues to handle popup hiding via its native signal; C++ orchestrates the delay and key injection.
3. **Transient Popup Scope:** Auto-paste triggers only when Klipper is displayed as a transient popup window, ignoring selections on embedded desktop widgets.
