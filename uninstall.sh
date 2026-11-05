#!/usr/bin/env bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=== Uninstalling Klipper Paste on Select & Restoring Stock Plasma ==="

PLASMA_VER=$(pacman -Q plasma-workspace 2>/dev/null | awk '{print $2}' | cut -d'-' -f1 || echo "6.8.2")
ORIGINAL_LIB="/usr/lib/libklipper.so.${PLASMA_VER}"
BACKUP_LIB="/usr/lib/libklipper.so.${PLASMA_VER}.orig"

# Restore original system library if backup exists
if [ -f "${BACKUP_LIB}" ]; then
    echo "Restoring original system library from ${BACKUP_LIB}..."
    pkexec cp "${BACKUP_LIB}" "${ORIGINAL_LIB}"
    pkexec ln -sfn "libklipper.so.${PLASMA_VER}" /usr/lib/libklipper.so.6
elif [ -f "/usr/lib/libklipper.so.${PLASMA_VER}.bak.bak" ]; then
    echo "Restoring original system library from .bak.bak..."
    pkexec cp "/usr/lib/libklipper.so.${PLASMA_VER}.bak.bak" "${ORIGINAL_LIB}"
    pkexec ln -sfn "libklipper.so.${PLASMA_VER}" /usr/lib/libklipper.so.6
else
    echo "Warning: No backup found. Reinstalling plasma-workspace via pacman is recommended if you wish to restore stock libklipper."
fi

# Remove QML override
OVERRIDE_PATH="${HOME}/.local/share/klipper-override"
if [ -d "${OVERRIDE_PATH}" ]; then
    echo "Removing QML override directory (${OVERRIDE_PATH})..."
    rm -rf "${OVERRIDE_PATH}"
fi

# Remove systemd user override
SYSTEMD_OVERRIDE="${HOME}/.config/systemd/user/plasma-plasmashell.service.d/override.conf"
if [ -f "${SYSTEMD_OVERRIDE}" ]; then
    echo "Removing systemd user override (${SYSTEMD_OVERRIDE})..."
    rm -f "${SYSTEMD_OVERRIDE}"
    rmdir "${HOME}/.config/systemd/user/plasma-plasmashell.service.d" 2>/dev/null || true
fi

# Remove user desktop override if it exists
USER_DESKTOP="${HOME}/.local/share/applications/org.kde.plasmashell.desktop"
if [ -f "${USER_DESKTOP}" ]; then
    echo "Removing local plasmashell.desktop override..."
    rm -f "${USER_DESKTOP}"
fi

# Restart plasmashell
echo "Reloading systemd user daemon and restarting plasmashell..."
systemctl --user daemon-reload
rm -rf "${HOME}/.cache/plasmashell/qmlcache"
systemctl --user restart plasma-plasmashell.service

echo "=== Restored stock Klipper successfully! ==="
