//______________________________________________________________________________
//
// Headers
//______________________________________________________________________________

#include "app.h"

//______________________________________________________________________________

void app_surface_event(u8 type, u8 index, u8 value)
{
	switch (type)
	{
		case  TYPEPAD:
		{
			// example - light / extinguish pad LEDs, send MIDI
			hal_plot_led(TYPEPAD, index, value, value, value);
			hal_send_midi(USBSTANDALONE, NOTEON | 0, index, value);
		}
		break;

		case TYPESETUP:
		{
			// example - light the setup LED
			hal_plot_led(TYPESETUP, 0, value, value, value);
		}
		break;
	}
}

void app_midi_event(u8 port, u8 status, u8 d1, u8 d2)
{
	// example - MIDI interface functionality for USB "MIDI" port -> DIN port
	if (port == USBMIDI)
	{
		hal_send_midi(DINMIDI, status, d1, d2);
	}

	// // example -MIDI interface functionality for DIN -> USB "MIDI" port port
	if (port == DINMIDI)
	{
		hal_send_midi(USBMIDI, status, d1, d2);
	}
}

void app_sysex_event(u8 port, u8 * data, u16 count)
{
}

void app_aftertouch_event(u8 index, u8 value)
{
    // example - send poly aftertouch to MIDI ports
	hal_send_midi(USBMIDI, POLYAFTERTOUCH | 0, index, value);

    // example - set LED to white, brightness in proportion to pressure
	hal_plot_led(TYPEPAD, index, value/2, value/2, value/2);
}

void app_cable_event(u8 type, u8 value)
{
    // example - light the Setup LED to indicate cable connection
	if (type == MIDI_IN_CABLE)
	{
		hal_plot_led(TYPESETUP, 0, 0, value, 0);
	}
	else if (type == MIDI_OUT_CABLE)
	{
		hal_plot_led(TYPESETUP, 0, value, 0, 0);
	}
}

void app_timer_event()
{
}

//______________________________________________________________________________

void app_init()
{
    // example - light the LEDs to say hello!
	for (int i=0; i < 10; ++i)
	{
		for (int j=0; j < 10; ++j)
		{
			u8 r = i < 5 ? (MAXLED * (5-i))/5 : 0;
			u8 g = i < 5 ? (MAXLED * i)/5 : (MAXLED * (10-i))/5;
			u8 b = i < 5 ? 0 : (MAXLED * (i-5))/5;

			hal_plot_led(TYPEPAD, j*10 + i, r, b, g);
		}
	}
}
