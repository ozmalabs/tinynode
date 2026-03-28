# TinyNode Setup — Teensy 4.1

## Why Teensy?

The Teensy 4.1 is a different class of device from the Linux SBCs:

- **No OS overhead** — bare-metal firmware, sub-millisecond boot
- **Native USB HID** — no gadget layer, no configfs, no kernel modules
- **Built-in Ethernet PHY** — no hat or adapter needed on Teensy 4.1
- **Trade-off:** No UAC audio or UVC camera — Teensy handles keyboard/mouse only (V0.1 scope)

## Hardware

| Item | Notes |
|---|---|
| **Teensy 4.1** | Built-in Ethernet jack (RJ45) on-board |
| USB-C cable | Teensy → host PC (USB HID) |
| Cat6 patch cable | Teensy Ethernet → head node / switch |

Note: The Ethernet jack on Teensy 4.1 requires two resistors and a connector populated — most boards ship with them, but verify yours has `J1` (RJ45) and `R1/R2` (10Ω termination) populated. Boards sold as "with Ethernet" have them; bare Teensy 4.1 boards often do not.

## Software prerequisites

1. **Arduino IDE 2.x** — https://www.arduino.cc/en/software
2. **Teensyduino 1.57+** — https://www.pjrc.com/teensy/td_download.html
   Install the Teensyduino add-on into your Arduino IDE installation.
3. **NativeEthernet library**
   In Arduino IDE: `Tools → Manage Libraries → search "NativeEthernet" → Install`

## Build and flash

1. Open `teensy/ozma_tinynode/ozma_tinynode.ino` in Arduino IDE.

2. Set board and USB type:
   ```
   Tools → Board → Teensyduino → Teensy 4.1
   Tools → USB Type → "Keyboard + Mouse + Joystick"
   Tools → CPU Speed → 600 MHz
   ```
   Use **"Serial + Keyboard + Mouse + Joystick"** while debugging (enables `Serial.print` output).

3. Verify (compile) first, then Upload.

4. The Teensy reboots and enumerates on the host PC as a keyboard and mouse within ~200ms.

## Verify it works

Open Serial Monitor (115200 baud) while the USB type is "Serial + Keyboard + Mouse + Joystick":

```
MAC: 04:E9:E5:xx:xx:xx
DHCP... OK — 10.0.100.20
Listening on UDP port 7331
```

Then send a test packet from the head node:

```sh
python3 scripts/test_hid.py 10.0.100.20
```

The target host PC should register a keypress and mouse movement.

## Network configuration

The firmware tries DHCP first (10-second timeout), then falls back to **10.0.100.20/24**. If your isolated network uses a different subnet, edit `STATIC_IP` / `STATIC_GW` near the top of the `.ino` file.

## Switching to production USB type

Once everything works, change:
```
Tools → USB Type → "Keyboard + Mouse + Joystick"
```
This removes the Serial interface, making the device enumerate slightly faster and appear cleaner to the host.

## Known limitations vs. Linux nodes

| Feature | Linux SBC | Teensy 4.1 |
|---|---|---|
| HID keyboard + mouse | Yes | Yes |
| UAC audio (V0.3) | Yes | No |
| UVC camera (V0.6) | Yes | No |
| Boot time | ~3–5 sec | < 1 sec |
| Cost | ~$10–55 | ~$35 |

The Teensy is best suited for a "minimum latency, keyboard/mouse only" node or for early prototyping before Linux node images are stable.
