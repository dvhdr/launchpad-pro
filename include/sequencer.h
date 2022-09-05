#ifndef SEQUENCER_H
#define SEQUENCER_H

#include "app.h"

// Sequencer
#define MS_PER_MIN 60000
#define CLOCK_RATE 4 // 4 == 16th notes, 8 == 32nd notes
#define NUM_TRACKS 8

// UI
#define TRACK_VIEW 0
#define CHANNEL_VIEW 1

static u8 tempo = 100;

void initialize();

void handlePadEvent(u8 type, u8 index, u8 value);

void setMuteValue(u8 track);

void setQuantizerBlackKeysValue(u8 note);

void setQuantizerWhiteKeysValue(u8 note);

void updateQuantizedNotesArray();

u8 getTempo();

u8 getMsPerClock();

void handleNextPulse();

void incrementSequencerTrack(u8 trackNumber);

u8 nextGate(u8 trackNumber);

u8 nextNote(u8 trackNumber);

u8 getQuantizedNote(u8 trackNumber, u8 note);

void playNote(u8 trackNumber, u8 note);

void updateUi();

void renderMuteButtons();

#endif