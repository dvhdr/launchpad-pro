/******************************************************************************
 
 Copyright (c) 2015, Focusrite Audio Engineering Ltd.
 All rights reserved.
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 
 * Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.
 
 * Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.
 
 * Neither the name of Focusrite Audio Engineering Ltd., nor the names of its
 contributors may be used to endorse or promote products derived from
 this software without specific prior written permission.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
 *****************************************************************************/

//______________________________________________________________________________
//
// Headers
//______________________________________________________________________________

#include "app.h"

//______________________________________________________________________________
//
// This is where the fun is!  Add your code to the callbacks below to define how
// your app behaves.
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

//______________________________________________________________________________

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

//______________________________________________________________________________

void app_sysex_event(u8 port, u8 * data, u16 count)
{
	// example - respond to UDI messages?
}

//______________________________________________________________________________

void app_aftertouch_event(u8 index, u8 value)
{
    // example - send poly aftertouch to MIDI ports
	hal_send_midi(USBMIDI, POLYAFTERTOUCH | 0, index, value);

    // example - set LED to white, brightness in proportion to pressure
	hal_plot_led(TYPEPAD, index, value/2, value/2, value/2);
}
	
//______________________________________________________________________________

void app_cable_event(u8 type, u8 value)
{
    // example - light the Setup LED to indicate cable connections
	if (type == MIDI_IN_CABLE)
	{
		hal_plot_led(TYPESETUP, 0, 0, value, 0); // green
	}
	else if (type == MIDI_OUT_CABLE)
	{
		hal_plot_led(TYPESETUP, 0, value, 0, 0); // red
	}
}

//______________________________________________________________________________


void app_timer_event()
{
	// example - send MIDI clock at 125bpm
#define TICK_MS 20
	
	static u8 ms = TICK_MS;
	
	if (++ms >= TICK_MS)
	{
		ms = 0;
		
		// send a clock pulse up the USB
		hal_send_midi(USBSTANDALONE, MIDITIMINGCLOCK, 0, 0);
	}
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
