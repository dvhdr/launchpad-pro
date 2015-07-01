//
//  app.h
//

#ifndef LAUNCHPAD_APP_H
#define LAUNCHPAD_APP_H

// ____________________________________________________________________________
//
#include "app_defs.h"

// ____________________________________________________________________________
//
// Interface to the hardware (implemented in launchpad_pro.a library)
// ____________________________________________________________________________

/**
 * Set an LED's RGB value.  This function is safe to call from any
 * of the app functions below, at any time.
 */
void hal_plot_led(u8 type, u8 index, u8 red, u8 green, u8 blue);

/**
 * Send a MIDI message to either USB port or to the DIN output.
 */
void hal_send_midi(u8 port, u8 status, u8 d1, u8 d2);

/**
 * Send system exclusive to USB or DIN.  Messages must be correctly formatted
 * (F0 ... F7) and must not exceed 320 bytes.
 */
void hal_send_sysex(u8 port, const u8* data, u16 length);

// ____________________________________________________________________________
//
// Interface from the hardware (implemented in app.c)
// ____________________________________________________________________________

/**
 * Called on startup, this is a good place to do any initialisation.
 */
void app_init();

/**
 *  1kHz (1ms) timer.  You can set LEDs and send MIDI out from this function,
 *  but you will get LED tearing as there is (currently) no double buffering.
 *
 *  You will get jitter.
 */
void app_timer_event();

/**
 * Called when a MIDI message is received from USB or DIN.
 */
void app_midi_event(u8 port, u8 status, u8 d1, u8 d2);

/**
 * As above, but for system exclusive messages.  Low level hardware buffering sets
 * a maximum message size of 320 bytes, messages larger than this will not work.
 */
void app_sysex_event(u8 port, u8 * data, u16 count);

/**
 * Called when a MIDI DIN breakout cable is connected or disconnected.  Note that
 * you can still write MIDI events to the DIN ports even if no cable is connected.
 */
void app_cable_event(u8 type, u8 value);

/**
 * Called when the user presses or releases any button or pad on the control surface.
 */
void app_surface_event(u8 type, u8 index, u8 value);

/**
 * Called when the low level pad scanning reports an aftertouch (pad pressure) event.
 * A pad press event will always come first.  Note that the factory firmware sets a
 * threshold to prevent excessive aftertouch after the initial hit, at the expense of
 * dynamic range.  This firmware does not - the full range of the pad is transmitted,
 * with the side effect that aftertouch onset is more rapid.
 */
void app_aftertouch_event(u8 index, u8 value);


#endif
