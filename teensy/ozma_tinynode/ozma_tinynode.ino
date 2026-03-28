/*
 * Ozma TinyNode — Teensy 4.1 firmware
 *
 * Handles:
 *   - USB HID keyboard + mouse (Teensyduino built-in)
 *   - USB UVC camera / MJPEG stream (usb_uvc.c — bulk transfer)
 *
 * Hardware:
 *   Teensy 4.1 (built-in Ethernet PHY, no shield needed)
 *
 * Arduino IDE settings:
 *   Board:     Teensy 4.1
 *   USB Type:  "Serial + Keyboard + Mouse + Joystick"  (debugging)
 *              "Keyboard + Mouse + Joystick"            (production)
 *   CPU Speed: 600 MHz
 *
 * IMPORTANT: Before building, apply the descriptor changes documented in
 *   docs/teensy_uvc_integration.md to the Teensyduino core usb_desc.h/.c.
 *   Without those changes the device will not enumerate as a UVC camera.
 *
 * Libraries (install via Library Manager):
 *   NativeEthernet — https://github.com/vjmuzik/NativeEthernet
 *
 * UDP ports:
 *   7331  HID events        (keyboard + mouse)
 *   7332  MJPEG video       (one frame per datagram, max 64 KB)
 *
 * HID packet format (port 7331):
 *   Byte 0:   type   0x01 = keyboard, 0x02 = mouse
 *   Bytes 1+: payload
 *
 *   Keyboard payload (8 bytes):
 *     [modifier, reserved, key1, key2, key3, key4, key5, key6]
 *
 *   Mouse payload (6 bytes):
 *     [buttons, x_lo, x_hi, y_lo, y_hi, scroll]
 *     x/y are int16, absolute, 0–32767.
 *
 * Video packet format (port 7332):
 *   Bytes 0-1: frame sequence number (uint16, big-endian) — for drop detection
 *   Bytes 2+:  raw MJPEG frame data
 *   Max useful payload: ~62000 bytes (UDP limit minus header).
 *   For larger frames, lower MJPEG quality on the head node.
 *
 * Network:
 *   Tries DHCP first (10s timeout), then falls back to static 10.0.100.20/24.
 */

#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>

extern "C" {
#include "usb_uvc.h"
}

// ---- Configuration ----------------------------------------------------------

#define UDP_HID_PORT    7331
#define UDP_VIDEO_PORT  7332

#define STATIC_IP       { 10, 0, 100, 20 }
#define STATIC_GW       { 10, 0, 100, 1  }
#define STATIC_SUBNET   { 255, 255, 255, 0 }

#define TYPE_KEYBOARD   0x01
#define TYPE_MOUSE      0x02

// ---- MAC address ------------------------------------------------------------

static void readTeensyMAC(uint8_t mac[6]) {
  uint32_t m0 = HW_OCOTP_MAC0;
  uint32_t m1 = HW_OCOTP_MAC1;
  mac[0] = m1 >> 8;
  mac[1] = m1;
  mac[2] = m0 >> 24;
  mac[3] = m0 >> 16;
  mac[4] = m0 >> 8;
  mac[5] = m0;
}

// ---- Globals ----------------------------------------------------------------

static uint8_t     mac[6];
static EthernetUDP udpHID;
static EthernetUDP udpVideo;

// Receive buffers
static uint8_t hidPkt[64];

// Video receive buffer in DMAMEM so it's accessible by USB DMA.
// 64 KB covers most 720p MJPEG frames at reasonable quality.
static uint8_t __attribute__((aligned(32))) videoBuf[65536];

// ---- HID handlers -----------------------------------------------------------

static void handleKeyboard(const uint8_t *p, int len) {
  if (len != 8) return;
  Keyboard.set_modifier(p[0]);
  Keyboard.set_key1(p[2]);
  Keyboard.set_key2(p[3]);
  Keyboard.set_key3(p[4]);
  Keyboard.set_key4(p[5]);
  Keyboard.set_key5(p[6]);
  Keyboard.set_key6(p[7]);
  Keyboard.send_now();
}

