#include "sequencer.h"
#include "channel.h"
#include "app_defs.h"
#include "utils.h"
#include <stdlib.h>

struct Track tracks[NUM_TRACKS];

static u8 currentTrack = 0;
static u8 currentPage = TRACK_VIEW;
static u8 tempo = 115;
static u8 msPerClock = 0;
static u8 syncMode = INTERNAL_CLOCK;
static u8 sequencerState = SEQUENCER_STOPPED;

void initialize()
{
    setMsPerClock();

    for (u8 i = 0; i < NUM_TRACKS; i++)
    {
        // gates
        tracks[i].euclidSequenceLength = 8;

        // notes
        tracks[i].turingMachineSequenceLength = 8;
        tracks[i].turingMachineScale = 12;
        tracks[i].turingMachineRandom = 8;

        // misc
        tracks[i].midiChannelsFlags = Pow(2, i);
        tracks[i].octave = DEFAULT_OCTAVE;
        tracks[i].resolution = 5;

        // UI
        tracks[i].uiEuclidSequenceLength = 8;
        tracks[i].uiTuringMachineSequenceLength = 8;

        tracks[i].red = 7 * i;
        tracks[i].green = MAXLED / i;
        tracks[i].blue = (i + 1) % MAXLED;
    }
}

u8 getSyncMode()
{
    return syncMode;
}

void toggleSyncMode()
{
    syncMode ^= 1;
}

void setCurrentView(u8 view)
{
    currentPage = view;
    clearAllPads();
}

void handlePadEvent(u8 type, u8 index, u8 value)
{
    switch (type)
    {
        case TYPEPAD:
        {
            if (value)
            {
                // Global settings

                // if tempo increase
                if (index == 91)
                {
                    setTempoIncrease();
                    break;
                }

                // if tempo decrease
                if (index == 92)
                {
                    setTempoDecrease();
                    break;
                }

                // if octave down
                if (index == 93)
                {
                    setOctaveDown();
                    break;
                }

                // if octave up
                if (index == 94)
                {
                    setOctaveUp();
                    break;
                }

                // if Mute buttons
                if ((index - 9) % 10 == 0)
                {
                    setMuteValue(8 - ((index - 9) / 10));
                    break;
                }
                        
                // if Track select
                if (index % 10 == 0)
                {
                    u8 subtractor = index * 0.1;
                    selectTrack(8 - subtractor);
                    break;
                }

                // if Track view select
                if (index == 95)
                {
                    setCurrentView(TRACK_VIEW);
                    break;
                }

                // if Channel view select
                if (index == 96)
                {
                    setCurrentView(CHANNEL_VIEW);
                    break;
                }

                switch (currentPage)
                {
                    case TRACK_VIEW:
                    {
                        // if White keys quantizer pad
                        if (index >= 11 && index <= 18)
                        {
                            toggleQuantizerWhiteKeysValue(index);
                            break;
                        }

                        // if Black keys quantizer pad
                        if (index >= 21 && index <= 28)
                        {
                            toggleQuantizerBlackKeysValue(index);
                            break;
                        }

                        // if euclid pattern length
                        if (index >= 83 && index <= 88)
                        {
                            u8 bitNumber = 5 - (index - 83);
                            setEuclidPatternLength(bitNumber);
                            break;
                        }

                        // if euclid density
                        if (index >= 73 && index <= 78)
                        {
                            u8 bitNumber = 5 - (index - 73);
                            setEuclidDensity(bitNumber);
                            break;
                        }

                        // if euclid offset
                        if (index >= 63 && index <= 68)
                        {
                            u8 bitNumber = 5 - (index - 63);
                            setEuclidOffset(bitNumber);
                            break;
                        }

                        // if turing machine pattern length
                        if (index >= 53 && index <= 58)
                        {
                            u8 bitNumber = 5 - (index - 53);
                            setTuringMachinePatternLength(bitNumber);
                            break;
                        }

                        // if turing machine pattern scale
                        if (index >= 43 && index <= 48)
                        {
                            u8 bitNumber = 5 - (index - 43);
                            setTuringMachineScale(bitNumber);
                            break;
                        }

                        // if turing machine pattern random
                        if (index >= 33 && index <= 38)
                        {
                            u8 bitNumber = 5 - (index - 33);
                            setTuringMachineRandom(bitNumber);
                            break;
                        }

                        break;
                    }

                    case CHANNEL_VIEW:
                    {
                        u8 unitValue = index % 10;
                        // Track 1
                        if (index >= 11 && index <= 88 && unitValue >= 1 && unitValue <= 8)
                        {
                            setChannel(index);
                            break;
                        }
                    }
                    case RESOLUTION_VIEW:
                    {

                    }
                    case GATES_VIEW_FOR_FUN:
                    {

                    }
                }
            }
        }
        break;

        case TYPESETUP:
        {
            if (value)
            {
                toggleSyncMode();
                break;
            }
        }

        break;
    }
}

