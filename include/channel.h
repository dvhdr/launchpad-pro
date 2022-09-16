#ifndef CHANNEL_H
#define CHANNEL_H

#include "app.h"

#define SEQUENCE_MAX_LENGTH 32
#define NOTES_MAX_RANGE 32

typedef struct Track
{
    // Gates
    u32 euclidSequenceFlags;
    u8 euclidSequenceLength;
    u8 euclidSequencePosition;

    u8 euclidDensity;
    u8 euclidOffset;

    // Notes
    u32 turingMachineSequenceShiftRegister;
    u8 turingMachineSequenceLength;
    u8 turingMachineSequencePosition;

    u8 turingMachineScale;
    u8 turingMachineRandom;

    u8 previousNote;
    u16 quantizerValues;

    u32 quantizedNotesArray;

    // Misc
    u8 midiChannelsFlags;
    u8 octave;
    u8 isMuted;
    u8 resolution;
    u8 resolutionCounter;

    // UI
    u8 uiEuclidSequenceLength;
    u8 uiEuclidDensity;
    u8 uiEuclidOffset;

    u8 uiTuringMachineSequenceLength;
    u8 uiTuringMachineSequenceScale;
    u8 uiTuringMachineSequenceRandom;

    u16 uiQuantizer;

    u8 red;
    u8 green;
    u8 blue;
} Track;

#endif