#include "sequencer.h"
#include "channel.h"
#include "app_defs.h"
#include "utils.h"
#include <stdlib.h>

struct Track tracks[NUM_TRACKS];

static u8 currentTrack = 0;
static u8 currentPage = TRACK_VIEW;

void initialize()
{
    for (u8 i = 0; i < NUM_TRACKS; i++)
    {
        // gates
        tracks[i].euclidSequenceLength = 8;

        // notes
        tracks[i].turingMachineSequenceLength = 8;

        // misc
        tracks[i].midiChannelsFlags = i;
        tracks[i].octave = 1;

        // UI
        tracks[i].uiEuclidSequenceLength = 8;
        tracks[i].uiTuringMachineSequenceLength = 8;

        tracks[i].red = 7 * i;
        tracks[i].green = MAXLED / i;
        tracks[i].blue = (i + 1) % MAXLED;
    }
}

void handlePadEvent(u8 type, u8 index, u8 value)
{
    switch (type)
    {
        case TYPEPAD:
        {
            if (value)
            {
                switch (currentPage)
                {
                    case TRACK_VIEW:
                    {
                        // if Mute buttons
                        if ((index - 9) % 10 == 0)
                        {
                            setMuteValue(((index - 9) / 10) - 1);
                        }

                        // if White keys quantizer pad
                        if (index >= 11 && index <= 18)
                        {
                            toggleQuantizerWhiteKeysValue(index);
                        }

                        // if Black keys quantizer pad
                        if (index >= 21 && index <= 28)
                        {
                            toggleQuantizerBlackKeysValue(index);
                        }

                        // if Track select
                        if (index % 10 == 0)
                        {
                            u8 subtractor = index * 0.1;
                            selectTrack(8 - subtractor);
                        }
                    }
                    case CHANNEL_VIEW:
                    {

                    }
                }
            }
        }
    }
}

void setMuteValue(u8 track)
{
    tracks[track].isMuted ^= 1;
}

void selectTrack(u8 track)
{
    currentTrack = track;
}

void toggleQuantizerBlackKeysValue(u8 note)
{
    u8 bitPosition = quantizerBlackKeysLookupTable[note - 21];

    if (!bitPosition) return;

    u16 bitMask = 1 << bitPosition;
    tracks[currentTrack].quantizerValues = tracks[currentTrack].quantizerValues ^ bitMask;
    updateQuantizedNotesArray();
}

void toggleQuantizerWhiteKeysValue(u8 note)
{
    u16 bitMask = 1 << quantizerWhiteKeysLookupTable[note - 11];
    tracks[currentTrack].quantizerValues = tracks[currentTrack].quantizerValues ^ bitMask;
    updateQuantizedNotesArray();
}

