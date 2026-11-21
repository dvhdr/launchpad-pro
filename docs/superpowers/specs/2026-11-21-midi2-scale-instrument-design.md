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

- The STM32F103RB has 20 KiB RAM and 128 KiB of physical flash. The app
  starts at `0x08006400` and the archive's user flash HAL uses a 1 KiB page
  at `0x0801F800`. Fix the 128 KiB length in `stm32_flash.ld` by capping
  the app linker region at 101 KiB
  (`0x08006400`–`0x0801F7FF`), including the flash load image for `.data`.
  USB is full-speed with 64-byte bulk packets on EP1 OUT/IN.
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
- A clean firmware build requires the pinned `tools/libintelhex` submodule
  and the ARM toolchain documented in the README.

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
- In both modes the menu curve is applied to the raw peak. MIDI 1.0 output
  uses the same shaped value, scaled as described in section 3. The Fixed
  setting sends full velocity.
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
  - **alt 1** — `bInterfaceProtocol = 0`, `bcdMSC = 0x0200`, and class-specific
    MS header `wTotalLength = 7`. Both 64-byte bulk endpoints (`0x01` and
    `0x81`) use 7-byte standard descriptors and `CS_ENDPOINT`
    `MS_GENERAL_2_0` descriptors listing both Group Terminal Block IDs.
- **Group Terminal Block descriptors** use the standard interface-recipient
  `GET_DESCRIPTOR` (`bmRequestType = 0x81`, `wValue = 0x2601`, `wIndex = 1`).
  Use the alternate setting in `wValue` even while alt 0 is active. Serve
  the 5-byte header or up to the full 31-byte descriptor set according to
  `wLength` (USB MIDI 2.0 §6). Both blocks are bidirectional. Block ID 1
  ("Standalone Port") covers Group 1 (`0x0`) with default protocol `0x11`
  (MIDI 2.0). Block ID 2 ("MIDI Port") covers Group 2 (`0x1`) with default
  protocol `0x03` (MIDI 1.0 in UMP, up to 128 bits) and the DIN bandwidth
  limit (`0x0001` in each direction).
- `usb_prop.c` reimplements the ST `Device_Property`, `Device_Table` and
  `User_Standard_Requests` tables (same symbol names as the removed object),
  preserving today's `Device_Reset` endpoint/PMA setup and the calls to
  `USBConfigured` / `USBUnconfigured`, and adding:
  - `Get_Interface_Setting` accepting alt 0 on interface 0 and alt 0 or 1 on
    interface 1, and `GET_INTERFACE` reporting the selected alt for the
    requested interface;
  - `SET_INTERFACE` handling that switches EP1 ownership. Quiesce EP1,
    discard old USB FIFO and parser state, reset endpoint data toggles and
    rearm the new path. USB reset and configuration changes restore alt 0
    and clear USB-side state. The DIN path keeps running across USB changes.
  - interface-recipient `GET_DESCRIPTOR` for the GTB descriptor.
- `usb_endp.c` reimplements `EP1_OUT_Callback`: in alt 0 it behaves exactly
  as the original (push bytes to the `MIDIToDev` fifo, NAK RX when the fifo
  is busy so `c_midi` re-validates it); in alt 1 it hands 32-bit UMP words to
  the UMP receive path.

### 2. UMP layer (`src/ump/`)

- **Receive:** read USB UMP words and frame each message by its defined word
  count before dispatch. Skip whole unsupported messages and keep any
  partial message bounded across reads. The USB interrupt accepts packets.
  App callbacks run from the polling path used by the existing MIDI parser.
  - Type `0xF` stream messages: honor Endpoint Discovery filter bits,
    including the Stream Configuration Notification bit. Report UMP v1.1,
    two active static Function Blocks and support for both MIDI protocols
    without JR timestamps. Send Device Identity (Novation ID `00 20 29`),
    Endpoint Name and a stable, per-device Product Instance ID when
    requested. Function Block Discovery handles individual block IDs `0`
    and `1` and the all-blocks request `0xFF`, according to the info/name
    filter. The DIN Function Block declares a 31.25 kbit/s MIDI 1.0
    connection. Start alt 1 in MIDI 2.0 for the Standalone block. Reply to
    Stream Configuration Requests with the actual protocol and JR settings.
    Release held USB notes in the old protocol before switching protocols.
  - Type `0x2` (MIDI 1.0 channel voice) and type `0x4` (MIDI 2.0 channel
    voice): translate supported messages per UMP v1.1.2 Appendix D and call
    `app_midi_event` with `USBSTANDALONE` for wire group `0x0` or `USBMIDI`
    for `0x1`. Other groups are outside this topology. Preserve status-byte
    semantics where translation requires multiple MIDI 1.0 messages.
  - Type `0x3` (SysEx7): reassemble separately for each exposed group, with
    a 320-byte limit including `F0`/`F7`. Restore those framing bytes for
    `app_sysex_event` and discard malformed or oversized sequences.
  - Type `0x1` system: pass through supported MIDI 1.0 system common and
    realtime messages, including the clock sent by the example app.
