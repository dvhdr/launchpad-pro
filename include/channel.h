#include "app.h"

#define SEQUENCE_MAX_LENGTH 32

struct Track
{
    // Gates
    u32 euclidSequenceFlags;
    u8 euclidSequenceLength;

    u8 euclidDensity;
    u8 euclidOffset;

    // Notes
    u32 turingMachineSequenceArray[SEQUENCE_MAX_LENGTH];
    u8 turingMachineSequenceLength;

    u8 turingMachineScale;
    u8 turingMachineRandom;

    u8 previousNote;

    u32 quantizedNotesArray;

    // Misc
    u8 midiChannelsFlags;

    // UI
    u8 uiEuclidSequenceLength;
    u8 uiEuclidDensity;
    u8 uiEuclidOffset;

    u8 uiTuringMachineSequenceLength;
    u8 uiTuringMachineSequenceScale;
    u8 uiTuringMachineSequenceRandom;

    u16 uiQuantizer;
};