void setSequencerState(u8 state)
{
    sequencerState = state;
}

void setTempoIncrease()
{
    tempo++;
    setMsPerClock();
}

void setTempoDecrease()
{
    tempo--;
    setMsPerClock();
}

void setOctaveDown()
{
    tracks[currentTrack].octave--;
}

void setOctaveUp()
{
    tracks[currentTrack].octave++;
}

void setMuteValue(u8 track)
{
    tracks[track].isMuted ^= 1;
    if (tracks[track].isMuted)
    {
        killNotes(track);
    }
}

void selectTrack(u8 track)
{
    currentTrack = track;
}

void setEuclidPatternLength(u8 bitNumber)
{
    u8 bitMask = 1 << bitNumber;
    tracks[currentTrack].euclidSequenceLength ^= bitMask;
    calculateEuclideanRhythm();
}

void setEuclidDensity(u8 bitNumber)
{
    u8 bitMask = 1 << bitNumber;
    tracks[currentTrack].euclidDensity ^= bitMask;

    if (tracks[currentTrack].euclidDensity == 0)
    {
        killNotes(currentTrack);
    }
    calculateEuclideanRhythm();
}

void setEuclidOffset(u8 bitNumber)
{
    u8 bitMask = 1 << bitNumber;
    tracks[currentTrack].euclidOffset ^= bitMask;
    calculateEuclideanRhythm();
}

void setTuringMachinePatternLength(u8 bitNumber)
{
    u8 bitMask = 1 << bitNumber;
    tracks[currentTrack].turingMachineSequenceLength ^= bitMask;
}

void setTuringMachineScale(u8 bitNumber)
{
    u8 bitMask = 1 << bitNumber;
    tracks[currentTrack].turingMachineScale ^= bitMask;
}

void setTuringMachineRandom(u8 bitNumber)
{
    u8 bitMask = 1 << bitNumber;
    tracks[currentTrack].turingMachineRandom ^= bitMask;
}

void setChannel(u8 index)
{
    u8 track = 8 - (index / 10);
    u8 channel = index % 10;
    tracks[track].midiChannelsFlags ^= (1 << (channel - 1));
}

void toggleQuantizerBlackKeysValue(u8 note)
{
    u8 bitPosition = quantizerBlackKeysLookupTable[note - 21];

    if (!bitPosition) return;

    u16 bitMask = 1 << bitPosition;
    tracks[currentTrack].quantizerValues ^= bitMask;
    updateQuantizedNotesArray();
}

void toggleQuantizerWhiteKeysValue(u8 note)
{
    u16 bitMask = 1 << quantizerWhiteKeysLookupTable[note - 11];
    tracks[currentTrack].quantizerValues ^= bitMask;
    updateQuantizedNotesArray();
}

void updateQuantizedNotesArray()
{
    tracks[currentTrack].quantizedNotesArray = 0;

    for (u8 quantizerIndex = 0; quantizerIndex < 12; quantizerIndex++)
    {

        u8 noteOn = isFlagOn16(tracks[currentTrack].quantizerValues, quantizerIndex);

        if (!noteOn) { continue; }

        u8 bitPos = quantizerIndex;
        while (bitPos < 32)
        {
            tracks[currentTrack].quantizedNotesArray |= (1 << bitPos);
            bitPos += 12;
        }
    }
}