- **Transmit:** queue complete UMP messages and fill EP1 IN up to 64 bytes
  per transfer while configured in alt 1. Keep the FIFO order for each
  note and reserve room for its release before accepting an attack. Give
  discovery and note traffic capacity ahead of pressure. Coalesce unsent
  pressure updates and discard stale updates when the note ends. Keep
  each SysEx sequence intact when admitting it for transmission. Pace DIN
  output to its wire rate. At 200 Hz per
  pad, a 64-pad MIDI 2.0 pressure burst alone would require 102.4 kB/s,
  above the 64 kB/s available with one 64-byte submission per 1 ms tick.
  Start with a single polling owner for EP1 IN. If measurements call for
  completion-driven refill, install an EP1 IN handler (`usb_istr.o`
  currently uses `NOP_Process`) and serialize access to the queue and EP1.
- **Existing-app compatibility:** the linker's `--wrap` option redirects app
  calls to `hal_send_midi` / `hal_send_sysex`. Wrapped versions: DIN or alt 0
  → call the real library function. In alt 1, encode system messages as
  type `0x1` and SysEx7 as type `0x3`. Use type `0x4` for channel voice on
  the Standalone group with MIDI 2.0 selected, and type `0x2` for MIDI 1.0
  in UMP, including the DIN bridge. Apply Appendix D when upscaling,
  including translating legacy zero-velocity Note On to MIDI 2.0 Note Off.
- **New API** in `include/midi2.h` (new file; `app.h` untouched):
  - `u8 midi2_active(void)` — 1 when the host selected alt 1 and the
    Standalone group is using MIDI 2.0 channel voice;
  - `void hal_send_note_on2(u8 port, u8 ch, u8 note, u16 velocity)`,
    `hal_send_note_off2(...)`, `hal_send_poly_pressure2(u8 port, u8 ch,
    u8 note, u32 value)`, `hal_send_channel_pressure2(...)`. Each falls back
    to the 7-bit MIDI 1.0 equivalent automatically when MIDI 2.0 isn't
    available on the requested port. In alt 1 with MIDI 1.0 selected, send
    the fallback as type `0x2` UMP.
- **Known conflict with `c_midi.o`:** `usbmidi_poll` → `usbmidi_tx` writes
  EP1 IN whenever its outgoing fifo (`MIDIToHst`) has data. `main.o` calls
  `midi_flush_windows_buffer` directly (not through `hal_send_midi`), and
  that puts a MIDI 1.0 "Windows flush" message into the fifo after
  configuration. `main.o` separately calls `midi_parsers_poll`, which
  consumes `MIDIToDev` and can rearm EP1 OUT with `SetEPRxValid` when it
  sees NAK. That same routine also parses DIN input. In alt 1, gate legacy
  IN and the Windows flush at their `main.o` call sites, and split or gate
  the USB half of `midi_parsers_poll` so DIN parsing continues. Wrap the
  whole parser only with an equivalent DIN-only path. Clearing USB FIFOs
  is part of the mode transition. Treat exclusive EP1 ownership as a
  transport milestone gate. If wrapper-based isolation cannot preserve DIN
  and legacy USB behavior, replace `c_midi.o` before the hardware checkpoint.

### 3. Pad engine (`src/pads/`)

- Keeps the `adc_buffer` pointer from `app_init`.
- Per-pad state: idle → capturing (peak-window) → held → released. A release
  inside the capture window finalizes the peak so a fast tap still produces
  an attack followed by a release. Latch the output destination, channel
  and pitch at attack so octave or root changes while held can release the
  correct note. Release active notes on entry to Setup and ignore pads held
  across the page change until they are released.
- **Velocity → 16 bits** (algorithms above). Curves: Soft / Linear / Hard,
  applied to the 16-bit value. Option "Fixed" sends a constant full velocity.
- **Pressure → 32 bits** from raw ADC: apply the Low/Med/High threshold and
  normalise to a 12-bit value `p` before bit replication to 32 bits
  (`(p << 20) | (p << 8) | (p >> 4)` with `uint32_t` operands).
  This preserves the sensor steps and reaches both endpoints without
  requiring a 64-bit division helper in the firmware link. Send only on
  change beyond a small hysteresis, with a per-pad cap of about 200 Hz and
  the transport-wide pacing above. Modes are Poly, Channel (maximum of
  held pads), and Off.
- MIDI 1.0 output (alt 0, alt 1 using MIDI 1.0, or DIN mirror) uses the top
  seven bits of the shaped velocity. Clamp Note On to at least 1 so soft
  strikes still sound. Use the selected pressure mode and threshold on all
  outputs, scaling the pressure value to seven bits for MIDI 1.0.
- Key sounding notes by destination, channel and pitch. Multiple held pads
  mapped to one pitch share one Note On and one final Note Off. Use the
  maximum pressure among pads sharing a pitch for poly pressure, and send
  channel pressure back to zero after the last pad is released in Channel mode.
