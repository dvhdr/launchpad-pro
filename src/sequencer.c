#include "sequencer.h"
#include "channel.h"
#include "app_defs.h"

struct Track tracks[NUM_TRACKS];

static u8 currentTrack = 0;

void initialize()
{

}

void handlePadEvent(u8 type, u8 index, u8 value)
{
    switch (type)
    {
        case TYPEPAD:
        {
            if (value)
            {

            }
        }
    }
}

u8 getTempo()
{
    return tempo;
}

u8 getMsPerClock()
{
    return MS_PER_MIN / (CLOCK_RATE * tempo);
}

void handleNextPulse()
{
    for (u8 trackNumber = 0; trackNumber < NUM_TRACKS; trackNumber++)
    {
        incrementSequencerTrack(trackNumber);
    }
}

void incrementSequencerTrack(u8 trackNumber)
{
    u8 gate = nextGate(trackNumber);

    if (gate)
    {
        u8 note = nextNote(trackNumber);
        playNote(trackNumber, note);
    }
}

u8 nextGate(u8 trackNumber)
{

}

u8 nextNote(u8 trackNumber)
{
    
}

void playNote(u8 trackNumber, u8 note)
{
    // kill previous note
    hal_send_midi(DINMIDI, NOTEOFF, tracks[trackNumber].previousNote, 0);

    // play new note
    hal_send_midi(DINMIDI, NOTEON, note, 127);

    // store off note for next time
    tracks[trackNumber].previousNote = note;
}

void updateUi()
{
    
}

