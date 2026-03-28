# tinynode

Ozma compute node firmware — turns a small Linux SBC into a networked USB gadget (HID keyboard, HID mouse, UAC audio, UVC camera).

Designed for the **Milk-V Duo S** (SG2000, RISC-V + ARM Cortex-A53, 512MB). Should work on any Linux SBC with USB OTG and configfs gadget support.

## What it does

The node connects to a host PC via USB (appearing as a keyboard, mouse, audio device, and eventually webcam), and receives input/audio/video from the Ozma head node over a local Ethernet network.

## Hardware

| Item | Notes |
|---|---|
| Milk-V Duo S 512MB | Target platform |
| USB-C cable | Node → host PC (USB gadget) |
| Cat6 patch cable | Node → head node / switch |

## Project status

| Version | Feature | Status |
|---|---|---|
| V0.1 | USB HID keyboard + mouse gadget, UDP listener | In progress |
| V0.2 | Multi-node virtual screen layout | Planned |
| V0.3 | UAC audio gadget + VBAN transport | Planned |
| V0.6 | UVC camera gadget | Planned |

## Repository layout

```
gadget/         USB gadget configfs setup scripts
node/           Node-side daemon (UDP listener → HID inject)
scripts/        Utility scripts (flash, debug, test)
docs/           Setup and development notes
```

## Quick start

See [docs/setup.md](docs/setup.md) for flashing and first boot instructions.

## License

MIT
