#!/usr/bin/env python3
"""
Quick test: sends a keyboard report and a mouse report to the node listener.
Run from the head node (or any machine on the isolated network).

Usage:
  python3 test_hid.py <node_ip> [--port 7331]

Example — types the letter 'a' and moves the mouse to center screen:
  python3 test_hid.py 10.0.100.10
"""

import argparse
import socket
import struct
import time

PORT = 7331
TYPE_KEYBOARD = 0x01
TYPE_MOUSE    = 0x02


def send_key(sock, addr, modifier, keycode):
    """Send a key press then release."""
    # Press
    report = struct.pack("8B", modifier, 0, keycode, 0, 0, 0, 0, 0)
    sock.sendto(bytes([TYPE_KEYBOARD]) + report, addr)
    time.sleep(0.02)
    # Release
    report = struct.pack("8B", 0, 0, 0, 0, 0, 0, 0, 0)
    sock.sendto(bytes([TYPE_KEYBOARD]) + report, addr)
    time.sleep(0.02)


def send_mouse(sock, addr, x, y, buttons=0, scroll=0):
    """Send an absolute mouse position (x, y in 0–32767)."""
    report = struct.pack("<BhhB", buttons, x, y, scroll & 0xFF)
    sock.sendto(bytes([TYPE_MOUSE]) + report, addr)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("host", help="Node IP address")
    parser.add_argument("--port", type=int, default=PORT)
    args = parser.parse_args()

    addr = (args.host, args.port)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    print(f"Sending test HID reports to {args.host}:{args.port}")

    # Type 'a' (HID keycode 0x04)
    print("  Typing 'a'...")
    send_key(sock, addr, 0x00, 0x04)

    # Move mouse to center
    print("  Moving mouse to center (16383, 16383)...")
    send_mouse(sock, addr, 16383, 16383)

    # Left click
    print("  Left click...")
    send_mouse(sock, addr, 16383, 16383, buttons=0x01)
    time.sleep(0.05)
    send_mouse(sock, addr, 16383, 16383, buttons=0x00)

    print("Done.")


if __name__ == "__main__":
    main()
