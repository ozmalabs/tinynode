# Teensy 4.1 UVC Integration Guide

This document describes the one-time changes needed to Teensyduino's core files
to add UVC descriptors to the USB configuration.  Everything else lives in the
sketch directory (`usb_uvc.h`, `usb_uvc.c`).

---

## 1. Arduino IDE USB type

Set to **"Serial + Keyboard + Mouse + Joystick"** (debug) or
**"Keyboard + Mouse + Joystick"** (production).

The UVC interfaces are appended after the HID interfaces:

| Interface | What |
|---|---|
| 0 | HID Keyboard |
| 1 | HID Mouse |
| 2 | HID Joystick |
| **3** | **UVC Video Control** |
| **4** | **UVC Video Streaming** |

Endpoint 5 IN is used for the UVC bulk stream.
If your USB type uses fewer HID interfaces, adjust `UVC_VC_INTERFACE_NUM`,
`UVC_VS_INTERFACE_NUM`, and `UVC_BULK_EP` in `usb_uvc.h` accordingly.

---

## 2. Locate `usb_desc.h` in the Teensyduino core

On Linux/macOS the Teensy 4 core files live at a path like:

```
~/.arduino15/packages/teensy/hardware/avr/<version>/cores/teensy4/usb_desc.h
```

Or inside the Arduino app bundle on macOS:
```
Arduino.app/Contents/Java/hardware/teensy/avr/cores/teensy4/usb_desc.h
```

---

## 3. Changes to `usb_desc.h`

Find the block for your USB type (search for `KEYBOARD_MOUSE_JOYSTICK`).
It will have lines like:

```c
#define NUM_INTERFACE          3
#define CONFIG_DESC_SIZE       (9 + 18 + 18 + 18)   // example — yours may differ
```

Change them to:

```c
#define NUM_INTERFACE          5
#define CONFIG_DESC_SIZE       (9 + 18 + 18 + 18 + 127)
```

(127 = total bytes of the UVC descriptor block added in step 4.)

Also add the endpoint definition.  Find the existing `ENDPOINT4_CONFIG` (or
the highest numbered one) and add after it:

```c
#define ENDPOINT5_CONFIG       EP_SINGLE_BUFFER + EP_TYPE_BULK_IN
```

---

## 4. Changes to `usb_desc.c`

Open `usb_desc.c` in the same directory.  Find the configuration descriptor
array for your USB type.  It ends with the last HID endpoint descriptor and a
closing `};`.  Insert the following **before** the `};`:

```c
        // ---- UVC: Interface Association Descriptor --------------------------
        8,                          // bLength
        0x0B,                       // bDescriptorType: IAD
        3,                          // bFirstInterface (UVC_VC_INTERFACE_NUM)
        2,                          // bInterfaceCount
        0x0E,                       // bFunctionClass: Video
        0x03,                       // bFunctionSubClass: Video Interface Collection
        0x00,                       // bFunctionProtocol
        0,                          // iFunction

        // ---- UVC: Video Control Interface -----------------------------------
        9,                          // bLength
        0x04,                       // bDescriptorType: INTERFACE
        3,                          // bInterfaceNumber (UVC_VC_INTERFACE_NUM)
        0,                          // bAlternateSetting
        0,                          // bNumEndpoints (no VC endpoint in minimal config)
        0x0E,                       // bInterfaceClass: Video
        0x01,                       // bInterfaceSubClass: Video Control
        0x00,                       // bInterfaceProtocol
        0,                          // iInterface

        // VC Class-Specific Interface Header
        13,                         // bLength
        0x24,                       // bDescriptorType: CS_INTERFACE
        0x01,                       // bDescriptorSubType: VC_HEADER
        0x10, 0x01,                 // bcdUVC: 1.10
        39, 0,                      // wTotalLength: 39 bytes of VC class-specific
        0x80, 0x8D, 0x5B, 0x00,     // dwClockFrequency: 6 MHz (informational)
        1,                          // bInCollection: 1 streaming interface
        4,                          // baInterfaceNr(1): VS interface = 4

        // Camera Terminal
        17,                         // bLength
        0x24,                       // bDescriptorType: CS_INTERFACE
        0x02,                       // bDescriptorSubType: VC_INPUT_TERMINAL
        1,                          // bTerminalID
        0x01, 0x02,                 // wTerminalType: ITT_CAMERA
        0,                          // bAssocTerminal: none
        0,                          // iTerminal
        0x00, 0x00,                 // wObjectiveFocalLengthMin
        0x00, 0x00,                 // wObjectiveFocalLengthMax
        0x00, 0x00,                 // wOcularFocalLength
        2,                          // bControlSize
        0x00, 0x00,                 // bmControls: none

        // Output Terminal
        9,                          // bLength
        0x24,                       // bDescriptorType: CS_INTERFACE
        0x03,                       // bDescriptorSubType: VC_OUTPUT_TERMINAL
        2,                          // bTerminalID
        0x01, 0x01,                 // wTerminalType: TT_STREAMING
        0,                          // bAssocTerminal
        1,                          // bSourceID: Camera Terminal
        0,                          // iTerminal

        // ---- UVC: Video Streaming Interface ---------------------------------
        9,                          // bLength
        0x04,                       // bDescriptorType: INTERFACE
        4,                          // bInterfaceNumber (UVC_VS_INTERFACE_NUM)
        0,                          // bAlternateSetting
        1,                          // bNumEndpoints: 1 bulk IN
        0x0E,                       // bInterfaceClass: Video
        0x02,                       // bInterfaceSubClass: Video Streaming
        0x00,                       // bInterfaceProtocol
        0,                          // iInterface

        // VS Class-Specific Input Header
        14,                         // bLength
        0x24,                       // bDescriptorType: CS_INTERFACE
        0x01,                       // bDescriptorSubType: VS_INPUT_HEADER
        1,                          // bNumFormats
        55, 0,                      // wTotalLength: 55 bytes of VS class-specific
        0x80 | 5,                   // bEndpointAddress: EP5 IN  (UVC_BULK_EP | 0x80)
        0x00,                       // bmInfo
        2,                          // bTerminalLink: Output Terminal ID
        0,                          // bStillCaptureMethod
        0,                          // bTriggerSupport
        0,                          // bTriggerUsage
        1,                          // bControlSize
        0x00,                       // bmaControls(1)

        // MJPEG Format Descriptor
        11,                         // bLength
        0x24,                       // bDescriptorType: CS_INTERFACE
        0x06,                       // bDescriptorSubType: VS_FORMAT_MJPEG
        1,                          // bFormatIndex
        1,                          // bNumFrameDescriptors
        0x01,                       // bmFlags: FixedSizeSamples
        1,                          // bDefaultFrameIndex
        0,                          // bAspectRatioX
        0,                          // bAspectRatioY
        0x00,                       // bmInterlaceFlags
        0,                          // bCopyProtect

        // MJPEG Frame Descriptor — 1280×720 @ 30fps
        30,                         // bLength
        0x24,                       // bDescriptorType: CS_INTERFACE
        0x07,                       // bDescriptorSubType: VS_FRAME_MJPEG
        1,                          // bFrameIndex
        0x00,                       // bmCapabilities
        0x00, 0x05,                 // wWidth:  1280 (little-endian)
        0xD0, 0x02,                 // wHeight:  720 (little-endian)
        0x00, 0x80, 0x97, 0x00,     // dwMinBitRate:  10 000 000 bps
        0x00, 0x00, 0x2F, 0x01,     // dwMaxBitRate:  20 000 000 bps
        0x00, 0x00, 0x04, 0x00,     // dwMaxVideoFrameBufferSize: 262144 bytes
        0x15, 0x16, 0x05, 0x00,     // dwDefaultFrameInterval: 333333 (30fps)
        1,                          // bFrameIntervalType: 1 discrete value
        0x15, 0x16, 0x05, 0x00,     // dwFrameInterval[0]: 333333

        // Bulk IN Endpoint
        7,                          // bLength
        0x05,                       // bDescriptorType: ENDPOINT
        0x80 | 5,                   // bEndpointAddress: EP5 IN
        0x02,                       // bmAttributes: Bulk
        0x00, 0x02,                 // wMaxPacketSize: 512 (HS bulk)
        0,                          // bInterval: ignored for bulk
```

**Count the bytes above:** 8+9+13+17+9+9+14+11+30+7 = **127** — this is the
value added to `CONFIG_DESC_SIZE` in step 3.

---

## 5. Verify the descriptor wTotalLength fields

Two `wTotalLength` fields must be exact:

| Field | Where | Value | What it counts |
|---|---|---|---|
| VC Header `wTotalLength` | VC class-specific header | **39** | VC Header(13) + Camera Terminal(17) + Output Terminal(9) |
| VS Input Header `wTotalLength` | VS class-specific header | **55** | VS Input Header(14) + MJPEG Format(11) + MJPEG Frame(30) |

If you add more frame descriptors or terminals, update these values.

---

## 6. Build and test

1. Recompile and flash the sketch.
2. On the host PC, open Device Manager (Windows) or `system_profiler SPUSBDataType`
   (macOS) and confirm a "USB Video Device" or "UVC Camera" appears.
3. Open a camera app (Windows Camera, QuickTime, OBS) — it should show the
   device.  No image yet until the head node starts sending MJPEG frames.
4. Send a test frame from the head node:
   ```sh
   # A minimal MJPEG file (any valid .jpg works for testing):
   python3 -c "
   import socket, struct
   data = open('test.jpg','rb').read()
   hdr  = struct.pack('>HI', 0, len(data))  # seq=0, len
   s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
   s.sendto(hdr + data, ('10.0.100.20', 7332))
   "
   ```

---

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| Device not recognised as camera | wTotalLength fields wrong, or NUM_INTERFACE not updated |
| Device recognised but no stream | uvc_is_streaming() returning 0 — host hasn't sent VS_COMMIT |
| Corrupt image on host | UVC header FID/EOF bit logic; or frame size > UVC_MAX_FRAME |
| `arm_dcache_flush_delete` undefined | Include `<DMAChannel.h>` or `<cache.h>` in the .ino |
| ENDPTCOMPLETE timeout | Endpoint not enabled in ENDPTCTRL, or descriptor endpoint address mismatch |