- Send instrument notes to the Standalone USB port. Keep the example app's
  bidirectional channel-voice and system-message bridge between the USB
  MIDI Port and DIN in both USB modes. DIN mirror adds instrument output
  without changing bridge routing.

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
  `12 × (stored + 1) + root` (stored 4 → C3 = 60, the default). In
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
- Row 4: eight octave positions from C−1 to C6.
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
- Status (read-only): Setup LED is green when the Standalone port is using
  MIDI 2.0. It is amber for MIDI 1.0, including MIDI 1.0 protocol in alt 1.

### 6. Persistence

- Settings (scale, root, octave, layout, absolute root, all options) use a
  fixed-width record in the HAL's 1 KiB user flash page, with a magic value,
  version and checksum. Validate field ranges before use. Blank flash,
  unsupported versions and incomplete or invalid records load defaults.
- Close the menu immediately and defer the flash write until a short quiet
  period with no active notes. Compare settings with the last saved record
  so reopening the menu or undoing a change avoids an erase. A power loss
  during the single-page erase/write may reset the settings to defaults.

### 7. Build and tests

- Build setup: initialize the pinned libintelhex submodule before invoking
  Make. Makefile: create target directories for standalone builds, produce
  `build/launchpad_pro_trimmed.a` with `arm-none-eabi-ar d` of the three
  USB objects, compile the platform and app sources, and add the wrappers
  described in section 2. Keep STM32-only USB code out of the host simulator
  build and provide host implementations for the app's platform API.
- `make test`: host-side (gcc) unit tests with no hardware:
  - scale/layout mapping (every layout × a sample of scales and roots;
    in-key layouts contain only scale notes, and all eight octave positions
    match the arrow controls, including default C3 = MIDI 60).
  - UMP encoding/decoding (byte and word order, unsupported message framing,
    group routing, interleaved SysEx7 and the 320-byte boundary). Exercise both
    Note On velocity-zero translation rules, system common and realtime.
  - Stream discovery replies against UMP 1.1, including discovery filters,
    an individual Function Block versus `0xFF`, and MIDI 1.0 protocol in
    alt 1.
  - Descriptor and control-path tests: verify configuration and class
    descriptor lengths, byte-exact alt 0, GTB header/full requests before
    and after `SET_INTERFACE`, per-interface `GET_INTERFACE`, and reset to
    alt 0 with each direction of EP1 owned by the right transport.
  - Velocity and pressure math (monotonic, full range, curve endpoints),
    plus fast taps, remapping while held, duplicate pitches, Setup entry
    with held pads, channel pressure returning to zero, and note releases
    surviving a saturated pressure queue.
  - Settings round trip, blank and corrupt flash, and coalesced/unchanged
    writes. Use fixed-width types for UMP words and pressure calculations
    in host tests, since `u32` in `app.h` maps to `unsigned long`.
- The existing simulator builds and runs.
- The ARM firmware links and fits the app flash region and RAM. Check the
  ELF load addresses and packaged image against the flash limit, including
  the converter's final block padding, and report the memory sizes.
- **Hardware checklist** for the player (we have no device here): flash
  via the bootloader; on Windows with MIDI Services, check that the endpoint
  appears as MIDI 2.0 with two function blocks and the right name (e.g.
  `midi endpoint list`); monitor UMP and see 16-bit velocities and 32-bit
  pressure; check that MIDI 1.0 apps still see the ports; exercise DIN
  routing in both directions on both USB alternate settings; check MIDI 1.0
  fallback on a machine without MIDI Services; reconnect and reset during
  traffic; compare the feel and measured note-on latency of both velocity
  modes against the stock device.

## Delivery stages

The player has the hardware and will test on Windows at the checkpoint.

1. **Transport milestone (hardware checkpoint).** Build the fix, trim the
   archive, add descriptors with alt 0 and alt 1, add GTB, add the UMP stream
   replies, and add the `c_midi` neutralisation. Install a minimal test app:
   pads send MIDI 2.0 notes with 16-bit Zero-latency velocity. The player
   flashes it and reports:
   - whether Windows MIDI Services lists a MIDI 2.0 endpoint with the right
     name and two function blocks;
   - whether notes arrive cleanly through reconnects and mode changes;
   - whether MIDI 1.0 fallback and both directions of DIN routing still work.
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

## References

- [USB-IF MIDI Devices 2.0 class specification, §§5–6 and Appendix B](https://www.usb.org/sites/default/files/USB%20MIDI%20v2_0.pdf)
- [UMP and MIDI 2.0 Protocol v1.1.2, §7 and Appendix D](https://amei-music.github.io/midi2.0-docs/amei-pdf/M2-104-UM_v1-1-2_UMP_and_MIDI_2-0_Protocol_Specification.pdf)
- [STM32F103RB memory and device specifications](https://www.st.com/en/microcontrollers-microprocessors/stm32f103rb.html)
