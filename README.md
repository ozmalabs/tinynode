# tinynode

Ozma compute node firmware — turns a small board into a networked USB gadget (HID keyboard, HID mouse, UAC audio, UVC camera).

The node connects to a host PC via USB and receives input/audio/video from the Ozma head node over a local Ethernet network. The host PC sees a normal keyboard, mouse, audio device, and webcam — no drivers required.

## Supported platforms

| Platform | Type | HID | Audio (V0.3) | Camera (V0.6) | Setup |
|---|---|---|---|---|---|
| **Milk-V Duo S** | Linux SBC (RISC-V) | Yes | Yes | Yes | [docs/setup_milkv_duo_s.md](docs/setup_milkv_duo_s.md) |
| **Raspberry Pi Zero 2 W** | Linux SBC (ARM) | Yes | Yes | Yes | [docs/setup_raspberry_pi.md](docs/setup_raspberry_pi.md) |
| **Teensy 4.1** | Bare-metal MCU | Yes | Yes | No | [docs/setup_teensy.md](docs/setup_teensy.md) |

The Linux platforms (Duo S, RPi) use the same `gadget/` scripts and `node/listener.py` — only initial OS setup differs. The Teensy has its own Arduino firmware in `teensy/`.

## Repository layout

```
gadget/         USB gadget configfs scripts (Linux platforms)
node/           UDP listener daemon + systemd units (Linux platforms)
teensy/         Arduino firmware (Teensy 4.1)
scripts/        Test and utility scripts
docs/           Per-platform setup guides
```

## Project status

| Version | Feature | Status |
|---|---|---|
| V0.1 | USB HID keyboard + mouse, UDP listener | In progress |
| V0.2 | Multi-node virtual screen layout | Planned |
| V0.3 | UAC audio gadget + VBAN transport | Planned |
| V0.6 | UVC camera gadget | Planned |

## License

MIT
