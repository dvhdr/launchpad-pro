#include "sequencer.h"
#include "channel.h"
#include "app_defs.h"
#include "utils.h"

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
    if (tracks[trackNumber].euclidSequencePosition >= tracks[trackNumber].euclidSequenceLength)
    {
        tracks[trackNumber].euclidSequencePosition = 0;
    }

    return isFlagOn32(tracks[trackNumber].euclidSequenceFlags, tracks[trackNumber].euclidSequencePosition);
}

u8 nextNote(u8 trackNumber)
{
    if (tracks[trackNumber].turingMachineSequencePosition >= tracks[trackNumber].turingMachineSequenceLength)
    {
        tracks[trackNumber].turingMachineSequencePosition = 0;
    }

    return getQuantizedNote(trackNumber, tracks[trackNumber].turingMachineSequenceArray[tracks[trackNumber].turingMachineSequencePosition]);
}

u8 getQuantizedNote(u8 trackNumber, u8 note)
{
    return 64;
}

void playNote(u8 trackNumber, u8 note)
{
    for (u8 channel = 0; channel < 8; channel++)
    {
        if(!isFlagOn8(tracks[trackNumber].midiChannelsFlags, channel)) { continue; }

        // kill previous note
        hal_send_midi(DINMIDI, NOTEOFF | channel, tracks[trackNumber].previousNote, 0);

        // play new note
        hal_send_midi(DINMIDI, NOTEON | channel, note, 127);

        // store off note for next time
        tracks[trackNumber].previousNote = note;
    }
}

void updateUi()
{
    
}

