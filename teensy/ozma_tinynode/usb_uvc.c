/*
 * usb_uvc.c — UVC bulk streaming implementation for Teensy 4.1 (iMXRT1062)
 *
 * Design
 * ------
 * UVC allows two transfer modes: isochronous and bulk.  Bulk is chosen here
 * because the Teensy 4.1 USB stack's isochronous path has unreliable packet
 * ordering (see forum.pjrc.com/threads/67260).  Bulk transfers are reliable,
 * ordered, and well-exercised in Teensyduino (USB serial, mass storage).
 *
 * Each USB transaction carries:
 *   [2-byte UVC payload header][up to 510 bytes of MJPEG data]
 *
 * The last transaction of a frame sets the EOF bit in the header.  The FID
 * (frame ID) bit toggles with each new frame so the host can detect dropped
 * frames.
 *
 * Transfers are issued directly via the iMXRT1062 EHCI registers, bypassing
 * Teensyduino's higher-level USB packet pool.  This lets us send arbitrarily
 * large buffers without copying into 64-byte usb_packet_t chunks.
 *
 * Class requests (VS_PROBE_CONTROL, VS_COMMIT_CONTROL) are handled by
 * overriding the weak usb_setup_class() hook from Teensyduino's usb_dev.c.
 * When the host commits a format we set uvc_state.streaming = 1.
 *
 * Cache coherence
 * ---------------
 * The iMXRT1062 Cortex-M7 has separate I/D caches.  DMA does not go through
 * the cache, so before handing a buffer to the USB engine we must flush
 * (clean + invalidate) its cache lines.  arm_dcache_flush_delete() is
 * provided by the Teensyduino runtime.
 */

#include "usb_uvc.h"
#include <string.h>
#include <Arduino.h>     // millis(), __disable_irq(), __enable_irq()
#include <imxrt.h>       // iMXRT1062 register definitions

// ---- iMXRT1062 USB EHCI registers -------------------------------------------
// These names match the Teensyduino imxrt.h definitions.

#define UVC_USBHS_ENDPTLISTADDR   USBHS_ENDPTLISTADDR
#define UVC_USBHS_ENDPTPRIME      USBHS_ENDPTPRIME
#define UVC_USBHS_ENDPTFLUSH      USBHS_ENDPTFLUSH
#define UVC_USBHS_ENDPTCOMPLETE   USBHS_ENDPTCOMPLETE
#define UVC_USBHS_ENDPTCTRL(n)    USBHS_ENDPTCTRL(n)

// Endpoint N IN:  ENDPTPRIME/COMPLETE bit = N + 16
#define EP_IN_BIT(ep)  (1u << ((ep) + 16))

// ---- EHCI data structures ---------------------------------------------------

// Device Transfer Descriptor (dTD) — 32-byte aligned.
typedef struct {
    volatile uint32_t next;    // next dTD ptr; bit0=1 → terminate
    volatile uint32_t token;   // [31:16]=total_bytes [15]=IOC [7]=active
    volatile uint32_t page[5]; // buffer page pointers (4KB pages)
    uint32_t          _pad;    // pad to 32 bytes
} __attribute__((packed)) dTD_t;

// Device Queue Head (dQH) — 64 bytes.
// Layout from iMXRT1062 Reference Manual §42 and EHCI spec §3.6.
typedef struct {
    uint32_t          cap;      // endpoint capabilities / characteristics
    uint32_t          cur;      // current dTD pointer
    // dTD overlay (first transfer):
    volatile uint32_t next;
    volatile uint32_t token;
    volatile uint32_t page[5];
    uint32_t          _res;
    uint8_t           setup[8]; // SETUP packet buffer (endpoint 0 only)
    uint32_t          _pad[3];  // pad to 64 bytes
} __attribute__((packed)) dQH_t;

// ---- Module state -----------------------------------------------------------

// VS_VideoProbeCommitControl structure (UVC 1.1, 26 bytes).
typedef struct {
    uint16_t bmHint;
    uint8_t  bFormatIndex;
    uint8_t  bFrameIndex;
    uint32_t dwFrameInterval;
    uint16_t wKeyFrameRate;
    uint16_t wPFrameRate;
    uint16_t wCompQuality;
    uint16_t wCompWindowSize;
    uint16_t wDelay;
    uint32_t dwMaxVideoFrameSize;
    uint32_t dwMaxPayloadTransferSize;
} __attribute__((packed)) uvc_probe_commit_t;

