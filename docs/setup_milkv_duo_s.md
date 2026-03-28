# TinyNode Setup — Milk-V Duo S

## Prerequisites

- Milk-V Duo S 512MB flashed with the official Debian image
- USB-C cable (data-capable) connecting the Duo S to your target host PC
- Cat6 cable connecting the Duo S to your head node / switch

## 1. Flash the Debian image

Download the latest Milk-V Duo S Debian image from the official releases and flash it to a microSD card:

```sh
# On your workstation:
xzcat duos-debian-*.img.xz | sudo dd of=/dev/sdX bs=4M status=progress
sync
```

Boot the Duo S. Default credentials: `root` / `milkv`.

## 2. Verify USB gadget support

```sh
# On the Duo S:
ls /sys/class/udc
# Should show something like: 4340000.usb
# If empty, USB OTG is not initialised — check your cable and kernel config.

zcat /proc/config.gz | grep -E 'CONFIG_USB_GADGET|CONFIG_USB_CONFIGFS'
# Should show both set to 'y' or 'm'
```

## 3. Install the gadget and listener

```sh
# On the Duo S (as root):
mkdir -p /opt/ozma/tinynode
git clone https://github.com/ozmalabs/tinynode.git /opt/ozma/tinynode

chmod +x /opt/ozma/tinynode/gadget/setup_gadget.sh
chmod +x /opt/ozma/tinynode/gadget/teardown_gadget.sh

# Test the gadget setup manually first:
/opt/ozma/tinynode/gadget/setup_gadget.sh

# Verify the gadget devices exist:
ls -l /dev/hidg*
# Should show /dev/hidg0 (keyboard) and /dev/hidg1 (mouse)
```

## 4. Start the listener

```sh
# On the Duo S:
python3 /opt/ozma/tinynode/node/listener.py --debug
```

## 5. Test from the head node

```sh
# On your head node / workstation (with network access to the Duo S):
python3 scripts/test_hid.py <duo-s-ip>
```

You should see the target PC register a keypress and mouse movement.

## 6. Install as systemd services

```sh
# On the Duo S:
cp /opt/ozma/tinynode/node/ozma-gadget.service   /etc/systemd/system/
cp /opt/ozma/tinynode/node/ozma-tinynode.service /etc/systemd/system/

systemctl daemon-reload
systemctl enable --now ozma-gadget.service
systemctl enable --now ozma-tinynode.service
```

## Troubleshooting

| Symptom | Check |
|---|---|
| `/sys/class/udc` is empty | Cable not data-capable, or USB OTG not enabled in boot config |
| `/dev/hidg*` not present | Gadget setup failed — check `dmesg \| tail -20` |
| Host PC doesn't see HID device | Replug USB-C after gadget is set up; some hosts need re-enumeration |
| Reports sent but nothing happens | Confirm listener is running; check `--debug` output |
