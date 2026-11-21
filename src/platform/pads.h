#ifndef PADS_H
#define PADS_H

#include "app_defs.h"

/* Per-pad peak ADC value (12 bit), kept by the vendor library (cx_pad.o).
 * Declared here with the app's u16 (unsigned short on both the firmware and
 * the host simulator) so the app does not need the ST/USB headers.
 * The simulator defines it in tools/sim_midi2.c. */
extern u16 ADC_Max[64];

#endif
