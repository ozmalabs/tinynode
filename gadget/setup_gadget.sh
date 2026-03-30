#!/bin/sh
# Sets up a composite USB gadget via configfs:
#   - HID keyboard (boot protocol, 8-byte reports)
#   - HID absolute pointing device (touchscreen/digitizer, 6-byte reports)
#
# Run as root on the node. Re-run after reboot (or install as a systemd service).
#
# Usage: ./setup_gadget.sh [serial_number]
#
# Tested on: Milk-V Duo S (SG2000, Linux 5.10)

set -e

GADGET_NAME="ozma"
GADGET_DIR="/sys/kernel/config/usb_gadget/${GADGET_NAME}"
SERIAL="${1:-OZMA0001}"

# Vendor/product IDs — use a private-use range for development
USB_VENDOR="0x1d6b"   # Linux Foundation
USB_PRODUCT="0x0104"  # Multifunction Composite Gadget

# Mount configfs if not already mounted
if ! mountpoint -q /sys/kernel/config; then
    mount -t configfs none /sys/kernel/config
fi

# Load required modules
modprobe libcomposite 2>/dev/null || true

# --- Create gadget ---
mkdir -p "${GADGET_DIR}"
echo "${USB_VENDOR}" > "${GADGET_DIR}/idVendor"
echo "${USB_PRODUCT}" > "${GADGET_DIR}/idProduct"
echo "0x0100" > "${GADGET_DIR}/bcdDevice"
echo "0x0200" > "${GADGET_DIR}/bcdUSB"

# Strings
mkdir -p "${GADGET_DIR}/strings/0x409"
echo "OzmaLabs"     > "${GADGET_DIR}/strings/0x409/manufacturer"
echo "Ozma TinyNode" > "${GADGET_DIR}/strings/0x409/product"
echo "${SERIAL}"    > "${GADGET_DIR}/strings/0x409/serialnumber"

# --- HID Keyboard (boot protocol) ---
# 8-byte report: [modifier, reserved, key1, key2, key3, key4, key5, key6]
mkdir -p "${GADGET_DIR}/functions/hid.keyboard"
echo 1    > "${GADGET_DIR}/functions/hid.keyboard/protocol"    # keyboard
echo 1    > "${GADGET_DIR}/functions/hid.keyboard/subclass"    # boot interface
echo 8    > "${GADGET_DIR}/functions/hid.keyboard/report_length"
printf '\x05\x01\x09\x06\xa1\x01\x05\x07\x19\xe0\x29\xe7\x15\x00\x25\x01\x75\x01\x95\x08\x81\x02\x95\x01\x75\x08\x81\x03\x95\x05\x75\x01\x05\x08\x19\x01\x29\x05\x91\x02\x95\x01\x75\x03\x91\x03\x95\x06\x75\x08\x15\x00\x25\x65\x05\x07\x19\x00\x29\x65\x81\x00\xc0' \
    > "${GADGET_DIR}/functions/hid.keyboard/report_desc"

# --- HID Absolute Pointer (digitizer/touchscreen) ---
# Reports absolute X (0-32767), absolute Y (0-32767), buttons, scroll
# 6-byte report: [buttons(1), x_lo, x_hi, y_lo, y_hi, scroll(1)]
mkdir -p "${GADGET_DIR}/functions/hid.mouse"
echo 0    > "${GADGET_DIR}/functions/hid.mouse/protocol"       # none (not boot)
echo 0    > "${GADGET_DIR}/functions/hid.mouse/subclass"
echo 6    > "${GADGET_DIR}/functions/hid.mouse/report_length"
printf '\x05\x01\x09\x02\xa1\x01\x09\x01\xa1\x00\x05\x09\x19\x01\x29\x05\x15\x00\x25\x01\x95\x05\x75\x01\x81\x02\x95\x01\x75\x03\x81\x03\x05\x01\x09\x30\x09\x31\x15\x00\x26\xff\x7f\x75\x10\x95\x02\x81\x02\x09\x38\x15\x81\x25\x7f\x75\x08\x95\x01\x81\x06\xc0\xc0' \
    > "${GADGET_DIR}/functions/hid.mouse/report_desc"

# --- USB Serial (ACM) ---
# Presents a virtual serial port to the target machine (/dev/ttyACM0 on the host).
# The node reads this for console output — zero extra hardware, zero config on the
# target beyond adding a console= kernel parameter.
#
# Target machine setup:
#   Linux:   console=ttyACM0,115200  (add to GRUB_CMDLINE_LINUX in /etc/default/grub)
#   FreeBSD: console="ucom0" in /boot/loader.conf
#   Or just: echo "Hello from target" > /dev/ttyACM0
#
# One-liner to enable console on the target (run as root):
#   grep -q ttyACM0 /etc/default/grub || \
#     sed -i 's/GRUB_CMDLINE_LINUX="\(.*\)"/GRUB_CMDLINE_LINUX="\1 console=ttyACM0,115200"/' /etc/default/grub && \
#     update-grub && echo "Serial console enabled — reboot to activate"
#
mkdir -p "${GADGET_DIR}/functions/acm.serial0"
# No additional configuration needed — ACM just works

# --- Configuration ---
mkdir -p "${GADGET_DIR}/configs/c.1"
echo 250 > "${GADGET_DIR}/configs/c.1/MaxPower"
mkdir -p "${GADGET_DIR}/configs/c.1/strings/0x409"
echo "Ozma HID+Serial" > "${GADGET_DIR}/configs/c.1/strings/0x409/configuration"

# Link functions into configuration
ln -sf "${GADGET_DIR}/functions/hid.keyboard" "${GADGET_DIR}/configs/c.1/"
ln -sf "${GADGET_DIR}/functions/hid.mouse"    "${GADGET_DIR}/configs/c.1/"
ln -sf "${GADGET_DIR}/functions/acm.serial0"  "${GADGET_DIR}/configs/c.1/"

# --- Bind to UDC (USB Device Controller) ---
UDC="$(ls /sys/class/udc | head -1)"
if [ -z "${UDC}" ]; then
    echo "ERROR: No UDC found. Is USB OTG connected and the cable data-capable?" >&2
    exit 1
fi
echo "${UDC}" > "${GADGET_DIR}/UDC"

echo "Gadget '${GADGET_NAME}' bound to UDC '${UDC}'"
echo "  /dev/hidg0  → keyboard"
echo "  /dev/hidg1  → mouse"
echo "  /dev/ttyGS0 → serial console (target sees /dev/ttyACM0)"
echo ""
echo "To enable serial console on the target machine (Linux), run:"
echo "  sudo sed -i 's/GRUB_CMDLINE_LINUX=\"\\(.*\\)\"/GRUB_CMDLINE_LINUX=\"\\1 console=ttyACM0,115200\"/' /etc/default/grub"
echo "  sudo update-grub && sudo reboot"
