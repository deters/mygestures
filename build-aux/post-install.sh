#!/bin/sh

# Post-installation script for mygestures

if [ -z "$DESTDIR" ]; then
    echo "Reloading udev rules..."
    udevadm control --reload-rules 2>/dev/null || true
    udevadm trigger 2>/dev/null || true

    # Determine user to add to input group
    TARGET_USER="${SUDO_USER:-$USER}"
    if [ -n "$TARGET_USER" ] && [ "$TARGET_USER" != "root" ]; then
        if ! groups "$TARGET_USER" 2>/dev/null | grep -q "\binput\b"; then
            echo "Adding user '$TARGET_USER' to 'input' group..."
            usermod -aG input "$TARGET_USER" 2>/dev/null || echo "Please add your user to the 'input' group manually: sudo usermod -aG input $TARGET_USER"
        fi
    fi
fi
