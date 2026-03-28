#!/usr/bin/env python3
"""
Ozma TinyNode — UDP HID listener (V0.1)

Receives HID report packets from the head node and writes them to
the USB gadget device files (/dev/hidg0 for keyboard, /dev/hidg1 for mouse).

Protocol (V0.1):
  All packets: 1-byte type header + payload
    0x01  Keyboard  — 8-byte boot protocol report
    0x02  Mouse     — 6-byte absolute pointer report [buttons, x_lo, x_hi, y_lo, y_hi, scroll]

UDP port: 7331 (fixed for V0.1)

Usage:
  python3 listener.py [--port 7331] [--kbd /dev/hidg0] [--mouse /dev/hidg1]
"""

import argparse
import logging
import socket
import struct
import sys

PORT = 7331
TYPE_KEYBOARD = 0x01
TYPE_MOUSE    = 0x02

KBD_REPORT_LEN   = 8
MOUSE_REPORT_LEN = 6


def main():
    parser = argparse.ArgumentParser(description="Ozma TinyNode UDP HID listener")
    parser.add_argument("--port",  type=int, default=PORT,         help="UDP listen port (default: 7331)")
    parser.add_argument("--kbd",   default="/dev/hidg0",           help="Keyboard gadget device")
    parser.add_argument("--mouse", default="/dev/hidg1",           help="Mouse gadget device")
    parser.add_argument("--bind",  default="0.0.0.0",              help="Bind address (default: all interfaces)")
    parser.add_argument("--debug", action="store_true",            help="Log every received report")
    args = parser.parse_args()

    logging.basicConfig(
        level=logging.DEBUG if args.debug else logging.INFO,
        format="%(asctime)s %(levelname)s %(message)s",
    )
    log = logging.getLogger("tinynode")

    try:
        kbd_fd   = open(args.kbd,   "wb", buffering=0)
        mouse_fd = open(args.mouse, "wb", buffering=0)
    except OSError as e:
        log.error("Failed to open gadget device: %s", e)
        log.error("Is the gadget set up? Run gadget/setup_gadget.sh first.")
        sys.exit(1)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((args.bind, args.port))
    log.info("Listening on %s:%d", args.bind, args.port)
    log.info("  keyboard → %s", args.kbd)
    log.info("  mouse    → %s", args.mouse)

    while True:
        try:
            data, addr = sock.recvfrom(64)
        except KeyboardInterrupt:
            break

        if len(data) < 2:
            log.debug("Short packet from %s, ignored", addr)
            continue

        pkt_type = data[0]
        payload  = data[1:]

        if pkt_type == TYPE_KEYBOARD:
            if len(payload) != KBD_REPORT_LEN:
                log.warning("Bad keyboard report length %d from %s", len(payload), addr)
                continue
            kbd_fd.write(payload)
            if args.debug:
                log.debug("KBD %s", payload.hex())

        elif pkt_type == TYPE_MOUSE:
            if len(payload) != MOUSE_REPORT_LEN:
                log.warning("Bad mouse report length %d from %s", len(payload), addr)
                continue
            mouse_fd.write(payload)
            if args.debug:
                log.debug("MOUSE %s", payload.hex())

        else:
            log.debug("Unknown packet type 0x%02x from %s", pkt_type, addr)


if __name__ == "__main__":
    main()