void calculateEuclideanRhythm()
{
    tracks[currentTrack].euclidSequenceFlags = 0;

    if (tracks[currentTrack].euclidDensity == 0) { return; }
    
    if (tracks[currentTrack].euclidDensity >= tracks[currentTrack].euclidSequenceLength)
    {
        tracks[currentTrack].euclidSequenceFlags = 0b11111111111111111111111111111111;
        return;
    }

    u8 position = 0;
    u8 stride = tracks[currentTrack].euclidSequenceLength - tracks[currentTrack].euclidDensity;
    for (u8 i = 0; i < tracks[currentTrack].euclidDensity; i++)
    {
        position = (position + stride + tracks[currentTrack].euclidOffset) % tracks[currentTrack].euclidSequenceLength;
        u8 bitMask = 1 << position;
        tracks[currentTrack].euclidSequenceFlags |= bitMask;
    }
}

u8 getTempo()
{
    return tempo;
}

void setMsPerClock()
{
    msPerClock = MS_PER_MIN / (CLOCK_RATE * tempo);
}

u8 getMsPerClock()
{
    return msPerClock;
}

void zeroCounters()
{
    for (u8 trackNumber = 0; trackNumber < NUM_TRACKS; trackNumber++)
    {
        tracks[trackNumber].euclidSequencePosition = 0;
        tracks[trackNumber].turingMachineSequencePosition = 0;
        tracks[trackNumber].resolutionCounter = 0;
    }
}

void handleMidiInput(u8 port, u8 status, u8 d1, u8 d2)
{
    if (status == MIDITIMINGCLOCK && port == DINMIDI)
    {
        if (getSyncMode() == EXTERNAL_CLOCK)
        {
            hal_plot_led(TYPESETUP, 0, 0, MAXLED, 0);
            handleNextPulse();
            return;
        }
    }

    if (status == MIDISTART)
    {
        setSequencerState(SEQUENCER_PLAYING);
    }

    if (status == MIDISTOP)
    {
        setSequencerState(SEQUENCER_STOPPED);
        zeroCounters();
    }
}

void handleNextPulse()
{
    if (sequencerState == SEQUENCER_STOPPED) { return; }

    for (u8 trackNumber = 0; trackNumber < NUM_TRACKS; trackNumber++)
    {
        if (tracks[trackNumber].resolutionCounter > tracks[trackNumber].resolution)
        {
            tracks[trackNumber].resolutionCounter = 0;

            if (!tracks[trackNumber].isMuted)
            {
                incrementSequencerTrack(trackNumber);
            }
        }
        tracks[trackNumber].resolutionCounter++;
    }
}

void incrementSequencerTrack(u8 trackNumber)
{
    u8 gate = nextGate(trackNumber);

    if (gate)
    {
        if (tracks[trackNumber].quantizerValues)
        { 
            u8 note = nextNote(trackNumber);
            playNote(trackNumber, note);
        }
    }
}

u8 nextGate(u8 trackNumber)
{
    u8 gate = isFlagOn32(tracks[trackNumber].euclidSequenceFlags, tracks[trackNumber].euclidSequencePosition + tracks[trackNumber].euclidOffset);

    tracks[trackNumber].euclidSequencePosition++;

    if (tracks[trackNumber].euclidSequencePosition >= tracks[trackNumber].euclidSequenceLength)
    {
        tracks[trackNumber].euclidSequencePosition = 0;
    }

    return gate;
}

u8 nextNote(u8 trackNumber)
{
    insertRandom(trackNumber);

    u8 note = tracks[trackNumber].turingMachineSequenceShiftRegister << tracks[trackNumber].turingMachineSequencePosition;

    tracks[trackNumber].turingMachineSequencePosition++;

    if (tracks[trackNumber].turingMachineSequencePosition >= tracks[trackNumber].turingMachineSequenceLength)
    {
        tracks[trackNumber].turingMachineSequencePosition = 0;
    }

    return getQuantizedNote(trackNumber, note);
}

void insertRandom(u8 trackNumber)
{
    u8 random = rand() % 32;
    if (random < tracks[trackNumber].turingMachineRandom)
    {
        u32 bitMask = 1 << tracks[trackNumber].turingMachineSequencePosition;
        tracks[trackNumber].turingMachineSequenceShiftRegister ^= bitMask;
    }
}

