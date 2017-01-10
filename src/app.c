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
//
// In this example, we render the raw ADC data as LED rainbows.
//______________________________________________________________________________

static const u16 *g_ADC = 0;

//______________________________________________________________________________

void app_surface_event(u8 type, u8 index, u8 value)
{
}

//______________________________________________________________________________

void app_midi_event(u8 port, u8 status, u8 d1, u8 d2)
{
}

//______________________________________________________________________________

void app_sysex_event(u8 port, u8 * data, u16 count)
{
}

//______________________________________________________________________________

void app_aftertouch_event(u8 index, u8 value)
{
}
	
//______________________________________________________________________________

void app_cable_event(u8 type, u8 value)
{
}

//______________________________________________________________________________

void app_timer_event()
{
	// render raw ADC data as LEDs
	for (int i=0; i < PAD_COUNT; ++i)
	{
		// raw adc values are 12 bit, but LEDs are 6 bit.
		// Let's saturate into r;g;b for a rainbow effect to show pressure
		u16 r = 0;
		u16 g = 0;
		u16 b = 0;
		
		u16 x = (3 * MAXLED * g_ADC[i]) >> 12;
		
		if (x < MAXLED)
		{
			r = x;
		}
		else if (x >= MAXLED && x < (2*MAXLED))
		{
			r = MAXLED - x;
			g = x - MAXLED;
		}
		else
		{
			g = MAXLED - x;
			b = x - MAXLED;
		}
		
		hal_plot_led(TYPEPAD, ADC_MAP[i], r, g, b);
	}
}

//______________________________________________________________________________

void app_init(const u16 *adc_raw)
{
	
	// store off the raw ADC frame pointer for later use
	g_ADC = adc_raw;
}
