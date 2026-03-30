#!/bin/sh
# Run this ON THE TARGET MACHINE (not the node) to enable serial console
# output over the ozma node's USB connection.
#
# After running this and rebooting, the target machine sends all kernel
# messages, boot logs, and panic backtraces over USB to the ozma node.
# No extra hardware needed — it uses the same USB cable as the HID gadget.
#
# Usage:
#   curl -sL http://ozma-node:7382/setup/serial | sudo sh
#   # or copy this script and run: sudo sh enable_serial_console.sh
#
# What it does:
#   1. Adds console=ttyACM0,115200 to the kernel command line (GRUB)
#   2. Enables a getty on ttyACM0 for interactive serial login
#   3. Updates GRUB and tells you to reboot
#
# Supports: Debian, Ubuntu, Fedora, Arch, RHEL, SUSE, and derivatives.
# For FreeBSD: add console="ucom0" to /boot/loader.conf manually.

set -e

echo "Ozma Serial Console Setup"
echo "========================="
echo ""

# Detect init system and GRUB location
if [ -f /etc/default/grub ]; then
    GRUB_CFG="/etc/default/grub"
elif [ -f /etc/sysconfig/grub ]; then
    GRUB_CFG="/etc/sysconfig/grub"
else
    echo "ERROR: Could not find GRUB config. Add 'console=ttyACM0,115200' to your kernel cmdline manually."
    exit 1
fi

# Check if already configured
if grep -q "ttyACM0" "$GRUB_CFG" 2>/dev/null; then
    echo "Serial console already configured in $GRUB_CFG"
else
    echo "Adding console=ttyACM0,115200 to kernel command line..."
    sed -i 's/GRUB_CMDLINE_LINUX="\(.*\)"/GRUB_CMDLINE_LINUX="\1 console=tty0 console=ttyACM0,115200"/' "$GRUB_CFG"

    # Update GRUB
    if command -v update-grub >/dev/null 2>&1; then
        update-grub
    elif command -v grub2-mkconfig >/dev/null 2>&1; then
        grub2-mkconfig -o /boot/grub2/grub.cfg
    elif command -v grub-mkconfig >/dev/null 2>&1; then
        grub-mkconfig -o /boot/grub/grub.cfg
    else
        echo "WARNING: Could not find grub-mkconfig. Run it manually."
    fi
    echo "GRUB updated."
fi

# Enable a getty on ttyACM0 for interactive serial login
if command -v systemctl >/dev/null 2>&1; then
    echo "Enabling serial getty on ttyACM0..."
    systemctl enable serial-getty@ttyACM0.service 2>/dev/null || true
fi

echo ""
echo "Done! Reboot the target machine to activate serial console."
echo ""
echo "After reboot, the ozma node will capture:"
echo "  - Kernel boot messages"
echo "  - Kernel panics (full backtrace)"
echo "  - System logs (if configured)"
echo "  - Interactive login (if getty enabled)"
echo ""
echo "No extra cables or hardware needed — it uses the existing USB connection."
