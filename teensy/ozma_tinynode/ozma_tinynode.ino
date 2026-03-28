/*
 * Ozma TinyNode — Teensy 4.1 firmware (V0.1)
 *
 * Receives UDP HID reports from the Ozma head node and injects them
 * as USB keyboard and mouse events via Teensyduino's built-in HID stack.
 *
 * Hardware:
 *   Teensy 4.1 (has built-in Ethernet PHY — no shield needed)
 *
 * Arduino IDE settings:
 *   Board:     Teensy 4.1
 *   USB Type:  "Keyboard + Mouse + Joystick"
 *              (use "Serial + Keyboard + Mouse + Joystick" while debugging)
 *   CPU Speed: 600 MHz
 *
 * Libraries (install via Library Manager or Teensyduino installer):
 *   NativeEthernet   — https://github.com/vjmuzik/NativeEthernet
 *   NativeEthernetUdp (bundled with NativeEthernet)
 *
 * Packet protocol (matches Linux listener.py):
 *   Byte 0:    type  0x01 = keyboard, 0x02 = mouse
 *   Bytes 1-N: payload
 *
 *   Keyboard payload (8 bytes):
 *     [modifier, reserved, key1, key2, key3, key4, key5, key6]
 *     Standard HID boot-protocol keyboard report.
 *
 *   Mouse payload (6 bytes):
 *     [buttons, x_lo, x_hi, y_lo, y_hi, scroll]
 *     x/y are int16, absolute, range 0–32767.
 *     buttons: bit0=left, bit1=right, bit2=middle
 *
 * Network:
 *   Tries DHCP first; falls back to static 10.0.100.20/24.
 *   UDP port: 7331
 *
 * Mouse positioning note:
 *   Mouse.moveTo() uses Teensyduino's absolute digitizer mode (added in
 *   Teensyduino 1.57). Range matches the 0–32767 HID coordinate space.
 *   If you are on an older Teensyduino without moveTo(), replace with
 *   relative Mouse.move() and track position manually.
 */

#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>

// ---- Configuration --------------------------------------------------------

#define UDP_PORT      7331
#define STATIC_IP     { 10, 0, 100, 20 }
#define STATIC_GW     { 10, 0, 100, 1  }
#define STATIC_SUBNET { 255, 255, 255, 0 }

#define TYPE_KEYBOARD 0x01
#define TYPE_MOUSE    0x02

// ---- MAC address ----------------------------------------------------------
// Teensy 4.1 has a factory-programmed MAC in fuse registers.

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

// ---- Globals --------------------------------------------------------------

static uint8_t  mac[6];
static EthernetUDP udp;
static uint8_t  pkt[64];

// ---- HID handlers ---------------------------------------------------------

static void handleKeyboard(const uint8_t* payload, int len) {
  if (len != 8) return;
  // payload: [modifier, reserved, key1..key6]
  Keyboard.set_modifier(payload[0]);
  Keyboard.set_key1(payload[2]);
  Keyboard.set_key2(payload[3]);
  Keyboard.set_key3(payload[4]);
  Keyboard.set_key4(payload[5]);
  Keyboard.set_key5(payload[6]);
  Keyboard.set_key6(payload[7]);
  Keyboard.send_now();
}

static void handleMouse(const uint8_t* payload, int len) {
  if (len != 6) return;
  // payload: [buttons, x_lo, x_hi, y_lo, y_hi, scroll]
  uint8_t buttons = payload[0];
  int16_t x       = (int16_t)((uint16_t)payload[1] | ((uint16_t)payload[2] << 8));
  int16_t y       = (int16_t)((uint16_t)payload[3] | ((uint16_t)payload[4] << 8));
  int8_t  scroll  = (int8_t)payload[5];

  // Absolute positioning — requires Teensyduino 1.57+
  Mouse.moveTo(x, y, scroll);

  // Button state (press/release on change)
  if (buttons & 0x01) Mouse.press(MOUSE_LEFT);   else Mouse.release(MOUSE_LEFT);
  if (buttons & 0x02) Mouse.press(MOUSE_RIGHT);  else Mouse.release(MOUSE_RIGHT);
  if (buttons & 0x04) Mouse.press(MOUSE_MIDDLE); else Mouse.release(MOUSE_MIDDLE);
}

// ---- Setup ----------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  // Brief wait for Serial monitor if debugging; harmless in production.
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
  if (Ethernet.begin(mac, 10000 /* ms timeout */)) {
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

  udp.begin(UDP_PORT);
  Serial.print("Listening on UDP port ");
  Serial.println(UDP_PORT);
}

// ---- Loop -----------------------------------------------------------------

void loop() {
  // Maintain DHCP lease
  Ethernet.maintain();

  int size = udp.parsePacket();
  if (size < 2) return;

  int len = udp.read(pkt, sizeof(pkt));
  if (len < 2) return;

  const uint8_t  type    = pkt[0];
  const uint8_t* payload = pkt + 1;
  const int      plen    = len - 1;

  switch (type) {
    case TYPE_KEYBOARD: handleKeyboard(payload, plen); break;
    case TYPE_MOUSE:    handleMouse(payload, plen);    break;
    default: break;  // unknown type — ignore
  }
}
