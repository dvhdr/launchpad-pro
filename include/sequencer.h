#ifndef SEQUENCER_H
#define SEQUENCER_H

#include "app.h"

// Sequencer
#define MS_PER_MIN 60000
#define CLOCK_RATE 24 // 4 == 16th notes, 8 == 32nd notes, 24 == fuck_knows
#define NUM_TRACKS 8
#define EXTERNAL_CLOCK 0
#define INTERNAL_CLOCK 1
#define DEFAULT_OCTAVE 3
#define SEQUENCER_STOPPED 0
#define SEQUENCER_PLAYING 1

// UI
#define TRACK_VIEW 0
#define CHANNEL_VIEW 1
#define RESOLUTION_VIEW 2
#define GATES_VIEW_FOR_FUN 3

void initialize();

u8 getSyncMode();

void toggleSyncMode();

void setSequencerState(u8 state);

void setCurrentView(u8 view);

void handlePadEvent(u8 type, u8 index, u8 value);

void setTempoIncrease();

void setTempoDecrease();

void setOctaveDown();

void setOctaveUp();

void setMuteValue(u8 track);

void selectTrack(u8 track);

void setEuclidPatternLength(u8 bitNumber);

void setEuclidDensity(u8 bitNumber);

void setEuclidOffset(u8 bitNumber);

void setTuringMachinePatternLength(u8 bitNumber);

void setTuringMachineScale(u8 bitNumber);

void setTuringMachineRandom(u8 bitNumber);

void setChannel(u8 index);

void toggleQuantizerBlackKeysValue(u8 note);

void toggleQuantizerWhiteKeysValue(u8 note);

void updateQuantizedNotesArray();

void calculateEuclideanRhythm();

u8 getTempo();

void setMsPerClock();

u8 getMsPerClock();

void zeroCounters();

void handleMidiInput(u8 port, u8 status, u8 d1, u8 d2);

void handleNextPulse();

void incrementSequencerTrack(u8 trackNumber);

u8 nextGate(u8 trackNumber);

u8 nextNote(u8 trackNumber);

void insertRandom(u8 trackNumber);

u8 getQuantizedNote(u8 trackNumber, u8 note);

void playNote(u8 trackNumber, u8 note);

void killNote(u8 trackNumber, u8 note);

void killNotes(u8 trackNumber);

void updateUi();

void renderMuteButtons();

void renderQuantizer();

void renderTrackSelectButtons();

void renderEuclidPatternLength();

void renderEuclidDensity();

void renderEuclidOffset();

void renderTuringMachinePatternLength();

void renderTuringMachineScale();

void renderTuringMachineRandom();

void renderTempoButtons();

void renderOctaveButtons();

void renderChannels();

void renderViewButtons();

void clearAllPads();

#endif