// Our fixed format: MJPEG 1280×720 @ 30fps
static const uvc_probe_commit_t uvc_default_probe = {
    .bmHint                  = 0x0001,  // dwFrameInterval is fixed
    .bFormatIndex            = 1,
    .bFrameIndex             = 1,
    .dwFrameInterval         = 333333,  // 100ns units → 30fps
    .wKeyFrameRate           = 0,
    .wPFrameRate             = 0,
    .wCompQuality            = 0,
    .wCompWindowSize         = 0,
    .wDelay                  = 0,
    .dwMaxVideoFrameSize     = UVC_MAX_FRAME,
    .dwMaxPayloadTransferSize = 512,
};

static struct {
    uvc_probe_commit_t probe;
    volatile int       streaming;
    volatile uint8_t   fid;
} uvc_state;

// Two dTDs — alternate between them (double-buffer).
static dTD_t uvc_dtd[2]        __attribute__((used, aligned(32)));
static int   uvc_dtd_idx = 0;

// Two packet buffers: 2-byte UVC header + 510 bytes MJPEG.
// Must be 32-byte aligned (cache-line alignment).
static uint8_t uvc_pkt[2][512] __attribute__((used, aligned(32)));

// ---- Internal helpers -------------------------------------------------------

extern void arm_dcache_flush_delete(void *addr, uint32_t size);

// Return pointer to the IN endpoint queue head for endpoint ep.
static dQH_t *qh_in(uint8_t ep) {
    // QH list base is given by USBHS_ENDPTLISTADDR.
    // Index = ep*2 + 1 for IN direction.
    dQH_t *list = (dQH_t *)(uintptr_t)UVC_USBHS_ENDPTLISTADDR;
    return &list[ep * 2 + 1];
}

// Issue one bulk IN transfer and block until the host reads it (or timeout).
// data must be 32-byte aligned and remain valid until the function returns.
// len must be <= 16384 (4 × 4KB pages covered by one dTD).
static int bulk_in_send(uint8_t ep, void *data, size_t len) {
    if (!uvc_state.streaming) return -1;

    int    idx = uvc_dtd_idx;
    uvc_dtd_idx ^= 1;

    dTD_t  *dtd  = &uvc_dtd[idx];
    dQH_t  *qh   = qh_in(ep);
    uint32_t base = (uint32_t)(uintptr_t)data;

    // Flush D-cache so the USB DMA engine sees fresh data.
    arm_dcache_flush_delete(data, len);

    // Fill the transfer descriptor.
    dtd->next    = 1u;  // no next dTD (terminate bit)
    dtd->token   = ((uint32_t)len << 16)
                 | (1u << 15)  // IOC — interrupt on complete
                 | (1u << 7);  // Active
    dtd->page[0] = base;
    dtd->page[1] = (base + 0x1000u) & 0xFFFFF000u;
    dtd->page[2] = (base + 0x2000u) & 0xFFFFF000u;
    dtd->page[3] = (base + 0x3000u) & 0xFFFFF000u;
    dtd->page[4] = (base + 0x4000u) & 0xFFFFF000u;
    arm_dcache_flush_delete(dtd, sizeof(*dtd));

    // Link dTD into the QH and prime the endpoint.
    uint32_t prime_bit = EP_IN_BIT(ep);
    __disable_irq();
    qh->next  = (uint32_t)(uintptr_t)dtd;
    qh->token = 0;
    arm_dcache_flush_delete(qh, sizeof(*qh));
    UVC_USBHS_ENDPTPRIME = prime_bit;
    __enable_irq();

    // Wait for ENDPTCOMPLETE.  The bit is set when the host has consumed the
    // data (or at the end of the transaction for IN endpoints).
    uint32_t deadline = millis() + 200;
    while (!(UVC_USBHS_ENDPTCOMPLETE & prime_bit)) {
        if ((int32_t)(millis() - deadline) >= 0) {
            UVC_USBHS_ENDPTFLUSH = prime_bit;
            return -1;
        }
    }
    UVC_USBHS_ENDPTCOMPLETE = prime_bit;  // W1C — clear by writing 1
    return 0;
}

// ---- USB class request handler ----------------------------------------------
// Teensyduino's usb_dev.c defines usb_setup_class() as a weak symbol.
// We override it here to handle UVC class-specific requests.
//
// Relevant requests:
//   bRequest = 0x01 (SET_CUR):  host is setting probe or commit control
//   bRequest = 0x81 (GET_CUR):  host is reading current probe/commit
//   bRequest = 0x82 (GET_MIN):  host wants minimum values (return defaults)
//   bRequest = 0x83 (GET_MAX):  host wants maximum values (return defaults)
//   bRequest = 0x84 (GET_RES):  resolution (return defaults)
//   bRequest = 0x87 (GET_DEF):  default (return defaults)
//
// wValue high byte: VS_PROBE_CONTROL = 0x01, VS_COMMIT_CONTROL = 0x02
// wIndex: VS interface number

