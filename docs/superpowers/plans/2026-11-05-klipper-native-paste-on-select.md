# Klipper Native "Paste on Select" Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement a 100% native "Paste on Select" feature in KDE Plasma 6 Klipper (`plasma-workspace`) with configurable shortcut keys and zero external dependencies.

**Architecture:** A native `SyntheticKeyboard` helper inside `libklipper` binds to the `org_kde_kwin_fake_input` Wayland protocol (and `XTest` on X11). Klipper settings schema (`klipper.kcfg`) adds `PasteOnSelect` and `PasteKeySequence`, configured via a checkbox and `KKeySequenceWidget` in `ConfigDialog`. `DeclarativeHistoryModel` exposes `selectAndPaste` to orchestrate popup dismissal and keystroke injection with focus transition timing.

**Tech Stack:** C++17/20, Qt 6, KDE Frameworks 6 (KF6), CMake / Extra CMake Modules (ECM), Wayland client protocols (`plasma-wayland-protocols`), XCB / XTest.

**Spec:** [docs/superpowers/specs/2026-11-05-klipper-native-paste-on-select-design.md](docs/superpowers/specs/2026-11-05-klipper-native-paste-on-select-design.md)

## Global Constraints
* Zero external CLI runtime dependencies (no `ydotool`, `wtype`, or `xdotool`).
* Zero `/dev/uinput` permissions or user daemons needed.
* Compatible with modern KDE Plasma 6.8.x and Qt 6.
* Configurable key combination via `KKeySequenceWidget`, defaulting to `Qt::CTRL | Qt::Key_V`.
* Scope limited strictly to transient popup invocations (`Meta+V` / tray click), ignoring embedded desktop widgets.

## Review Focus
1. **Modifier Release Guarantee:** If a user specifies a complex shortcut (e.g. `Ctrl+Shift+V`), modifier keys must always be released even if injection of the main key fails or encounters an invalid keysym.
2. **Wayland Global Availability:** If KWin has not yet announced or has revoked `org_kde_kwin_fake_input`, calling `sendKeySequence` must fail gracefully without crashing plasmashell.
3. **Empty / Invalid KeySequence:** If the user clears the shortcut in settings, `sendKeySequence` must be a no-op and not emit corrupted or zero keycodes.
4. **Popup Focus Race:** Keystroke dispatch must only trigger after the popup window has closed so the target client receives the input events.
5. **KConfig Migration / Defaults:** If upgrading from an older config without `PasteKeySequence`, it must default cleanly to `Ctrl+V`.

---

### Task 1: Configuration Schema & Settings UI

**Files:**
- Modify: `cpp/klipper.kcfg`
- Modify: `cpp/configdialog.h`
- Modify: `cpp/configdialog.cpp`
- Test: `cpp/autotests/test_settings.cpp` (or manual test fixture)

**Interfaces:**
- Produces:
  - `bool KlipperSettings::pasteOnSelect()`
  - `QKeySequence KlipperSettings::pasteKeySequence()`
  - UI widgets `m_pasteOnSelectCb` and `m_pasteKeySequenceWidget` linked via KConfig dialog.

- [ ] **Step 1: Define settings entries in `cpp/klipper.kcfg`**
Add `PasteOnSelect` (Bool, default false) and `PasteKeySequence` (KeySequence, default `Ctrl+V`) to `cpp/klipper.kcfg`.

- [ ] **Step 2: Add widgets to `GeneralWidget` in `cpp/configdialog.h`**
Declare `QCheckBox *m_pasteOnSelectCb` and `KKeySequenceWidget *m_pasteKeySequenceWidget`.

- [ ] **Step 3: Integrate widgets and layout in `cpp/configdialog.cpp`**
In `GeneralWidget::GeneralWidget(QWidget *parent)`:
1. Instantiate `m_pasteOnSelectCb` connected to `kcfg_PasteOnSelect`.
2. Instantiate `m_pasteKeySequenceWidget` connected to `kcfg_PasteKeySequence`.
3. Connect `m_pasteOnSelectCb::toggled` to enable/disable `m_pasteKeySequenceWidget`.
4. Add them to the form layout in the General tab.

