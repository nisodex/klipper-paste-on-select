#!/usr/bin/env bash

# Exit immediately if a command exits with a non-zero status
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=== Starting Native Klipper Paste on Select Installation / Update ==="

# 1. Install Build Dependencies
echo "Ensuring required build dependencies are installed..."
pkexec pacman -S --needed --noconfirm extra-cmake-modules plasma-wayland-protocols cmake ninja git

# 2. Detect Installed Plasma-Workspace Version
PLASMA_VER=$(pacman -Q plasma-workspace | awk '{print $2}' | cut -d'-' -f1)
echo "Detected plasma-workspace version: ${PLASMA_VER}"

# 3. Clone & Sparse Checkout Klipper
BUILD_DIR="${SCRIPT_DIR}/plasma-workspace-src"
if [ -d "${BUILD_DIR}" ]; then
    echo "Cleaning up previous build directory..."
    rm -rf "${BUILD_DIR}"
fi

echo "Cloning plasma-workspace (sparse checkout for klipper, branch v${PLASMA_VER})..."
git clone --depth 1 --branch "v${PLASMA_VER}" --filter=blob:none --sparse https://invent.kde.org/plasma/plasma-workspace.git "${BUILD_DIR}"
cd "${BUILD_DIR}"
git sparse-checkout set klipper shell
cd "${SCRIPT_DIR}"

# 4. Apply Native Paste-on-Select Patch
PATCH_FILE="${SCRIPT_DIR}/patches/0001-klipper-native-paste-on-select.patch"
echo "Applying native patch (${PATCH_FILE})..."
cd "${BUILD_DIR}"
git apply --verbose "${PATCH_FILE}"

# Adjust root CMakeLists to build only klipper
echo "Adjusting build configuration for standalone klipper build..."
sed -i '/add_subdirectory(login-sessions)/,$d' CMakeLists.txt
echo "add_subdirectory(klipper)" >> CMakeLists.txt

# 5. Build Klipper
echo "Building native Klipper library and declarative plugin..."
mkdir -p build
cd build
cmake .. -GNinja -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTING=OFF
ninja
cd "${SCRIPT_DIR}"

# 6. Install the core libklipper.so library
ORIGINAL_LIB="/usr/lib/libklipper.so.${PLASMA_VER}"
BACKUP_LIB="/usr/lib/libklipper.so.${PLASMA_VER}.orig"

if [ ! -f "${BACKUP_LIB}" ]; then
    if [ -f "/usr/lib/libklipper.so.${PLASMA_VER}.bak.bak" ]; then
        echo "Adopting existing original backup..."
        pkexec cp "/usr/lib/libklipper.so.${PLASMA_VER}.bak.bak" "${BACKUP_LIB}"
    elif [ -f "${ORIGINAL_LIB}" ]; then
        echo "Backing up original system library to ${BACKUP_LIB}..."
        pkexec cp "${ORIGINAL_LIB}" "${BACKUP_LIB}"
    fi
fi

echo "Installing modified libklipper..."
pkexec cp "${BUILD_DIR}/build/bin/libklipper.so.${PLASMA_VER}" "${ORIGINAL_LIB}"
pkexec ln -sfn "libklipper.so.${PLASMA_VER}" /usr/lib/libklipper.so.6

# 7. Setup QML Overrides
OVERRIDE_PATH="${HOME}/.local/share/klipper-override/org/kde/plasma/private/clipboard"
echo "Setting up local QML overrides in ${OVERRIDE_PATH}..."
mkdir -p "${OVERRIDE_PATH}"

# Copy system QML files as a fallback
cp -r /usr/lib/qt6/qml/org/kde/plasma/private/clipboard/* "${OVERRIDE_PATH}/"
rm -f "${OVERRIDE_PATH}/"*.bak

# Remove 'prefer' in qmldir to prioritize local files
sed -i '/prefer/d' "${OVERRIDE_PATH}/qmldir"

# Copy our modified QML and compiled plugin
cp "${SCRIPT_DIR}/qml/ClipboardMenu.qml" "${OVERRIDE_PATH}/"
cp "${BUILD_DIR}/build/bin/org/kde/plasma/private/clipboard/libklipperplugin.so" "${OVERRIDE_PATH}/"

# 8. Install Systemd User Override
SYSTEMD_OVERRIDE_DIR="${HOME}/.config/systemd/user/plasma-plasmashell.service.d"
echo "Installing systemd environment override for plasmashell..."
mkdir -p "${SYSTEMD_OVERRIDE_DIR}"
cp "${SCRIPT_DIR}/systemd/override.conf" "${SYSTEMD_OVERRIDE_DIR}/"

# 9. Configure Wayland Protocol Permission for Plasmashell
USER_APPS_DIR="${HOME}/.local/share/applications"
mkdir -p "${USER_APPS_DIR}"
PLASMASHELL_DESKTOP="${USER_APPS_DIR}/org.kde.plasmashell.desktop"
if [ ! -f "${PLASMASHELL_DESKTOP}" ] && [ -f "/usr/share/applications/org.kde.plasmashell.desktop" ]; then
    cp "/usr/share/applications/org.kde.plasmashell.desktop" "${PLASMASHELL_DESKTOP}"
fi

if [ -f "${PLASMASHELL_DESKTOP}" ]; then
    if ! grep -q "org_kde_kwin_fake_input" "${PLASMASHELL_DESKTOP}"; then
        echo "Adding org_kde_kwin_fake_input permission to ${PLASMASHELL_DESKTOP}..."
        sed -i 's/X-KDE-Wayland-Interfaces=\(.*\)/X-KDE-Wayland-Interfaces=\1,org_kde_kwin_fake_input/' "${PLASMASHELL_DESKTOP}"
    fi
fi

# 10. Reload and Restart Plasmashell
echo "Reloading systemd user daemon and restarting plasmashell..."
systemctl --user daemon-reload
rm -rf "${HOME}/.cache/plasmashell/qmlcache"
systemctl --user restart plasma-plasmashell.service

echo "=== Native installation complete successfully! ==="