#define UVC_SET_CUR  0x01
#define UVC_GET_CUR  0x81
#define UVC_GET_MIN  0x82
#define UVC_GET_MAX  0x83
#define UVC_GET_RES  0x84
#define UVC_GET_DEF  0x87

#define VS_PROBE_CONTROL   0x01
#define VS_COMMIT_CONTROL  0x02

// EP0 data buffer — used to send probe/commit responses back to host.
// Teensyduino exposes usb_setup_send() for sending EP0 IN data.
extern void usb_setup_send(const void *data, uint32_t len);
extern void usb_setup_ack(void);

void usb_setup_class(uint8_t bmRequestType, uint8_t bRequest,
                     uint16_t wValue, uint16_t wIndex, uint16_t wLength) {
    // Only handle requests targeting the VS interface.
    if ((wIndex & 0xFF) != UVC_VS_INTERFACE_NUM) return;

    uint8_t  cs = (wValue >> 8) & 0xFF;  // control selector

    if (cs != VS_PROBE_CONTROL && cs != VS_COMMIT_CONTROL) return;

    if (bRequest == UVC_SET_CUR) {
        // Host is sending probe or commit data — acknowledge it.
        // For probe: we could read the host's preferences, but since we only
        // support one format we just accept whatever they send.
        // For commit: start streaming.
        if (cs == VS_COMMIT_CONTROL) {
            uvc_state.streaming = 1;
        }
        usb_setup_ack();
        return;
    }

    // All GET variants return our default probe/commit structure.
    if (bRequest == UVC_GET_CUR || bRequest == UVC_GET_MIN ||
        bRequest == UVC_GET_MAX || bRequest == UVC_GET_RES ||
        bRequest == UVC_GET_DEF) {
        uint32_t send_len = wLength < sizeof(uvc_probe_commit_t)
                          ? wLength
                          : sizeof(uvc_probe_commit_t);
        usb_setup_send(&uvc_default_probe, send_len);
        return;
    }
}

// ---- Public API -------------------------------------------------------------

void uvc_init(void) {
    memcpy(&uvc_state.probe, &uvc_default_probe, sizeof(uvc_default_probe));
    uvc_state.streaming = 0;
    uvc_state.fid       = 0;
    uvc_dtd_idx         = 0;

    // Configure the bulk IN endpoint in ENDPTCTRL.
    // Bits [23:22] = TX type: 10 = Bulk
    // Bit  [21]    = TX data toggle inhibit: 0
    // Bit  [19]    = TX stall: 0
    // Bit  [18]    = TXE (TX enable): 1
    uint32_t ctrl = UVC_USBHS_ENDPTCTRL(UVC_BULK_EP);
    ctrl &= ~(0x00FF0000u);
    ctrl |= (0x02u << 18) | (1u << 23);  // bulk type + enable
    UVC_USBHS_ENDPTCTRL(UVC_BULK_EP) = ctrl;
}

int uvc_is_streaming(void) {
    return uvc_state.streaming;
}

int uvc_send_frame(const uint8_t *mjpeg, size_t len) {
    if (!uvc_state.streaming)       return -1;
    if (!mjpeg || len == 0)         return -1;
    if (len > UVC_MAX_FRAME)        return -1;

    uint8_t  fid    = uvc_state.fid;
    size_t   offset = 0;
    int      buf    = 0;
    int      rc     = 0;

    while (offset < len) {
        size_t chunk   = len - offset;
        int    is_last = (chunk <= UVC_DATA_PER_PKT);
        if (chunk > UVC_DATA_PER_PKT) chunk = UVC_DATA_PER_PKT;

        uint8_t *pkt = uvc_pkt[buf & 1];
        buf++;

        // UVC payload header: 2 bytes (minimal, no PTS/SCR).
        pkt[0] = 0x02;                             // bHeaderLength = 2
        pkt[1] = fid | (is_last ? 0x02u : 0x00u); // FID | (EOF if last)

        memcpy(pkt + 2, mjpeg + offset, chunk);
        offset += chunk;

        rc = bulk_in_send(UVC_BULK_EP, pkt, 2 + chunk);
        if (rc != 0) break;
    }

    uvc_state.fid ^= 1;  // toggle FID for next frame
    return rc;
}
