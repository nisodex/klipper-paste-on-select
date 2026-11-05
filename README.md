# Native Klipper Paste on Select (KDE Plasma 6)

This repository implements a **100% native "Paste on Select"** feature for KDE Plasma 6's clipboard manager (**Klipper**) under both Wayland and X11 sessions.

It addresses [KDE Bug #427214 ("Add ability to paste Klipper's contents on selection")](https://bugs.kde.org/show_bug.cgi?id=427214) with a clean, upstream-ready architecture that eliminates third-party input injection tools (`ydotool`, `wtype`, `xdotool`) and deprecated Plasma 5 dataengines.

---

## 🛠️ How It Works

1. **Native Synthetic Input Engine (`SyntheticKeyboard`)**:
   * **Wayland**: Binds to KWin's privileged native Wayland interface `org_kde_kwin_fake_input` (defined in `plasma-wayland-protocols/fake-input.xml`). Keystrokes are injected directly into the compositor without needing kernel-level virtual devices or `/dev/uinput` daemons.
   * **X11**: Dispatches synthetic key events using standard `XTestFakeKeyEvent`.

2. **Configurable Shortcut UI**:
   * Fully integrated into Klipper's native Configure dialog (General tab).
   * Checkbox: *"Paste selected item automatically"* (disabled by default).
   * Shortcut picker: Uses `KKeySequenceWidget` allowing users to configure the simulated key combination (defaults to `Ctrl+V`, but can be customized to `Ctrl+Shift+V` for terminal users or any other shortcut).

3. **Safe Timing & Transient Scope**:
   * Scoped strictly to transient popup invocations (`Meta+V` or clicking the system tray icon).
   * Selecting an item unmaps the popup and schedules key injection after a brief delay, ensuring the underlying target application window has resumed active input focus before keystrokes arrive.

---

## 📂 Repository Structure

* `patches/0001-klipper-native-paste-on-select.patch`: Unified git patch against upstream KDE `plasma-workspace` (`klipper`).
* `cpp/`: Native C++ implementation sources.
  * `synthetickeyboard.h` / `synthetickeyboard.cpp`: Native Wayland and X11 synthetic keystroke generator.
  * `keymapper.h` / `keymapper.cpp`: Bidirectional mapping between `Qt::Key` / modifiers and Linux/X11 keycodes.
  * `klipper.kcfg` / `klippersettings.kcfgc`: Settings schema definitions (`PasteOnSelect`, `PasteKeySequence`).
  * `configdialog.cpp` / `configdialog.h`: Native configuration GUI layout and widgets.
  * `declarativehistorymodel.cpp` / `declarativehistorymodel.h`: QML bindings and `selectAndPaste()` orchestration.
  * `autotests/`: Unit tests for key mapping and setting serialization.
* `qml/ClipboardMenu.qml`: Updated Plasma 6.8+ QML menu invoking `selectAndPaste()`.
* `install.sh`: Automated build and deployment script for local testing.

---

## 🚀 Local Installation

To build and install on your Arch / CachyOS Linux system running KDE Plasma 6:

```bash
git clone https://github.com/nisodex/klipper-paste-on-select.git
cd klipper-paste-on-select
./install.sh
```

### What the installer does:
1. Installs build tools (`extra-cmake-modules`, `plasma-wayland-protocols`, `cmake`, `ninja`).
2. Checks out the exact matching `plasma-workspace` version for your installed Plasma packages.
3. Applies `patches/0001-klipper-native-paste-on-select.patch`.
4. Compiles `libklipper.so` and the declarative plugin using Ninja.
5. Backs up your stock `/usr/lib/libklipper.so.6.x` and installs the patched library.
6. Sets up local QML overrides and systemd environment.
7. Restarts `plasmashell` to apply changes immediately.

---

## 🔄 Uninstallation / Revert to Stock

To completely restore the stock KDE Plasma Klipper library and remove all local overrides:

```bash
./uninstall.sh
```

