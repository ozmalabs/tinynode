# TinyNode Setup — Raspberry Pi

## Supported models

| Model | USB OTG port | Notes |
|---|---|---|
| Pi Zero / Zero W | Micro-USB (middle port) | Best option — compact and cheap |
| Pi Zero 2 W | Micro-USB | Same, faster CPU |
| Pi 4 Model B | USB-C (power port) | Works, but needs powered hub for peripherals |
| Pi 5 | Not supported | No USB OTG in device mode |
| Pi 3 A+/B+ | Micro-USB | Requires `dwc2` overlay; limited to USB 2.0 |

**Recommended for this use case:** Pi Zero 2 W. Add a USB-Ethernet hat (e.g., Waveshare USB to Ethernet) for wired network on the isolated KVM subnet.

## Prerequisites

- Raspberry Pi OS Lite (Bookworm, 64-bit recommended for Zero 2 W)
- Data-capable micro-USB / USB-C cable → target host PC
- USB-Ethernet adapter (Zero / Zero 2 W) or Cat6 into the built-in port (Pi 4)

## 1. Enable USB OTG (device mode)

Edit `/boot/firmware/config.txt` (Bookworm) or `/boot/config.txt` (older):

```sh
sudo nano /boot/firmware/config.txt
```

Add at the end:

```
dtoverlay=dwc2
```

Edit `/boot/firmware/cmdline.txt` and add `modules-load=dwc2` after `rootwait` (on the same line, space-separated):

```
... rootwait modules-load=dwc2 ...
```

Reboot:

```sh
sudo reboot
```

## 2. Verify the UDC is present

```sh
ls /sys/class/udc
# Pi Zero/Zero 2 W:  20980000.usb
# Pi 4:             fe980000.usb
```

If empty, the overlay didn't load — double-check `/boot/firmware/config.txt` has `dtoverlay=dwc2` and that you edited the correct file for your OS version.

## 3. Verify configfs gadget support

```sh
zcat /proc/config.gz | grep -E 'CONFIG_USB_GADGET|CONFIG_USB_CONFIGFS'
# Should show both as 'y' or 'm'
```

Raspberry Pi OS Lite ships with these enabled. If missing, use the official kernel — don't build a custom one.

## 4. Install and run

Same as the Duo S from this point:

```sh
sudo -i
mkdir -p /opt/ozma/tinynode
git clone https://github.com/ozmalabs/tinynode.git /opt/ozma/tinynode
chmod +x /opt/ozma/tinynode/gadget/setup_gadget.sh
chmod +x /opt/ozma/tinynode/gadget/teardown_gadget.sh

# Set up the gadget
/opt/ozma/tinynode/gadget/setup_gadget.sh

# Verify
ls -l /dev/hidg*
# → /dev/hidg0 (keyboard), /dev/hidg1 (mouse)

# Start the listener
python3 /opt/ozma/tinynode/node/listener.py --debug
```

## 5. Install as systemd services

```sh
cp /opt/ozma/tinynode/node/ozma-gadget.service   /etc/systemd/system/
cp /opt/ozma/tinynode/node/ozma-tinynode.service /etc/systemd/system/
systemctl daemon-reload
systemctl enable --now ozma-gadget.service
systemctl enable --now ozma-tinynode.service
```

## Pi-specific notes

- **Pi Zero / Zero 2 W:** Only one USB port (the OTG port). If you need a keyboard for debugging, use SSH over WiFi or a USB hub between the Pi and the host (the gadget still enumerates).
- **Pi 4 USB-C OTG:** The Pi 4 power port doubles as OTG, but the board needs a data-capable USB-C cable and a host that supplies power separately or a powered USB-C hub. Easier to just use a Pi Zero 2 W.
- **Network on Zero/Zero 2 W:** The USB-Ethernet hat should appear as `eth0`. Configure a static IP on `10.0.100.x` for the isolated KVM subnet.

## Troubleshooting

| Symptom | Check |
|---|---|
| `/sys/class/udc` empty after reboot | `dtoverlay=dwc2` in wrong config file, or cmdline.txt not updated |
| Host sees "Unknown USB device" | Replug after gadget is set up; or check `dmesg` on the Pi |
| `hid.keyboard` function fails to create | Kernel missing `CONFIG_USB_HID_GADGET` — use the stock RPi kernel |