- [ ] **Step 4: Verify settings compilation**
Compile KConfig generated headers to verify `KlipperSettings::pasteOnSelect()` and `KlipperSettings::pasteKeySequence()` are generated with correct signatures.

- [ ] **Step 5: Commit**
```bash
git add cpp/klipper.kcfg cpp/configdialog.h cpp/configdialog.cpp
git commit -m "feat(config): add PasteOnSelect and PasteKeySequence settings and GUI"
```

---

### Task 2: Native Synthetic Keyboard Engine (`SyntheticKeyboard`)

**Files:**
- Create: `cpp/synthetickeyboard.h`
- Create: `cpp/synthetickeyboard.cpp`
- Create: `cpp/keymapper.h`
- Create: `cpp/keymapper.cpp`
- Modify: `cpp/CMakeLists.txt`
- Create: `cpp/autotests/test_keymapper.cpp`

**Interfaces:**
- Consumes:
  - `QKeySequence` from `KlipperSettings::pasteKeySequence()`
  - Wayland protocol `org_kde_kwin_fake_input` from `plasma-wayland-protocols/fake-input.xml`
  - `XTestFakeKeyEvent` via `XCB::XCB` / `XTest`
- Produces:
  - `SyntheticKeyboard::self()->sendKeySequence(const QKeySequence &seq)`
  - `KeyMapper::toLinuxKey(Qt::Key key) -> quint32`

- [ ] **Step 1: Write unit test for `KeyMapper`**
Write `cpp/autotests/test_keymapper.cpp` testing translation of `Qt::Key_V`, `Qt::Key_C`, `Qt::Key_Insert`, `Qt::Key_Control`, `Qt::Key_Shift` to Linux `KEY_V`, `KEY_C`, `KEY_INSERT`, `KEY_LEFTCTRL`, `KEY_LEFTSHIFT` from `linux/input-event-codes.h`.

- [ ] **Step 2: Implement `KeyMapper` in `cpp/keymapper.h` and `cpp/keymapper.cpp`**
Implement the mapping functions for keys and modifiers with fallback for unknown keys.

- [ ] **Step 3: Run test to verify key mapping passes**
Build and run `test_keymapper` verifying all mapped combinations pass.

- [ ] **Step 4: Implement `SyntheticKeyboard` in `cpp/synthetickeyboard.h` and `cpp/synthetickeyboard.cpp`**
1. Implement Wayland backend:
   - Use `QWaylandClientExtensionTemplate` to bind to `org_kde_kwin_fake_input`.
   - Call `authenticate(QStringLiteral("klipper"), QStringLiteral("Paste on select"))`.
   - Implement `sendKeySequence(const QKeySequence &seq)`:
     - Decompose `QKeySequence` into modifiers (Ctrl, Shift, Alt, Meta) and key.
     - Emit press events for modifiers, then press and release for the key, then release events for modifiers in reverse order.
2. Implement X11 fallback backend:
   - Check `KWindowSystem::isPlatformX11()`.
   - Convert keys to X11 keycodes using `XKeysymToKeycode` and emit press/release via `XTestFakeKeyEvent`.

- [ ] **Step 5: Update CMakeLists to generate protocol and link dependencies**
In `cpp/CMakeLists.txt`:
```cmake
qt_generate_wayland_protocol_client_sources(klipper
    PRIVATE_CODE
    FILES
        "${PLASMA_WAYLAND_PROTOCOLS_DIR}/plasma-shell.xml"
        "${PLASMA_WAYLAND_PROTOCOLS_DIR}/fake-input.xml"
)
```
Add `synthetickeyboard.cpp` and `keymapper.cpp` to `libklipper_common_SRCS`.

- [ ] **Step 6: Commit**
```bash
git add cpp/synthetickeyboard.* cpp/keymapper.* cpp/CMakeLists.txt cpp/autotests/
git commit -m "feat(input): implement native SyntheticKeyboard with Wayland and X11 backends"
```

---