u8 getQuantizedNote(u8 trackNumber, u8 note)
{
    if (tracks[trackNumber].quantizedNotesArray == 0)
    {
        // return -1 to signify there is no note to play
        return -1;
    }

    u8 truncatedNote = note % (tracks[trackNumber].turingMachineScale + 1);

    for (u8 i = 0; i < NOTES_MAX_RANGE; i++)
    {
        s8 bitPos = truncatedNote + i;
        // Forwards scanning
        if (isFlagOn32(tracks[trackNumber].quantizedNotesArray, bitPos))
        {
            return bitPos + (tracks[trackNumber].octave * 12);
        }

        // Backwards scanning
        bitPos = truncatedNote - 1;
        if (bitPos && isFlagOn32(tracks[trackNumber].quantizedNotesArray, bitPos))
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
        killNote(channel, tracks[trackNumber].previousNote);

        // if note is -1, there is no note to play
        if (note < 0) { continue; }

        // play new note
        hal_send_midi(DINMIDI, NOTEON | channel, note, 127);
        hal_send_midi(USBMIDI, NOTEON | channel, note, 127);

        // store off triggered note for next time
        tracks[trackNumber].previousNote = note;
    }
}

void killNote(u8 channel, u8 note)
{
    hal_send_midi(DINMIDI, NOTEOFF | channel, note, 0);
    hal_send_midi(USBMIDI, NOTEOFF | channel, note, 0);
}

void killNotes(u8 trackNumber)
{
    for (u8 channel = 0; channel < 8; channel++)
    {
        if(!isFlagOn8(tracks[trackNumber].midiChannelsFlags, channel)) { continue; }

        // kill previous note
        killNote(channel, tracks[trackNumber].previousNote);
    }
}

void updateUi()
{
    renderMuteButtons();
    renderTrackSelectButtons();
    renderTempoButtons();
    renderOctaveButtons();
    renderViewButtons();

    if (currentPage == TRACK_VIEW)
    {
        renderEuclidPatternLength();
        renderEuclidDensity();
        renderEuclidOffset();
        renderTuringMachinePatternLength();
        renderTuringMachineScale();
        renderTuringMachineRandom();
        renderQuantizer();
    }

    if (currentPage == CHANNEL_VIEW)
    {
        renderChannels();
    }

    if (currentPage == RESOLUTION_VIEW)
    {

    }

    if (currentPage == GATES_VIEW_FOR_FUN)
    {

    }

    // STRETCH
    // render gates view to pads to provide some visual feedback
}

void renderMuteButtons()
{
    for (u8 trackNumber = 0; trackNumber < NUM_TRACKS; trackNumber++)
    {
        u8 padNumber = 9 + ((8 - trackNumber) * 10);
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
        u8 isNoteOn = isFlagOn16(tracks[currentTrack].quantizedNotesArray, i);
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
        hal_plot_led(TYPEPAD, 80 - (trackNumber * 10), tracks[trackNumber].red, tracks[trackNumber].green, tracks[trackNumber].blue);
    }
}

void renderEuclidPatternLength()
{
    for (u8 bitNumber = 0; bitNumber < 6 ; bitNumber++)
    {
        if (isFlagOn8(tracks[currentTrack].euclidSequenceLength, bitNumber))
        {
            hal_plot_led(TYPEPAD, 88 - bitNumber, tracks[currentTrack].red, tracks[currentTrack].green, tracks[currentTrack].blue);
        }
        else
        {
            hal_plot_led(TYPEPAD, 88 - bitNumber, MAXLED, MAXLED, MAXLED);
        }
    }
}

void renderEuclidDensity()
{
    for (u8 bitNumber = 0; bitNumber < 6 ; bitNumber++)
    {
        if (isFlagOn8(tracks[currentTrack].euclidDensity, bitNumber))
        {
            hal_plot_led(TYPEPAD, 78 - bitNumber, tracks[currentTrack].red, tracks[currentTrack].green, tracks[currentTrack].blue);
        }
        else
        {
            hal_plot_led(TYPEPAD, 78 - bitNumber, MAXLED, MAXLED, MAXLED);
        }
    }
}

