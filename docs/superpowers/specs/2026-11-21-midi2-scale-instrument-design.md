# Launchpad Pro: USB MIDI 2.0 scale instrument — design

Date: 2026-11-21
Status: draft, awaiting approval

## Intent

Turn the open Launchpad Pro firmware into a dedicated, playable scale instrument
that talks **USB MIDI 2.0** to a Windows PC (Windows MIDI Services), so that pad
**velocity and pressure reach the host at far higher resolution** than MIDI 1.0's
7 bits. The note layouts and scale selection are modelled on **Launchpad95**'s
Instrument/Scale mode. A settings menu lets the player choose between two
velocity algorithms and tune other playing options.

Why: the pads' sensors are 12-bit, but the stock path squeezes velocity and
pressure into 0–127. MIDI 2.0 carries 16-bit velocity and 32-bit per-note
pressure natively, and Windows now has an in-box USB MIDI 2.0 driver.

## Goals

1. The device enumerates as a USB MIDI 2.0 device (USB MIDI class 2.0,
   alternate setting 1) and still enumerates and works as a plain MIDI 1.0
   device on hosts without MIDI 2.0 support (alternate setting 0, unchanged
   from today's two-port behaviour).
2. In MIDI 2.0 mode, pad hits send MIDI 2.0 Note On with 16-bit velocity
   derived from the raw 12-bit sensor data, and pad pressure is sent as MIDI 2.0
   Poly Pressure (32-bit) or Channel Pressure (32-bit), per the settings.
3. Windows MIDI Services shows the device with a proper name and two function
   blocks, via UMP Endpoint Discovery (stream messages).
4. Launchpad95-style scale play: the 8×8 grid maps to notes of a chosen root +
   scale in a chosen layout, coloured by role (root / in-scale / highlighted
   degree / out-of-scale).
5. A settings menu on the device, opened from the Setup button, with a Scale
   page and an Options page. Settings persist across power cycles.
6. Both velocity algorithms are available and selectable in the menu:
   **Peak-window** and **Zero-latency** (definitions below).
7. Existing open-firmware apps keep working: `include/app.h` and its ABI are
   not changed; `hal_send_midi` / `hal_send_sysex` / `app_midi_event` /
   `app_sysex_event` keep working in both USB modes.

## Non-goals

- MPE mode (explicitly declined).
- Drum-rack layout and note repeat from Launchpad95.
- Ableton Live integration, session/clip control.
- MIDI-CI, Property Exchange, Profiles.
- MIDI 2.0 over the DIN ports (DIN stays MIDI 1.0).
- Validating on macOS/Linux. The implementation follows the USB MIDI 2.0
  spec so it should work there, but Windows is the target and test host.
- Modifying the closed library beyond swapping out the three USB objects
  named below.

## Anti-goals (would count as failure even if it "works")

- The device stops working on a host without MIDI 2.0 support.
- Zero-latency mode adds any latency over today's firmware.
- The firmware wears out flash (settings writes must be debounced).
- GPL code from Launchpad95 lands in this BSD-licensed repo. Behaviour is
  reimplemented from its documentation; scale interval sets are music theory.
- The device becomes unrecoverable. (It shouldn't: firmware upload happens in
  the separate bootloader, which this work does not touch.)

## Constraints

- MCU: STM32F103RB, 20 KB RAM, app flash 128 KB at `0x08006400`
  (`stm32_flash.ld`). USB full-speed, 64-byte bulk packets, EP1 OUT/IN.
- The USB stack lives in the closed `lib/launchpad_pro.a`. It is ST's standard
  USB-FS device library split into separate archive members
  (`usb_core.o`, `usb_init.o`, `usb_int.o`, `usb_regs.o`, `usb_sil.o`,
  `usb_mem.o`, `usb_desc.o`, `usb_prop.o`, `usb_endp.o`, `usb_istr.o`,
  `usb_pwr.o`, `usb.o`) plus Novation's MIDI glue `c_midi.o`.
- Today's descriptors: VID `0x1235`, PID `0x4051`, bcdDevice `0x0200`,
  product "Launchpad Open", one Audio Control interface + one MIDIStreaming
  interface with two embedded/external jack pairs ("Standalone Port",
  "MIDI Port"), bulk EP1 OUT (`0x01`) and EP1 IN (`0x81`), 64 bytes.
  Endpoint PMA layout in `Device_Reset`: EP0 RX `0x40`, EP0 TX `0x48`,
  EP1 TX `0x90`, EP1 RX `0x50`.
- `app.h` says not to modify it. New APIs go in a new header.
- The raw ADC frame buffer (64 × u16, 12-bit, ADC_MAP indexing) is passed to
  `app_init()` and is the source for high-resolution values. Velocity and
  pressure from the library callbacks are 7-bit.
- Windows caches USB descriptors per VID/PID/bcdDevice; bcdDevice must change.
- The build currently fails because the `tools/libintelhex` submodule is not
  checked out.

## Approaches considered

**USB transport (one sensible way).**

- *Chosen: link-time replacement.* The Makefile builds a trimmed copy of
  `launchpad_pro.a` without `usb_desc.o`, `usb_prop.o` and `usb_endp.o`, and
  links our own source in their place. Everything else (USB core, interrupts,
  pad scanning, LEDs, DIN, flash) stays the library's.
- Rejected: binary-patching the library (fragile, opaque); staying on MIDI
  1.0 with 14-bit CC/NRPN tricks (doesn't meet the goal); replacing the whole
  USB stack (needless risk).

**Velocity (both kept; selectable).**

Finding that shaped this: the library's `cx_pad.o` already captures a running
per-pad peak (`ADC_Max`, 64 × u16, a linkable global) before it raises the
press event, then squeezes it to 7 bits with its own curve. So:

- *Zero-latency*: on the library's press event, read that pad's `ADC_Max`
  peak (12-bit) at once and turn it into 16-bit velocity. Full sensor
  resolution, no added delay beyond today's firmware. (Revised from the
  earlier "interpolate within the 7-bit bucket" idea, which turned out to be
  meaningless once the library's own peak capture was found.)
- *Peak-window* (default): send nothing at the press event. Keep sampling that
  pad's raw ADC for a short window afterwards (default 2 ms, tunable constant)
  and use the highest of `ADC_Max` and the window peak. This catches presses
  that keep rising after the library's trigger point (slow or "pushed"
  strikes), giving the most even response. Adds about 2 ms of note-on latency.
- In both modes the menu curve is applied to the raw peak. The library's
  7-bit curve is bypassed, except that its 7-bit value is used for DIN and
  MIDI 1.0 fallback when the curve is "Linear".
  *(Builder's call; if reading `ADC_Max` proves unreliable on the device,
  Zero-latency falls back to the raw `adc_buffer` reading at the press event.)*

The player chose: implement both, selectable in the menu.

**Structure (builder's call).** The MIDI 2.0 layer (`src/usb`, `src/ump`,
`include/midi2.h`) is a reusable platform underneath `app.h`. The scale
instrument *is* the app: it replaces `src/app.c`. Goal 7 means other apps built
from this tree, without the instrument, still compile and work unchanged.

## Design

### 1. USB descriptors and setup handling (`src/usb/`)

- Device descriptor as today but bcdDevice bumped (e.g. `0x0300`), product
  string changed to "Launchpad Pro MIDI 2.0" (builder may tweak wording).
- Config descriptor: Audio Control interface 0 unchanged; MIDIStreaming
  interface 1 with:
  - **alt 0** — byte-for-byte today's MIDI 1.0 class-specific descriptors
    (bcdMSC `0x0100`, 2 cables) and endpoints.
  - **alt 1** — bcdMSC `0x0200`, class-specific MS header, bulk EP `0x01` /
    `0x81` 64 bytes each with `CS_ENDPOINT` `MS_GENERAL_2_0` descriptors
    listing their Group Terminal Block IDs.
- **Group Terminal Block descriptors** served for `GET_DESCRIPTOR`
  (type `CS_GR_TRM_BLOCK` = 0x26) on interface 1 alt 1. Two bidirectional
  blocks: block 1 "Standalone Port" → Group 1, block 2 "MIDI Port" → Group 2,
  default protocol MIDI 2.0.
- `usb_prop.c` reimplements the ST `Device_Property`, `Device_Table` and
  `User_Standard_Requests` tables (same symbol names as the removed object),
  preserving today's `Device_Reset` endpoint/PMA setup and the calls to
  `USBConfigured` / `USBUnconfigured`, and adding:
  - `Get_Interface_Setting` accepting alt 0 and 1 on interface 1;
  - `SET_INTERFACE` handling that records the active alt and resets the
    stream state;
  - interface-recipient `GET_DESCRIPTOR` for the GTB descriptor.
- `usb_endp.c` reimplements `EP1_OUT_Callback`: in alt 0 it behaves exactly
  as the original (push bytes to the `MIDIToDev` fifo, NAK RX when the fifo
  is busy so `c_midi` re-validates it); in alt 1 it hands 32-bit UMP words to
  the UMP receive path.

### 2. UMP layer (`src/ump/`)

- **Receive:** parse UMP words by message type.
  - Type 0xF stream messages: answer Endpoint Discovery with Endpoint Info
    (UMP v1.1, 2 static function blocks, supports MIDI 1.0 + 2.0 protocols),
    Device Identity (Novation manufacturer ID `00 20 29`), Endpoint Name,
    Product Instance ID; answer Function Block Discovery with FB Info and FB
    Name for both blocks; accept Stream Configuration Request for MIDI 2.0 or
    MIDI 1.0 protocol and reply with Stream Configuration Notification.
  - Type 2 (MIDI 1.0 channel voice) and type 4 (MIDI 2.0 channel voice):
    convert down to 3-byte MIDI 1.0 and call `app_midi_event` with port
    `USBSTANDALONE` (group 1) or `USBMIDI` (group 2).
  - Type 3 (SysEx7): reassemble (≤ 320 bytes, matching the existing limit)
    and call `app_sysex_event`.
  - Type 1 system: pass through as MIDI 1.0 realtime/common.
- **Transmit:** a small TX ring of UMP words flushed to EP1 IN in 64-byte
  packets from the 1 kHz path (and from the TX-complete interrupt if
  available). Must never touch EP1 while in alt 0.
- **Existing-app compatibility:** the linker's `--wrap` option redirects app
  calls to `hal_send_midi` / `hal_send_sysex`. Wrapped versions: DIN or alt 0
  → call the real library function; alt 1 → encode as UMP (MIDI 2.0 channel
  voice with upscaled values when the negotiated protocol is 2.0, otherwise
  type 2; SysEx as type 3).
- **New API** in `include/midi2.h` (new file; `app.h` untouched):
  - `u8 midi2_active(void)` — 1 when the host selected alt 1;
  - `void hal_send_note_on2(u8 port, u8 ch, u8 note, u16 velocity)`,
    `hal_send_note_off2(...)`, `hal_send_poly_pressure2(u8 port, u8 ch,
    u8 note, u32 value)`, `hal_send_channel_pressure2(...)`. Each falls back
    to the 7-bit MIDI 1.0 equivalent automatically when MIDI 2.0 isn't
    active.
- **Known conflict with `c_midi.o`:** `usbmidi_poll` → `usbmidi_tx` writes
  EP1 IN whenever its outgoing fifo (`MIDIToHst`) has data. `main.o` calls
  `midi_flush_windows_buffer` directly (not through `hal_send_midi`), and
  that puts a MIDI 1.0 "Windows flush" message into the fifo after
  configuration. `c_midi.o` also calls `SetEPRxValid` on EP1 by itself. In
  alt 1, all three must be neutralised, by `--wrap` of `usbmidi_poll` and
  `midi_flush_windows_buffer` (called from `main.o`, so wrappable) and by
  clearing `MIDIToHst` on the switch to alt 1. If hardware testing shows the
  wraps aren't enough, `c_midi.o` gets replaced in full.

### 3. Pad engine (`src/pads/`)

- Keeps the `adc_buffer` pointer from `app_init`.
- Per-pad state: idle → capturing (peak-window) → held → released.
- **Velocity → 16 bits** (algorithms above). Curves: Soft / Linear / Hard,
  applied to the 16-bit value. Option "Fixed" sends a constant full velocity.
- **Pressure → 32 bits** from raw ADC: subtract a threshold (menu-selectable
  Low/Med/High), scale to 32 bits. Send only on change beyond a small
  hysteresis, rate-limited per pad (≈ 200 Hz cap) to protect USB bandwidth.
  Mode Poly / Channel (max of held pads) / Off.
- MIDI 1.0 fallback (alt 0 or DIN mirror): 7-bit velocity = top 7 bits of the
  16-bit value; pressure = library's 7-bit aftertouch.

### 4. Scale engine (`src/scales/`)

- **Scales** (the Live/Launchpad95 list, our own table of semitone sets):
  Major, Minor, Dorian, Mixolydian, Lydian, Phrygian, Locrian, Diminished,
  Whole-half, Whole Tone, Minor Blues, Minor Pentatonic, Major Pentatonic,
  Harmonic Minor, Melodic Minor, Super Locrian, Bhairav, Hungarian Minor,
  Minor Gypsy, Hirojoshi, In-Sen, Iwato, Kumoi, Pelog, Spanish (25 scales).
  Interval data comes from standard music-theory definitions, not from
  Launchpad95 source (which reads them from Live at runtime anyway).
- **Layouts:**
  - In-key 4ths (default): each row up = +3 scale degrees, each pad right =
    +1 degree. Only scale notes are on the grid (the "quantized" layouts).
  - In-key 3rds: row up = +2 degrees.
  - In-key sequential: rows continue on from each other (row up = +8 degrees).
  - Chromatic 4ths: semitones left/right, row up = +5 semitones; out-of-scale
    pads shown grey.
  - Chromatic guitar: like chromatic 4ths, but the 4th→5th row step is +4
    semitones (a guitar's B string).
  - *Absolute root* toggle: off (default) = bottom-left pad is the root;
    on = the layout is anchored to C and the root is only shown in colour.
- **Colours** (0–63 per channel): root bright blue; in-scale dim blue;
  3rd and 5th degree lighter blue; out-of-scale dark grey; out of MIDI
  range off. A held pad lights white, and **every other pad sounding the
  same pitch lights too** (useful because 4ths layouts repeat notes).
- Pressing a pad plays the mapped note on the configured MIDI channel; notes
  out of 0–127 do nothing.

### 5. User interface

Button numbering is from `app.h` (bottom-left pad = 11, top row of buttons
91–98, Setup button = `TYPESETUP`).

**Play page (default):**
- 8×8 grid = scale layout.
- 91 / 92 (top-row up/down arrows): octave up/down.
- **Octave model (builder's call):** naming follows Ableton/Launchpad95, where
  C3 = MIDI note 60. The stored octave is 0–7 and maps to octaves −1 … 6. In
  non-absolute-root mode the bottom-left pad = root note in that octave,
  `12 × (stored + 1) + root` (stored 3 → C3 = 60, the default). In
  absolute-root mode it is C in that octave. The arrows and the 8 octave pads
  on the Scale page cover the same range.
- 93 / 94 (left/right arrows): root down/up by one semitone.
- Other buttons unused for now (unlit).

**Settings menu:** tap Setup to toggle in/out. Holding Setup for more than
~300 ms and releasing returns to play (hold-to-peek). Top-row buttons 95 /
96 choose the **Scale** page and the **Options** page; the selected page's
button is lit.

*Scale page* (Launchpad95 arrangement, rows counted from the top):
- Row 1 (81–88): pad 81 = absolute-root toggle; 83–87 = layouts
  (chromatic guitar, in-key sequential, in-key 3rds, in-key 4ths,
  chromatic 4ths).
- Rows 2–3: root note as a piano keyboard. Row 2 = sharps C# D# · F# G# A#,
  row 3 = naturals C D E F G A B.
- Row 4: octave 0–7.
- Rows 5–8: scales (25 pads, 8 per row; pad index = scale index).
- Colours: root keys green (selected bright, others dim), scales blue,
  octave red, layouts amber.

*Options page* (one option per row, one pad per value, selected value
bright):
- Row 1: velocity mode — Peak-window / Zero-latency / Fixed.
- Row 2: velocity curve — Soft / Linear / Hard.
- Row 3: pressure mode — Poly / Channel / Off.
- Row 4: pressure threshold — Low / Med / High.
- Row 5: DIN mirror — Off / On (sends notes and pressure to the DIN out as
  MIDI 1.0).
- Rows 7–8: MIDI channel 1–16.
- Status (read-only): Setup LED colour shows the USB mode — green = MIDI 2.0
  active, amber = MIDI 1.0 fallback.

### 6. Persistence

- Settings (scale, root, octave, layout, absolute root, all options) are
  stored in the 1 KB user flash area with a magic/version byte. Invalid or
  blank flash (0xFF) → defaults.
- Writes are debounced: saved once when leaving the menu, and only if
  something changed.

### 7. Build and tests

- Makefile: init the libintelhex submodule if missing; produce
  `build/launchpad_pro_trimmed.a` with `arm-none-eabi-ar d` of the three
  USB objects; compile the new `src/**` sources; add `-Wl,--wrap=hal_send_midi
  -Wl,--wrap=hal_send_sysex`.
- `make test`: host-side (gcc) unit tests with no hardware:
  - scale/layout mapping (every layout × a sample of scales and roots;
    in-key layouts contain only scale notes);
  - UMP encoding/decoding and the down-conversion to MIDI 1.0;
  - stream message replies (byte-exact against the UMP 1.1 spec);
  - descriptor sanity: walk the config descriptor and check that every
    `bLength` sum matches `wTotalLength`, the alt 0 block is byte-identical to
    today's, and the GTB descriptor is well-formed;
  - velocity and pressure math (monotonic, full-range, curve endpoints);
  - settings serialise/deserialise, including blank flash.
- The existing simulator builds and runs.
- The ARM firmware links and fits flash/RAM; report sizes.
- **Hardware checklist** for the player (we have no device here): flash
  via the bootloader; on Windows with MIDI Services, check that the endpoint
  appears as MIDI 2.0 with two function blocks and the right name (e.g.
  `midi endpoint list`); monitor UMP and see 16-bit velocities and 32-bit
  pressure; check that MIDI 1.0 apps still see the ports; check MIDI 1.0
  fallback on a machine without MIDI Services; check the velocity feel of
  both modes.

## Delivery stages

The player has the hardware and will test on Windows at the checkpoint.

1. **Transport milestone (hardware checkpoint).** Build the fix, trim the
   archive, add descriptors with alt 0 and alt 1, add GTB, add the UMP stream
   replies, and add the `c_midi` neutralisation. Install a minimal test app:
   pads send MIDI 2.0 notes with 16-bit Zero-latency velocity. The player
   flashes it and reports:
   - whether Windows MIDI Services lists a MIDI 2.0 endpoint with the right
     name and two function blocks;
   - whether notes arrive cleanly;
   - whether MIDI 1.0 fallback still works.
   If something fails, fix it here, or replace `c_midi.o`, before going on.
2. **Instrument.** Scale engine, pad engine (both velocity modes, curves,
   pressure), menu, persistence, all host-tested.
3. **Final hardware pass.** The full checklist in section 7.

## Left to the builder

- Exact peak-window length and baseline/normalisation, after measuring how
  often `adc_buffer` updates (if it updates faster than 1 kHz, sample faster).
- Exact colours, the pressure hysteresis and the rate cap.
- File layout inside `src/` and the TX ring size.
- Product-string wording within the 2.0 naming above.
- Whether TX flushing also needs to happen in the EP1 IN interrupt for
  throughput.
