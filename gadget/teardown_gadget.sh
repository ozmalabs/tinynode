#!/bin/sh
# Detaches and removes the Ozma USB gadget.
# Run before reconfiguring or during shutdown.

set -e

GADGET_NAME="ozma"
GADGET_DIR="/sys/kernel/config/usb_gadget/${GADGET_NAME}"

if [ ! -d "${GADGET_DIR}" ]; then
    echo "Gadget '${GADGET_NAME}' not found — nothing to do."
    exit 0
fi

# Unbind from UDC
echo "" > "${GADGET_DIR}/UDC" 2>/dev/null || true

# Remove symlinks from config
rm -f "${GADGET_DIR}/configs/c.1/hid.keyboard"
rm -f "${GADGET_DIR}/configs/c.1/hid.mouse"

# Remove config strings and config
rmdir "${GADGET_DIR}/configs/c.1/strings/0x409" 2>/dev/null || true
rmdir "${GADGET_DIR}/configs/c.1" 2>/dev/null || true

# Remove functions
rmdir "${GADGET_DIR}/functions/hid.keyboard" 2>/dev/null || true
rmdir "${GADGET_DIR}/functions/hid.mouse" 2>/dev/null || true

# Remove gadget strings and gadget
rmdir "${GADGET_DIR}/strings/0x409" 2>/dev/null || true
rmdir "${GADGET_DIR}" 2>/dev/null || true

echo "Gadget '${GADGET_NAME}' removed."
