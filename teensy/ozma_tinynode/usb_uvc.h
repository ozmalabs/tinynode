/*
 * usb_uvc.h — USB Video Class (UVC 1.1) bulk streaming for Teensy 4.1
 *
 * Uses bulk transfer mode (not isochronous) to avoid the missing-packet
 * problem documented at forum.pjrc.com/threads/67260.
 *
 * The host sees a UVC camera that streams MJPEG over a bulk IN endpoint.
 * The firmware receives MJPEG frames from the head node over UDP and
 * forwards them to the host via this interface.
 *
 * Before this will work you must add UVC descriptors to usb_desc.h/.c —
 * see docs/teensy_uvc_integration.md for the exact changes.
 */

#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---- Configuration ----------------------------------------------------------
// These must match the descriptor additions in usb_desc.h.
// UVC_VC_INTERFACE_NUM and UVC_VS_INTERFACE_NUM depend on how many HID
// interfaces the chosen USB type creates before ours.
// For "Keyboard + Mouse + Joystick": VC=3, VS=4, EP=5.

#ifndef UVC_VC_INTERFACE_NUM
#define UVC_VC_INTERFACE_NUM  3
#endif
#ifndef UVC_VS_INTERFACE_NUM
#define UVC_VS_INTERFACE_NUM  4
#endif
#ifndef UVC_BULK_EP
#define UVC_BULK_EP           5   // endpoint number (IN direction)
#endif

// Maximum MJPEG frame size accepted by uvc_send_frame().
#define UVC_MAX_FRAME         (256u * 1024u)   // 256 KB

// MJPEG bytes carried per USB bulk packet (512 max packet - 2 UVC header).
#define UVC_DATA_PER_PKT      510u

// Video format — must match the frame descriptor in usb_desc additions.
#define UVC_WIDTH             1280u
#define UVC_HEIGHT            720u

// ---- API --------------------------------------------------------------------

// Initialise UVC state and enable the bulk endpoint in the USB controller.
// Call once in setup(), after USB has been configured (usb_configuration != 0).
void uvc_init(void);

// Returns 1 if the host has committed a streaming format (ready for frames).
int  uvc_is_streaming(void);

// Internal — called from the USB setup-packet handler to handle VS_PROBE /
// VS_COMMIT class requests.  Do not call directly.
void uvc_handle_setup(uint8_t bmRequestType, uint8_t bRequest,
                      uint16_t wValue, uint16_t wIndex, uint16_t wLength);

// Send one complete MJPEG frame to the host.
// Packetises into 512-byte UVC bulk payloads and blocks until all sent.
// Returns 0 on success, -1 on error (not streaming, frame too large, timeout).
int  uvc_send_frame(const uint8_t *mjpeg, size_t len);

#ifdef __cplusplus
}
#endif