static void handleMouse(const uint8_t *p, int len) {
  if (len != 6) return;
  uint8_t buttons = p[0];
  int16_t x       = (int16_t)((uint16_t)p[1] | ((uint16_t)p[2] << 8));
  int16_t y       = (int16_t)((uint16_t)p[3] | ((uint16_t)p[4] << 8));
  int8_t  scroll  = (int8_t)p[5];
  Mouse.moveTo(x, y, scroll);
  if (buttons & 0x01) Mouse.press(MOUSE_LEFT);   else Mouse.release(MOUSE_LEFT);
  if (buttons & 0x02) Mouse.press(MOUSE_RIGHT);  else Mouse.release(MOUSE_RIGHT);
  if (buttons & 0x04) Mouse.press(MOUSE_MIDDLE); else Mouse.release(MOUSE_MIDDLE);
}

static void pollHID() {
  int size = udpHID.parsePacket();
  if (size < 2) return;
  int len = udpHID.read(hidPkt, sizeof(hidPkt));
  if (len < 2) return;
  const uint8_t *payload = hidPkt + 1;
  int plen = len - 1;
  switch (hidPkt[0]) {
    case TYPE_KEYBOARD: handleKeyboard(payload, plen); break;
    case TYPE_MOUSE:    handleMouse(payload, plen);    break;
  }
}

// ---- Video handler ----------------------------------------------------------

static uint16_t lastVideoSeq = 0xFFFF;

static void pollVideo() {
  int size = udpVideo.parsePacket();
  if (size < 3) return;  // need at least 2-byte seq + 1 byte MJPEG

  int len = udpVideo.read(videoBuf, sizeof(videoBuf));
  if (len < 3) return;

  // First two bytes: frame sequence number (big-endian)
  uint16_t seq = ((uint16_t)videoBuf[0] << 8) | videoBuf[1];
  if (seq == lastVideoSeq) return;  // duplicate
  lastVideoSeq = seq;

  uint8_t *mjpeg     = videoBuf + 2;
  size_t   mjpeg_len = (size_t)(len - 2);

  if (!uvc_is_streaming()) return;

  int rc = uvc_send_frame(mjpeg, mjpeg_len);
  if (rc != 0) {
    Serial.printf("uvc_send_frame failed (seq=%u, len=%u)\n", seq, (unsigned)mjpeg_len);
  }
}

// ---- Setup ------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(200);

  readTeensyMAC(mac);
  Serial.print("MAC: ");
  for (int i = 0; i < 6; i++) {
    if (i) Serial.print(":");
    if (mac[i] < 0x10) Serial.print("0");
    Serial.print(mac[i], HEX);
  }
  Serial.println();

  Serial.print("DHCP... ");
  if (Ethernet.begin(mac, 10000)) {
    Serial.print("OK — ");
    Serial.println(Ethernet.localIP());
  } else {
    Serial.println("failed, using static IP");
    IPAddress ip(STATIC_IP);
    IPAddress gw(STATIC_GW);
    IPAddress sn(STATIC_SUBNET);
    Ethernet.begin(mac, ip, gw, gw, sn);
    Serial.print("Static IP: ");
    Serial.println(Ethernet.localIP());
  }

  udpHID.begin(UDP_HID_PORT);
  udpVideo.begin(UDP_VIDEO_PORT);

  // Wait for USB to be configured before initialising UVC.
  // usb_configuration is a Teensyduino global set to 1 when enumerated.
  extern volatile uint8_t usb_configuration;
  Serial.print("Waiting for USB host...");
  while (!usb_configuration) delay(10);
  Serial.println(" OK");

  uvc_init();

  Serial.printf("TinyNode ready — HID:%d  Video:%d\n",
                UDP_HID_PORT, UDP_VIDEO_PORT);
}

// ---- Loop -------------------------------------------------------------------

void loop() {
  Ethernet.maintain();
  pollHID();
  pollVideo();
}
