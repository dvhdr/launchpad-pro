
#ifndef APP_TYPES_H
#define APP_TYPES_H

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

#define		TYPEPAD		0
#define		TYPESETUP	1

#define		MAXLED		63

// ____________________________________________________________________________
//
// Useful MIDI constants
// ____________________________________________________________________________

#define		NOTEON				0x90
#define		NOTEOFF				0x80
#define		POLYAFTERTOUCH		0xA0
#define		CC					0xB0
#define		CHANNELAFTERTOUCH   0xD0
#define     SONGPOSITIONPOINTER 0xF2
#define     MIDITIMINGCLOCK     0xF8
#define     MIDISTART           0xFA
#define     MIDICONTINUE        0xFB
#define     MIDISTOP            0xFC

// ____________________________________________________________________________
//
// MIDI ports
// ____________________________________________________________________________

// USB MIDI: "Standalone" port
#define     USBSTANDALONE       0

// USB MIDI: "MIDI" port
#define     USBMIDI		        1

// MIDI DIN port
#define     DINMIDI      	    2


// MIDI Jack Socket Switch IDs

#define     MIDI_IN_CABLE      0
#define     MIDI_OUT_CABLE     1

// ____________________________________________________________________________
#endif
