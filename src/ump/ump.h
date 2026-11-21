#ifndef UMP_H
#define UMP_H

#include <stdint.h>

/* ---- codec section: UMP / MIDI 2.0 Protocol encode, decode, translation ---- */

/* Number of 32-bit words of the UMP whose first word is word0 (by message type). */
uint8_t ump_word_count(uint32_t word0);

/* Min-Center-Max upscaling (UMP spec) and plain truncating downscale. */
uint32_t ump_scale_up(uint32_t v, uint8_t src_bits, uint8_t dst_bits);
uint32_t ump_scale_down(uint32_t v, uint8_t src_bits, uint8_t dst_bits);

/* MIDI 2.0 channel voice (MT 4) encoders; return words written (2). */
uint8_t ump_m2_note_on(uint32_t out[2], uint8_t group, uint8_t ch, uint8_t note, uint16_t vel);
uint8_t ump_m2_note_off(uint32_t out[2], uint8_t group, uint8_t ch, uint8_t note, uint16_t vel);
uint8_t ump_m2_poly_pressure(uint32_t out[2], uint8_t group, uint8_t ch, uint8_t note, uint32_t value);
uint8_t ump_m2_channel_pressure(uint32_t out[2], uint8_t group, uint8_t ch, uint32_t value);

/* MIDI 1.0 message -> UMP (MT 1 system, MT 2 or MT 4 channel voice).
 * Returns words written, 0 for SysEx bytes or an invalid status. */
uint8_t ump_from_midi1(uint32_t out[2], uint8_t group, uint8_t status, uint8_t d1, uint8_t d2,
                       uint8_t as_midi2);

/* UMP (MT 1, 2, 4) -> up to max_out 3-byte MIDI 1.0 messages. Returns the count
 * (0 if untranslatable or if max_out is too small for the whole translation). */
uint8_t ump_to_midi1(const uint32_t *msg, uint8_t out[][3], uint8_t max_out);

/* SysEx7: data includes F0..F7 (stripped). Returns words written, 0 if it won't fit. */
uint16_t ump_sysex7_pack(uint32_t *out, uint16_t max_words, uint8_t group, const uint8_t *data,
                         uint16_t len);

struct ump_sysex7_rx {
	uint8_t buf[320];
	uint16_t len;
	uint8_t active;
};

void ump_sysex7_rx_reset(struct ump_sysex7_rx *rx);
/* 1 = complete (buf = F0..F7), 0 = in progress, -1 = discarded/error. */
int ump_sysex7_rx_feed(struct ump_sysex7_rx *rx, const uint32_t msg[2]);

/* ---- TX section: prioritised queue for USB EP1 IN ---- */

enum ump_tx_class { UMP_TX_CONTROL, UMP_TX_NOTE, UMP_TX_PRESSURE };

#define UMP_TX_CONTROL_WORDS 192
#define UMP_TX_NOTE_WORDS 256
#define UMP_TX_PRESSURE_SLOTS 64

void ump_tx_reset(void);
/* 0 on success, -1 if full (all-or-nothing). UMP_TX_PRESSURE is not allowed here. */
int ump_tx_push(enum ump_tx_class cls, const uint32_t *msg, uint8_t words);
/* Accepts only if free note words >= words + reserve + 2; then reserve += 2.
 * The reserve is counted in 2-word units per outstanding attack and is not keyed per note:
 * callers must send a release only for an accepted attack. */
int ump_tx_push_attack(const uint32_t *msg, uint8_t words);
/* Release is 1 or 2 words. Succeeds while reserve >= 2 (reserve -= 2 whatever its size); drops pending pressure for the key. */
int ump_tx_push_release(const uint32_t *msg, uint8_t words);
/* note 0xFF = channel pressure. Coalesces by key; drops silently if no slot is free. */
void ump_tx_push_pressure(uint8_t group, uint8_t ch, uint8_t note, const uint32_t msg[2]);
/* Pops whole messages (control, then notes, then pressure) that fit in 64 bytes.
 * Writes little-endian words; returns the byte count. */
uint8_t ump_tx_fill_packet(uint8_t out[64]);
uint16_t ump_tx_reserve_words(void);

/* ---- stream section: UMP Stream (MT 0xF) responder ---- */

#define UMP_PROTO_MIDI1 0x01
#define UMP_PROTO_MIDI2 0x02

/* Protocol = UMP_PROTO_MIDI2. Does not clear the protocol hook. */
void ump_stream_reset(void);
uint8_t ump_stream_protocol(void);
/* Called with the old protocol just before the protocol changes. */
void ump_stream_set_protocol_hook(void (*fn)(uint8_t old_proto));
/* Handles one MT 0xF message, queuing replies (UMP_TX_CONTROL).
 * Returns 1 if the protocol changed. */
int ump_stream_handle(const uint32_t msg[4]);

#endif
