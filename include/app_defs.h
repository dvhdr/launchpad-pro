
#ifndef APP_TYPES_H
#define APP_TYPES_H

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

// ____________________________________________________________________________
//
// Don't modify this file!  This declares the binary interface to the library,
// so modifying it will probably break things.
// ____________________________________________________________________________
//
// Types
// ____________________________________________________________________________

typedef signed long  s32;
typedef signed short s16;
typedef signed char  s8;

typedef unsigned long  u32;
typedef unsigned short u16;
typedef unsigned char  u8;

// ____________________________________________________________________________
//
// App structure
// ____________________________________________________________________________

#define	TYPEPAD				0
#define	TYPESETUP			1

#define	MAXLED				63

// ____________________________________________________________________________
//
// Useful MIDI constants
// ____________________________________________________________________________

#define	NOTEON				0x90
#define	NOTEOFF				0x80
#define	POLYAFTERTOUCH		0xA0
#define	CC					0xB0
#define	CHANNELAFTERTOUCH   0xD0
#define	SONGPOSITIONPOINTER 0xF2
#define	MIDITIMINGCLOCK     0xF8
#define	MIDISTART           0xFA
#define	MIDICONTINUE        0xFB
#define	MIDISTOP            0xFC

// ____________________________________________________________________________
//
// MIDI ports
// ____________________________________________________________________________

// USB MIDI: "Standalone" port
#define	USBSTANDALONE		0

// USB MIDI: "MIDI" port
#define	USBMIDI				1

// MIDI DIN port
#define	DINMIDI				2

// ____________________________________________________________________________
//
// MIDI Jack Socket Switch IDs
// ____________________________________________________________________________

#define	MIDI_IN_CABLE		0
#define	MIDI_OUT_CABLE		1

// ____________________________________________________________________________
//
// Flash storage
// ____________________________________________________________________________

#define USER_AREA_SIZE 1024

// ____________________________________________________________________________
//
// Raw ADC reads. For technical reasons, the ADC layout is not the same as the
// LED layout. Be sure to translate ADC values using the table below as you
// need them!
// ____________________________________________________________________________

#define PAD_COUNT 64

static const u8 ADC_MAP[PAD_COUNT] =
{
	11, 51, 12, 52, 13, 53, 14, 54,
	15, 55, 16, 56, 17 ,57, 18, 58,
	21, 61, 22, 62, 23, 63, 24, 64,
	25, 65, 26, 66, 27, 67, 28, 68,
	31, 71, 32, 72, 33, 73, 34, 74,
	35, 75, 36, 76, 37, 77, 38, 78,
	41, 81, 42, 82, 43, 83, 44, 84,
	45, 85, 46, 86, 47, 87, 48, 88,
};

// ____________________________________________________________________________

#endif