### Task 3: Declarative History Model Integration & Desktop Interface Permissions

**Files:**
- Modify: `cpp/declarativehistorymodel.h`
- Modify: `cpp/declarativehistorymodel.cpp`
- Modify: `org.kde.klipper.desktop` (and check `org.kde.plasmashell.desktop`)
- Test: `cpp/autotests/test_pasteonselect.cpp`

**Interfaces:**
- Consumes:
  - `SyntheticKeyboard::self()->sendKeySequence(...)`
  - `KlipperSettings::pasteOnSelect()`
  - `KlipperSettings::pasteKeySequence()`
- Produces:
  - `Q_INVOKABLE void DeclarativeHistoryModel::selectAndPaste(const QString &uuid);`
  - `Q_PROPERTY(bool pasteOnSelect READ pasteOnSelect NOTIFY pasteOnSelectChanged)`

- [ ] **Step 1: Add `selectAndPaste` method to `DeclarativeHistoryModel`**
In `cpp/declarativehistorymodel.h` and `declarativehistorymodel.cpp`:
1. Expose `pasteOnSelect` property.
2. Implement `selectAndPaste(const QString &uuid)`:
   - Invoke `moveToTop(uuid)`.
   - If `KlipperSettings::pasteOnSelect()` is true:
     - Schedule `QTimer::singleShot(150, this, []() { SyntheticKeyboard::self()->sendKeySequence(KlipperSettings::pasteKeySequence()); });`

- [ ] **Step 2: Declare Wayland interface in desktop files**
Add `org_kde_kwin_fake_input` to `X-KDE-Wayland-Interfaces` in `org.kde.klipper.desktop` (and documentation notes for `org.kde.plasmashell.desktop`).

- [ ] **Step 3: Update `ClipboardMenu.qml` to invoke `selectAndPaste`**
In `qml/ClipboardMenu.qml`, replace the legacy `pasteExecutor` / `Plasma5Support.DataSource` hack with:
```qml
onItemSelected: uuid => {
    if (model.pasteOnSelect) {
        model.selectAndPaste(uuid);
    } else {
        model.moveToTop(uuid);
    }
}
```
Remove all `Plasma5Support` imports and the `DataSource` item.

- [ ] **Step 4: Commit**
```bash
git add cpp/declarativehistorymodel.* org.kde.klipper.desktop qml/ClipboardMenu.qml
git commit -m "feat(history): wire selectAndPaste into DeclarativeHistoryModel and update QML"
```

---

### Task 4: Upstream Patch Generation & Automated Installer Refactor

**Files:**
- Create: `patches/0001-klipper-native-paste-on-select.patch`
- Modify: `install.sh`
- Modify: `README.md`

**Interfaces:**
- Produces:
  - Git format-patch suitable for direct upstream Merge Request on KDE Invent (`plasma/plasma-workspace`).
  - Safe, non-destructive `install.sh` that applies git patches to sparse-checked-out Plasma source matching installed version, builds with Ninja, and installs locally without `ydotoold`.

- [ ] **Step 1: Generate clean unified patch for Plasma 6.8.x**
Generate `patches/0001-klipper-native-paste-on-select.patch` against upstream KDE `plasma-workspace.git` v6.8.2.

- [ ] **Step 2: Refactor `install.sh`**
Update `install.sh`:
1. Remove `ydotool` package requirement and systemd service enablement.
2. Apply `git apply patches/0001-klipper-native-paste-on-select.patch` against the sparse-checkout build directory.
3. Build `klipper` with CMake & Ninja.
4. Deploy the compiled `libklipper.so` and local QML plugin override.
5. Restart `plasma-plasmashell.service`.

- [ ] **Step 3: Update `README.md`**
Document the native architecture, configuration options, upstream contribution status, and usage instructions.

- [ ] **Step 4: End-to-end verification**
Execute compilation and verify clean build, no compiler warnings or link errors.

- [ ] **Step 5: Commit**
```bash
git add patches/ install.sh README.md
git commit -m "chore(installer): modernize installer and produce upstream patch"
```