void renderEuclidOffset()
{
    for (u8 bitNumber = 0; bitNumber < 6 ; bitNumber++)
    {
        if (isFlagOn8(tracks[currentTrack].euclidOffset, bitNumber))
        {
            hal_plot_led(TYPEPAD, 68 - bitNumber, tracks[currentTrack].red, tracks[currentTrack].green, tracks[currentTrack].blue);
        }
        else
        {
            hal_plot_led(TYPEPAD, 68 - bitNumber, MAXLED, MAXLED, MAXLED);
        }
    }
}

void renderTuringMachinePatternLength()
{
    for (u8 bitNumber = 0; bitNumber < 6 ; bitNumber++)
    {
        if (isFlagOn8(tracks[currentTrack].turingMachineSequenceLength, bitNumber))
        {
            hal_plot_led(TYPEPAD, 58 - bitNumber, tracks[currentTrack].red, tracks[currentTrack].green, tracks[currentTrack].blue);
        }
        else
        {
            hal_plot_led(TYPEPAD, 58 - bitNumber, MAXLED, MAXLED, MAXLED);
        }
    }
}

void renderTuringMachineScale()
{
    for (u8 bitNumber = 0; bitNumber < 6 ; bitNumber++)
    {
        if (isFlagOn8(tracks[currentTrack].turingMachineScale, bitNumber))
        {
            hal_plot_led(TYPEPAD, 48 - bitNumber, tracks[currentTrack].red, tracks[currentTrack].green, tracks[currentTrack].blue);
        }
        else
        {
            hal_plot_led(TYPEPAD, 48 - bitNumber, MAXLED, MAXLED, MAXLED);
        }
    }
}

void renderTuringMachineRandom()
{
    for (u8 bitNumber = 0; bitNumber < 6 ; bitNumber++)
    {
        if (isFlagOn8(tracks[currentTrack].turingMachineRandom, bitNumber))
        {
            hal_plot_led(TYPEPAD, 38 - bitNumber, tracks[currentTrack].red, tracks[currentTrack].green, tracks[currentTrack].blue);
        }
        else
        {
            hal_plot_led(TYPEPAD, 38 - bitNumber, MAXLED, MAXLED, MAXLED);
        }
    }
}

void renderTempoButtons()
{
    hal_plot_led(TYPEPAD, 91, MAXLED, MAXLED, MAXLED);
    hal_plot_led(TYPEPAD, 92, MAXLED, MAXLED, MAXLED);
}

void renderOctaveButtons()
{
    hal_plot_led(TYPEPAD, 93, MAXLED, MAXLED, MAXLED);
    hal_plot_led(TYPEPAD, 94, MAXLED, MAXLED, MAXLED);
}

void renderChannels()
{
    for (u8 trackNumber = 0; trackNumber < NUM_TRACKS; trackNumber++)
    {
        for (u8 channelNumber = 0; channelNumber < 8; channelNumber++)
        {
            u8 isChannelOn = isFlagOn8(tracks[trackNumber].midiChannelsFlags, channelNumber);

            u8 padNumber = ((8 - trackNumber) * 10) + (1 + channelNumber);

            if (isChannelOn)
            {
                hal_plot_led(TYPEPAD, padNumber, tracks[trackNumber].red, tracks[trackNumber].green, tracks[trackNumber].blue);
            }
            else
            {
                hal_plot_led(TYPEPAD, padNumber, MAXLED, MAXLED, MAXLED);
            }
        }
    }
}

void renderViewButtons()
{
    for (u8 view = 0; view < 4; view++)
    {
        if (view == currentPage)
        {
            hal_plot_led(TYPEPAD, 95 + view, MAXLED, MAXLED, MAXLED);
        }
        else
        {
            hal_plot_led(TYPEPAD, 95 + view, 16, 16, 16);
        }

    }
}

void clearAllPads()
{
    for (u8 i = 1; i < 99; i ++)
    {
        hal_plot_led(TYPEPAD, i, 0, 0, 0);
    }
}