void updateQuantizedNotesArray()
{
    for (u8 quantizerIndex = 0; quantizerIndex < 12; quantizerIndex++)
    {
        tracks[currentTrack].quantizedNotesArray = 0;

        u8 noteOn = isFlagOn16(tracks[currentTrack].quantizerValues, quantizerIndex);

        if (!noteOn) { continue; }

        u8 bitPos = quantizerIndex;
        while (bitPos < 32)
        {
            tracks[currentTrack].quantizedNotesArray = tracks[currentTrack].quantizedNotesArray | (1 << bitPos);
            bitPos += 12;
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
        if (tracks[trackNumber].isMuted) {continue;}
        incrementSequencerTrack(trackNumber);
    }
}

void incrementSequencerTrack(u8 trackNumber)
{
    u8 gate = nextGate(trackNumber);

    if (gate)
    {
        u8 note = 0;
        if (tracks[trackNumber].quantizedNotesArray != 0)
        { 
            note = nextNote(trackNumber);
        }
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
    insertRandom(trackNumber);
    
    if (tracks[trackNumber].turingMachineSequencePosition >= tracks[trackNumber].turingMachineSequenceLength)
    {
        tracks[trackNumber].turingMachineSequencePosition = 0;
    }

    u8 note = tracks[trackNumber].turingMachineSequenceShiftRegister << tracks[trackNumber].turingMachineSequencePosition;
    return getQuantizedNote(trackNumber, note);
}

void insertRandom(u8 trackNumber)
{
    u8 random = rand() % 32;
    if (random < tracks[trackNumber].turingMachineRandom)
    {
        u32 bitMask = 1 << tracks[trackNumber].turingMachineSequencePosition;
        tracks[trackNumber].turingMachineSequenceShiftRegister = tracks[trackNumber].turingMachineSequenceShiftRegister ^ bitMask;
    }
}

u8 getQuantizedNote(u8 trackNumber, u8 note)
{
    u8 truncatedNote = note & tracks[trackNumber].turingMachineScale;

    if (tracks[trackNumber].quantizedNotesArray == 0)
    {
        // return -1 to signify there is no note to play
        return -1;
    }

    for (u8 bitPos = truncatedNote; bitPos < NOTES_MAX_RANGE; bitPos++)
    {
        if (isFlagOn32(tracks[trackNumber].quantizedNotesArray, bitPos))
        {
            return bitPos + (tracks[trackNumber].octave * 12);
        }
    }

    for (u8 bitPos = truncatedNote; bitPos > 0; bitPos--)
    {
        if (isFlagOn32(tracks[trackNumber].quantizedNotesArray, bitPos))
        {
            return bitPos + (tracks[trackNumber].octave * 12);
        }
    }

    return 0;
}

void playNote(u8 trackNumber, u8 note)
{
    for (u8 channel = 0; channel < 8; channel++)
    {
        if(!isFlagOn8(tracks[trackNumber].midiChannelsFlags, channel)) { continue; }

        // kill previous note
        hal_send_midi(DINMIDI, NOTEOFF | channel, tracks[trackNumber].previousNote, 0);

        // if note is -1, there is no note to play
        if (note < 0) { continue; }

        // play new note
        hal_send_midi(DINMIDI, NOTEON | channel, note, 127);

        // store off triggered note for next time
        tracks[trackNumber].previousNote = note;
    }
}

void updateUi()
{
    // TODO
    renderMuteButtons();
    // draw currentTrack euclid parameters
    // draw currentTrack turing machine parameters
    renderQuantizer();
    renderTrackSelectButtons();

    // STRETCH
    // render gates along the bottom pads to provide some visual feedback
}

void renderMuteButtons()
{
    for (u8 trackNumber = 0; trackNumber < NUM_TRACKS; trackNumber++)
    {
        u8 padNumber = 19 + (trackNumber * 10);
        if (tracks[trackNumber].isMuted)
        {
            hal_plot_led(TYPEPAD, padNumber, MAXLED, 0, 0);
        }
        else
        {
            hal_plot_led(TYPEPAD, padNumber, 16, 16, 16);
        }
    }
}

void renderQuantizer()
{
    for (u8 i = 0; i < 12; i++)
    {
        u8 isNoteOn = isFlagOn16(tracks[currentTrack].quantizerValues, i);
        if (isNoteOn)
        {
            hal_plot_led(TYPEPAD, quantizerUiKeyToPadLookupTable[i], tracks[currentTrack].red, tracks[currentTrack].green, tracks[currentTrack].blue);
        }
        else
        {
            hal_plot_led(TYPEPAD, quantizerUiKeyToPadLookupTable[i], MAXLED, MAXLED, MAXLED);
        }
    }
}

void renderTrackSelectButtons()
{
    for (u8 trackNumber = 0; trackNumber < NUM_TRACKS; trackNumber++)
    {
        hal_plot_led(TYPEPAD, 80 - (trackNumber * 10), tracks[currentTrack].red, tracks[currentTrack].green, tracks[currentTrack].blue);
    }
}

