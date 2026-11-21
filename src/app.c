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
#include "midi2.h"
#include "ump/ump.h"
#include "platform/pads.h"

//______________________________________________________________________________
//
// Stage 1 checkpoint app.  Pads send MIDI 2.0 notes with 16-bit velocity on
// the Standalone port, the USB "MIDI" port is bridged to the DIN port, and the
// Setup LED shows the USB MIDI state:
//   green  - MIDI 2.0 active (alt 1, MIDI 2.0 Protocol)
//   amber  - alt 1 but MIDI 1.0 Protocol
//   red    - alt 0 (USB MIDI 1.0)
// Holding Setup pauses UMP receive (RX drain) for the backpressure test.
//______________________________________________________________________________

#define PAD_INDEXES 100
#define NO_ADC 0xFF

// store ADC frame pointer
static const u16 *g_ADC = 0;

// inverse of ADC_MAP: pad index -> ADC channel
static u8 g_adc_of[PAD_INDEXES];

// 16-bit velocity of each held pad (0 = not held by us)
static u16 g_vel[PAD_INDEXES];
static u8 g_held[PAD_INDEXES];

//______________________________________________________________________________

static void set_pad_led(u8 index, u8 level)
{
    hal_plot_led(TYPEPAD, index, level, level, level);
}

static void update_setup_led(void)
{
    if (midi2_active())
    {
        hal_plot_led(TYPESETUP, 0, 0, MAXLED, 0);   // green
    }
    else if (midi2_usb_alt() == 1)
    {
        hal_plot_led(TYPESETUP, 0, MAXLED, 20, 0);  // amber
    }
    else
    {
        hal_plot_led(TYPESETUP, 0, MAXLED, 0, 0);   // red
    }
}

//______________________________________________________________________________

void app_surface_event(u8 type, u8 index, u8 value)
{
    switch (type)
    {
        case TYPEPAD:
        {
            if (index >= PAD_INDEXES)
            {
                break;
            }
            
            if (value)
            {
                u8 adc = g_adc_of[index];
                u16 v = 0;
                
                if (adc != NO_ADC)
                {
                    v = (u16)ump_scale_up(ADC_Max[adc] & 0x0FFF, 12, 16);
                }
                
                g_vel[index] = v;
                g_held[index] = 1;
                set_pad_led(index, (u8)(v >> 10));
                hal_send_note_on2(USBSTANDALONE, 0, index, v);
            }
            else if (g_held[index])
            {
                g_held[index] = 0;
                g_vel[index] = 0;
                set_pad_led(index, 0);
                hal_send_note_off2(USBSTANDALONE, 0, index, 0);
            }
        }
        break;
            
        case TYPESETUP:
        {
            // held: stop draining received UMP, released: resume
            midi2_debug_pause_rx(value ? 1 : 0);
        }
        break;
    }
}

//______________________________________________________________________________

void app_midi_event(u8 port, u8 status, u8 d1, u8 d2)
{
    // MIDI interface functionality for USB "MIDI" port -> DIN port
    if (port == USBMIDI)
    {
        hal_send_midi(DINMIDI, status, d1, d2);
    }
    
    // MIDI interface functionality for DIN -> USB "MIDI" port
    if (port == DINMIDI)
    {
        hal_send_midi(USBMIDI, status, d1, d2);
    }
}

//______________________________________________________________________________

void app_sysex_event(u8 port, u8 * data, u16 count)
{
}

//______________________________________________________________________________

void app_aftertouch_event(u8 index, u8 value)
{
    // poly pressure on the Standalone port, with the pad notes (32-bit and
    // coalesced in MIDI 2.0, 7-bit otherwise)
    hal_send_poly_pressure2(USBSTANDALONE, 0, index, ump_scale_up(value, 7, 32));
}

//______________________________________________________________________________

void app_cable_event(u8 type, u8 value)
{
    // the Setup LED shows the USB MIDI state instead (see app_timer_event)
}

//______________________________________________________________________________

void app_timer_event()
{
    // send MIDI clock at 125bpm
#define TICK_MS 20
#define SETUP_LED_MS 100
    
    static u8 ms = TICK_MS;
    static u8 led_ms = SETUP_LED_MS;
    
    if (++ms >= TICK_MS)
    {
        ms = 0;
        
        // send a clock pulse up the USB
        hal_send_midi(USBSTANDALONE, MIDITIMINGCLOCK, 0, 0);
    }
    
    if (++led_ms >= SETUP_LED_MS)
    {
        led_ms = 0;
        update_setup_led();
    }
}

//______________________________________________________________________________

void app_init(const u16 *adc_raw)
{
    for (int i = 0; i < PAD_INDEXES; ++i)
    {
        g_adc_of[i] = NO_ADC;
    }
    
    for (int i = 0; i < PAD_COUNT; ++i)
    {
        g_adc_of[ADC_MAP[i]] = (u8)i;
    }
    
    // store off the raw ADC frame pointer for later use
    g_ADC = adc_raw;
    
    update_setup_led();
}
