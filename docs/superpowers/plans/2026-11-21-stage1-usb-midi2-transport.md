# Stage 1: USB MIDI 2.0 Transport Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship the stage 1 "transport milestone" firmware. The Launchpad Pro
enumerates with a MIDI 1.0 alt 0 and a USB MIDI 2.0 alt 1, answers UMP stream
discovery, sends 16-bit-velocity MIDI 2.0 notes from the pads, keeps the
USB ↔ DIN bridge working, and comes with a hardware checkpoint document the
player runs on Windows.

**Architecture:** The closed `lib/launchpad_pro.a` is linked as a trimmed
copy: `usb_desc.o`, `usb_prop.o` and `usb_endp.o` are removed and replaced by
our sources, and `c_midi.o` gets two local symbols made global with
`objcopy`. GNU ld `--wrap` sends `main.o`'s and the app's calls into
`c_midi.o` through a mode switch, so in alt 1 EP1 belongs only to the new UMP
layer, while DIN parsing keeps running. The UMP layer (codec, stream
responder, TX queue, RX framing) is plain C99 and is tested on the host.

**Tech Stack:** C99, arm-none-eabi-gcc (Cortex-M3, STM32F103RB), GNU ld
`--wrap`, host gcc for tests and the simulator, ST USB-FS device library
(closed, v3.x layout), USB MIDI 2.0 class spec, UMP v1.1.2.

**Spec:** `docs/superpowers/specs/2026-11-21-midi2-scale-instrument-design.md`.
This plan covers spec §1, §2 and the transport parts of §7, plus delivery
stage 1. Stages 2 (instrument), 3 (MIDI-CI) and 4 (final hardware pass) get
their own plans after the checkpoint results are in.

## Global Constraints

- `include/app.h` and `include/app_defs.h` are not modified.
- The app flash region is `0x08006400`–`0x0801F7FF` (101 KiB). The user
  flash page `0x0801F800` must never be overwritten by the image.
- RAM is 20 KiB total; baseline `.bss` is 6360 bytes. New static buffers
  for stage 1 must stay ≤ 4 KiB in total.
- VID `0x1235`, PID `0x4051`, bcdDevice `0x0300`. Product string
  "Launchpad Pro MIDI 2.0".
- Alt 0 class-specific descriptors are byte-identical to today's.
- UMP words are `uint32_t` (never `u32`, which is `unsigned long` and 64-bit
  on the host). On USB they are little-endian.
- Port → group: `USBSTANDALONE` (0) → group `0x0`; `USBMIDI` (1) → group
  `0x1`. Any other group is ignored.
- Stage 1 Function Block 0 reports MIDI-CI version `0x00` (MIDI-CI arrives
  in stage 3).
- No heap, no 64-bit division in firmware code.
- No GPL code. ST library behaviour is reimplemented from its documented
  v3.x layout and the archive disassembly, not copied from ST sources.
- Every commit ends with the session's attribution lines.

## Review Focus

1. **USB reset or replug in the middle of traffic.** A reset during a
   half-received SysEx7 or with queued TX must leave alt 0, empty queues, no
   reassembly in progress, and DIN still parsing. Test:
   `test_reset_mid_traffic` (Task 7).
2. **Unsupported UMP mixed in with notes.** A 128-bit message (MT `0x5`
   SysEx8 or `0xD` Flex Data), or a message on group `0x5`, between two
   Note Ons must be skipped whole: both notes reach the app and nothing
   desyncs. Test: `test_rx_skips_unsupported_between_notes` (Task 5).
3. **A USB packet whose length isn't a multiple of 4.** The trailing 1–3
   bytes are dropped and the next packet still frames correctly. Test:
   `test_ep1_out_odd_length` (Task 8).
4. **TX pushes from the 1 kHz timer interrupt while the main loop flushes.**
   `app_timer_event` runs in interrupt context (it's called from `c_init.o`).
   Every TX queue push and pop runs inside `irq_save`/`irq_restore`. Test:
   `test_tx_push_inside_fill` (Task 4) calls push re-entrantly from the
   stub's critical-section hook and checks the queue stays consistent.
5. **The host stops reading EP1 IN** (app closed) while pads are played.
   Note Ons are refused once only release reserve remains, Note Offs are
   always accepted, and the main loop never blocks. Test:
   `test_tx_saturated_release_always_fits` (Task 4).

---

## File Structure

| File | Responsibility |
|---|---|
| `Makefile` | trimmed archive, wraps, `test` and simulator targets, image check |
| `stm32_flash.ld` | FLASH length capped to 101K |
| `tools/check_image.sh` | ELF and `.syx` packaging end-address check |
| `include/midi2.h` | public MIDI 2.0 app API (new) |
| `src/platform/irq.h` | `irq_save` / `irq_restore` (PRIMASK on ARM, no-op on host) |
| `src/platform/platform.c` | product instance ID from the STM32 UID (ARM only) |
| `src/platform/platform.h` | `platform_product_instance_id` |
| `src/platform/lib_private.h` | declarations of closed-library internals we touch |
| `src/ump/ump.h` | all UMP-layer types and prototypes |
| `src/ump/ump_codec.c` | scaling, encoders, MIDI 1.0 ↔ UMP translation, SysEx7 pack/reassemble |
| `src/ump/ump_stream.c` | stream message responder (discovery, FB, stream config) |
| `src/ump/ump_tx.c` | prioritised TX queue with release reserve and pressure coalescing |
| `src/ump/ump_rx.c` | RX word ring, framing, dispatch to app callbacks |
| `src/usb/usb_compat.h` | ST USB-FS v3.x struct layouts and externs |
| `src/usb/usb_desc.c`, `usb_desc.h` | device, config (both power variants), GTB, string data |
| `src/usb/usb_prop.c` | `Device_Property`, `Device_Table`, `User_Standard_Requests` |
| `src/usb/usb_endp.c` | `EP1_OUT_Callback` |
| `src/usb/usb_midi2.c`, `usb_midi2.h` | alt state, EP1 ownership, wraps, TX flush, `midi2.h` implementation, sounding-note tracking |
| `src/app.c` | checkpoint test app (replaced again in stage 2) |
| `tools/sim_midi2.c` | host versions of the `midi2.h` API and `ADC_Max` for the simulator |
| `tests/test.h`, `tests/run_tests.c` | minimal assert framework and runner |
| `tests/stubs/stubs.h`, `tests/stubs/stubs.c` | recording stubs for ST library, fifos, c_midi, app callbacks |
| `tests/test_*.c` | one file per module |
| `docs/notes/usb_prop_original.md` | annotated behaviour of the removed `usb_prop.o` |
| `docs/checkpoint-1-transport.md` | the player's hardware test procedure and results form |

---

### Task 1: Build foundation and host test harness

**Files:**
- Modify: `Makefile`, `stm32_flash.ld:15`, `README.md` (build section)
- Create: `tools/check_image.sh`, `tests/test.h`, `tests/run_tests.c`,
  `tests/test_harness.c`

**Interfaces:**
- Produces: the `make test` target, which compiles `$(TEST_SOURCES)` with
  host gcc, `-std=c99 -Wall -Werror -DHOST_TEST -Iinclude -Isrc -Itests`, into
  `build/tests/run_tests` and runs it. Later tasks append their files to
  `TEST_SOURCES`.
- Produces in `tests/test.h`: `CHECK(cond)`, `CHECK_EQ(a, b)` (compares as
  `unsigned long long`, prints both in hex), `CHECK_MEM(a, b, n)`, and
  `RUN(test_fn)`. `int test_failures(void)`.
- Produces in `tests/run_tests.c`: `main` calls each module's
  `void test_<module>_all(void)` in order. Exit code 1 if any check failed.
- Produces the Makefile variable `HOST_GPP = g++ -include cstdint`, which
  fixes the pinned libintelhex header under modern g++.

- [ ] **Step 1: Write the harness self-test**

`tests/test_harness.c`: `test_harness_all()` runs
`CHECK_EQ(1 + 1, 2)` and `CHECK_MEM("ab", "ab", 2)`.

- [ ] **Step 2: Run it and see it fail**

Run: `make test`
Expected: FAIL with `No rule to make target 'test'`.

- [ ] **Step 3: Add the targets and fixes**

- `Makefile`:
  - `HOST_GPP` as above;
  - `test:` target;
  - `$(SIMULATOR)` gains `mkdir -p $(BUILDDIR)`;
  - `$(SYX)` runs `tools/check_image.sh $(ELF)` after `hextosyx`;
  - a `submodules` hint: if `tools/libintelhex/include/intelhex.h` is
    missing, fail with `run: git submodule update --init`.
- `stm32_flash.ld:15`: `LENGTH = 101K`.
- `tools/check_image.sh ELF`:
  - compute `end` = max over ALLOC sections of (LMA + size), from
    `arm-none-eabi-objdump -h`;
  - fail if `end > 0x0801F800`;
  - mirror `hextosyx`: blocks start at `base + 32·k` while `< end`, and each
    block reads 35 bytes (five 7-byte groups). Fail if
    `last_block_start + 35 > 0x0801F800`;
  - print `end`, the last block and the remaining bytes.
- `README.md`: build steps (`git submodule update --init`, toolchain
  packages `gcc-arm-none-eabi binutils-arm-none-eabi
  libnewlib-arm-none-eabi`, then `make`, `make test`).

- [ ] **Step 4: Run and see it pass**

Run: `make clean && make && make test`
Expected: `build/launchpad_pro.syx` is produced, `check_image.sh` prints an
end address below `0x0801F800`, and `make test` prints `0 failures`, exit 0.

- [ ] **Step 5: Commit**

```bash
git add Makefile stm32_flash.ld README.md tools/check_image.sh tests/
git commit -m "build: fix host converter build, cap app flash at 101K, add host test harness"
```

---

### Task 2: UMP codec

**Files:**
- Create: `src/ump/ump.h` (codec section), `src/ump/ump_codec.c`,
  `tests/test_ump_codec.c`

**Interfaces:**
- Produces (all in `ump.h`, all `uint*_t`):
  - `uint8_t ump_word_count(uint32_t word0)`: by MT:
    0,1,2,6,7 → 1; 3,4,8,9,A → 2; B,C → 3; 5,D,E,F → 4.
  - `uint32_t ump_scale_up(uint32_t v, uint8_t src_bits, uint8_t dst_bits)`:
    the UMP spec's Min-Center-Max upscaling (code below).
  - `uint32_t ump_scale_down(uint32_t v, uint8_t src_bits, uint8_t dst_bits)`:
    `v >> (src_bits - dst_bits)`.
  - Encoders return the word count written to `out`:
    - `uint8_t ump_m2_note_on(uint32_t out[2], uint8_t group, uint8_t ch, uint8_t note, uint16_t vel)`
    - `uint8_t ump_m2_note_off(uint32_t out[2], uint8_t group, uint8_t ch, uint8_t note, uint16_t vel)`
    - `uint8_t ump_m2_poly_pressure(uint32_t out[2], uint8_t group, uint8_t ch, uint8_t note, uint32_t value)`
    - `uint8_t ump_m2_channel_pressure(uint32_t out[2], uint8_t group, uint8_t ch, uint32_t value)`
  - `uint8_t ump_from_midi1(uint32_t out[2], uint8_t group, uint8_t status, uint8_t d1, uint8_t d2, uint8_t as_midi2)`:
    - system common/realtime (`0xF1`–`0xFF`, not `0xF0`/`0xF7`) → MT 1;
    - channel voice → MT 4 when `as_midi2`, otherwise MT 2;
    - returns 0 for SysEx bytes or invalid status.
    - MT 4 follows UMP v1.1.2 Appendix D:
      - Note On velocity 0 → MIDI 2.0 Note Off with the Appendix D velocity;
      - 7-bit values → `ump_scale_up(…, 7, 16 or 32)`;
      - pitch bend: 14-bit → 32-bit;
      - program change: bank-valid flag clear.
  - `uint8_t ump_to_midi1(const uint32_t *msg, uint8_t out[][3], uint8_t max_out)`:
    - MT 1, 2 and 4 → 3-byte MIDI 1.0 messages, returning how many;
    - MT 4 velocity `vel >> 9`, and a Note On result of 0 becomes 1;
    - 32-bit values `>> 25` (pitch bend `>> 18`, split into LSB/MSB);
    - Program Change with bank valid → CC 0, CC 32, program (3 messages);
    - RPN/NRPN (MT 4 status `0x2`/`0x3`) → CC 101/100 or 99/98, then
      CC 6/38 (4 messages);
    - per-note controllers and per-note management → 0 messages.
  - `uint16_t ump_sysex7_pack(uint32_t *out, uint16_t max_words, uint8_t group, const uint8_t *data, uint16_t len)`:
    - `data` includes `F0`…`F7`, which are stripped;
    - 6 payload bytes per 2-word packet, status complete/start/continue/end;
    - returns words written, or 0 if it won't fit (nothing partial).
  - Reassembly:
    `struct ump_sysex7_rx { uint8_t buf[320]; uint16_t len; uint8_t active; };`,
    `void ump_sysex7_rx_reset(struct ump_sysex7_rx *rx)`, and
    `int ump_sysex7_rx_feed(struct ump_sysex7_rx *rx, const uint32_t msg[2])`.
    Feed returns 1 when complete (`buf` = `F0`…`F7`, `len` includes both),
    0 while in progress, and -1 when it discards: oversize > 320, continue
    without start, a start while active (discard old, begin new), or
    byte count > 6.

`ump_scale_up` must follow the spec's algorithm exactly:

```c
uint32_t ump_scale_up(uint32_t v, uint8_t src_bits, uint8_t dst_bits) {
    uint8_t scale_bits = dst_bits - src_bits;
    uint32_t shifted = v << scale_bits;
    uint32_t center = 1u << (src_bits - 1);
    if (v <= center) return shifted;
    uint8_t repeat_bits = src_bits - 1;
    uint32_t repeat = v & ((1u << repeat_bits) - 1);
    repeat = scale_bits > repeat_bits ? repeat << (scale_bits - repeat_bits)
                                      : repeat >> (repeat_bits - scale_bits);
    while (repeat) { shifted |= repeat; repeat >>= repeat_bits; }
    return shifted;
}
```

- [ ] **Step 1: Write failing tests in `tests/test_ump_codec.c`**

```c
static void test_scale_up(void) {
    CHECK_EQ(ump_scale_up(0, 7, 16), 0x0000);
    CHECK_EQ(ump_scale_up(64, 7, 16), 0x8000);
    CHECK_EQ(ump_scale_up(127, 7, 16), 0xFFFF);
    CHECK_EQ(ump_scale_up(127, 7, 32), 0xFFFFFFFFu);
    CHECK_EQ(ump_scale_up(4095, 12, 16), 0xFFFF);
    CHECK_EQ(ump_scale_up(0x2000, 14, 32), 0x80000000u);
}
static void test_encoders(void) {
    uint32_t w[2];
    CHECK_EQ(ump_m2_note_on(w, 0, 0, 60, 0xFFFF), 2);
    CHECK_EQ(w[0], 0x40903C00u); CHECK_EQ(w[1], 0xFFFF0000u);
    ump_m2_note_off(w, 1, 2, 60, 0);  CHECK_EQ(w[0], 0x41823C00u);
    ump_m2_poly_pressure(w, 0, 0, 60, 0x12345678u);
    CHECK_EQ(w[0], 0x40A03C00u); CHECK_EQ(w[1], 0x12345678u);
    ump_m2_channel_pressure(w, 0, 3, 7); CHECK_EQ(w[0], 0x40D30000u); CHECK_EQ(w[1], 7);
}
static void test_from_midi1(void) {
    uint32_t w[2];
    CHECK_EQ(ump_from_midi1(w, 0, 0x90, 60, 100, 0), 1); CHECK_EQ(w[0], 0x20903C64u);
    CHECK_EQ(ump_from_midi1(w, 0, 0xF8, 0, 0, 1), 1);    CHECK_EQ(w[0], 0x10F80000u);
    CHECK_EQ(ump_from_midi1(w, 0, 0x90, 60, 127, 1), 2); CHECK_EQ(w[1] >> 16, 0xFFFF);
    CHECK_EQ(ump_from_midi1(w, 0, 0x90, 60, 0, 1), 2);   /* Note On vel 0 → Note Off */
    CHECK_EQ((w[0] >> 20) & 0xF, 0x8);                   /* velocity per Appendix D */
    CHECK_EQ(ump_from_midi1(w, 0, 0xF0, 0, 0, 1), 0);
}
static void test_to_midi1(void) {
    uint8_t m[4][3]; uint32_t on[2] = {0x40903C00u, 0x00010000u}; /* tiny velocity */
    CHECK_EQ(ump_to_midi1(on, m, 4), 1);
    CHECK_EQ(m[0][0], 0x90); CHECK_EQ(m[0][1], 60); CHECK_EQ(m[0][2], 1);
    uint32_t pc[2] = {0x40C00001u, 0x05000203u};          /* program 5, bank 2/3, valid */
    CHECK_EQ(ump_to_midi1(pc, m, 4), 3);
    CHECK_EQ(m[0][1], 0); CHECK_EQ(m[0][2], 2); CHECK_EQ(m[1][1], 32); CHECK_EQ(m[1][2], 3);
    CHECK_EQ(m[2][0], 0xC0); CHECK_EQ(m[2][1], 5);
    uint32_t pn[2] = {0x40F03C00u, 0};                    /* per-note management */
    CHECK_EQ(ump_to_midi1(pn, m, 4), 0);
}
static void test_sysex7(void) {
    uint8_t sx[15] = {0xF0, 1,2,3,4,5,6,7,8,9,10,11,12,13, 0xF7};
    uint32_t w[8]; struct ump_sysex7_rx rx; ump_sysex7_rx_reset(&rx);
    CHECK_EQ(ump_sysex7_pack(w, 8, 1, sx, 15), 6);        /* start 6, continue 6, end 1 */
    CHECK_EQ(w[0] >> 16, 0x3116); CHECK_EQ(w[2] >> 16, 0x3126); CHECK_EQ(w[4] >> 16, 0x3131);
    CHECK_EQ(ump_sysex7_rx_feed(&rx, &w[0]), 0);
    CHECK_EQ(ump_sysex7_rx_feed(&rx, &w[2]), 0);
    CHECK_EQ(ump_sysex7_rx_feed(&rx, &w[4]), 1);
    CHECK_EQ(rx.len, 15); CHECK_MEM(rx.buf, sx, 15);
    CHECK_EQ(ump_sysex7_pack(w, 4, 1, sx, 15), 0);        /* doesn't fit → nothing */
}
static void test_sysex7_limit(void) {
    /* 318 payload bytes + F0/F7 = 320 → accepted; 319 payload → discarded (-1) */
}
```

Fill in `test_sysex7_limit` by packing 318- and 319-byte payloads and
feeding every packet. `test_ump_codec_all` runs all of them.

- [ ] **Step 2: Run and see them fail**

Run: `make test`
Expected: compile failure, `ump.h` not found.

- [ ] **Step 3: Implement `src/ump/ump_codec.c`**

Follow the signatures above. For each Appendix D rule, take the exact
values from UMP v1.1.2 Appendix D: Note On velocity 0 → Note Off, and the
bank-valid and attribute fields.

- [ ] **Step 4: Run and see them pass**

Run: `make test`
Expected: `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add src/ump/ump.h src/ump/ump_codec.c tests/test_ump_codec.c tests/run_tests.c Makefile
git commit -m "ump: add codec (scaling, encoders, MIDI 1.0 translation, SysEx7)"
```

---

### Task 3: UMP stream responder

**Files:**
- Create: `src/ump/ump_stream.c`, `src/platform/platform.h`,
  `tests/test_ump_stream.c`
- Modify: `src/ump/ump.h` (stream section)

**Interfaces:**
- Consumes: `ump_tx_push(UMP_TX_CONTROL, …)` from Task 4. In this task's
  tests, link a fake `ump_tx_push` that records the words.
  `void platform_product_instance_id(char out[25])` (a NUL-terminated hex
  string; the host stub returns `"TEST0001"`).
- Produces:
  - `#define UMP_PROTO_MIDI1 0x01`, `#define UMP_PROTO_MIDI2 0x02`
  - `void ump_stream_reset(void)`: protocol = `UMP_PROTO_MIDI2`.
  - `uint8_t ump_stream_protocol(void)`
  - `int ump_stream_handle(const uint32_t msg[4])`: handles one MT `0xF`
    message, queuing its replies. Returns 1 if the protocol changed. Before
    changing, it calls the hook `void midi2_protocol_changing(uint8_t old_proto)`
    (implemented in Task 8; the test fake records it).
- Fixed identity values:
  - Device Identity: manufacturer `00 20 29`, family `0x0051`, model
    `0x0000`, version bytes `00 01 00 00`. *(Family, model and version are
    the builder's calls, recorded here.)*
  - Endpoint name: `"Launchpad Pro MIDI 2.0"`.
  - Function Block names: FB 0 `"Standalone Port"`, FB 1 `"MIDI Port"`.

- [ ] **Step 1: Write failing tests with byte-exact vectors**

```c
static void test_endpoint_discovery_all(void) {
    uint32_t req[4] = {0xF0000101u, 0x0000001Fu, 0, 0};
    fake_tx_clear(); ump_stream_reset(); ump_stream_handle(req);
    /* replies in order: info, identity, name(2), product id, stream config */
    CHECK_EQ(fake_tx_msg_count(), 6);
    CHECK_EQ(fake_tx_word(0,0), 0xF0010101u); CHECK_EQ(fake_tx_word(0,1), 0x82000300u);
    CHECK_EQ(fake_tx_word(1,0), 0xF0020000u); CHECK_EQ(fake_tx_word(1,1), 0x00002029u);
    CHECK_EQ(fake_tx_word(1,2), 0x51000000u); CHECK_EQ(fake_tx_word(1,3), 0x00010000u);
    CHECK_EQ(fake_tx_word(2,0), 0xF4034C61u); CHECK_EQ(fake_tx_word(2,1), 0x756E6368u);
    CHECK_EQ(fake_tx_word(2,2), 0x70616420u); CHECK_EQ(fake_tx_word(2,3), 0x50726F20u);
    CHECK_EQ(fake_tx_word(3,0), 0xFC034D49u); CHECK_EQ(fake_tx_word(3,1), 0x44492032u);
    CHECK_EQ(fake_tx_word(3,2), 0x2E300000u); CHECK_EQ(fake_tx_word(3,3), 0);
    CHECK_EQ(fake_tx_word(4,0), 0xF0045445u); CHECK_EQ(fake_tx_word(4,1), 0x53543030u);
    CHECK_EQ(fake_tx_word(4,2), 0x30310000u);
    CHECK_EQ(fake_tx_word(5,0), 0xF0060200u);
}
static void test_endpoint_discovery_filter(void) {
    uint32_t req[4] = {0xF0000101u, 0x00000001u, 0, 0};   /* info only */
    fake_tx_clear(); ump_stream_handle(req); CHECK_EQ(fake_tx_msg_count(), 1);
    uint32_t req2[4] = {0xF0000101u, 0x00000010u, 0, 0};  /* stream config only */
    fake_tx_clear(); ump_stream_handle(req2);
    CHECK_EQ(fake_tx_msg_count(), 1); CHECK_EQ(fake_tx_word(0,0), 0xF0060200u);
}
static void test_fb_discovery(void) {
    uint32_t all[4] = {0xF010FF03u, 0, 0, 0};             /* all blocks, info+name */
    fake_tx_clear(); ump_stream_handle(all);
    CHECK_EQ(fake_tx_word(0,0), 0xF0118033u); CHECK_EQ(fake_tx_word(0,1), 0x00010000u);
    /* FB0 name: 15 bytes → start + end packets; then FB1 info, FB1 name (complete) */
    CHECK_EQ(fake_tx_msg_count(), 5);
    CHECK_EQ(fake_tx_word(3,0), 0xF011813Bu); CHECK_EQ(fake_tx_word(3,1), 0x01010000u);
    uint32_t one[4] = {0xF0100101u, 0, 0, 0};             /* FB1, info only */
    fake_tx_clear(); ump_stream_handle(one);
    CHECK_EQ(fake_tx_msg_count(), 1); CHECK_EQ(fake_tx_word(0,0), 0xF011813Bu);
    uint32_t none[4] = {0xF0100203u, 0, 0, 0};            /* FB2 doesn't exist */
    fake_tx_clear(); ump_stream_handle(none); CHECK_EQ(fake_tx_msg_count(), 0);
}
static void test_stream_config_cycle(void) {
    uint32_t to1[4] = {0xF0050100u, 0, 0, 0}, to2jr[4] = {0xF0050203u, 0, 0, 0};
    ump_stream_reset(); fake_tx_clear();
    CHECK_EQ(ump_stream_handle(to1), 1);
    CHECK_EQ(fake_proto_hook_last(), UMP_PROTO_MIDI2);
    CHECK_EQ(ump_stream_protocol(), UMP_PROTO_MIDI1); CHECK_EQ(fake_tx_word(0,0), 0xF0060100u);
    fake_tx_clear(); CHECK_EQ(ump_stream_handle(to2jr), 1);   /* JR refused */
    CHECK_EQ(fake_tx_word(0,0), 0xF0060200u);
    fake_tx_clear(); CHECK_EQ(ump_stream_handle(to2jr), 0);   /* already MIDI 2 */
    CHECK_EQ(fake_tx_word(0,0), 0xF0060200u);
}
```

Also add `test_fb_name_round_trip`. It decodes the FB 0 name packets
(13 bytes per packet: one in word 0, twelve in words 1–3, form bits in
`word0[27:26]`) and checks they rebuild `"Standalone Port"`.

- [ ] **Step 2: Run and see them fail**

Run: `make test` → compile failure (`ump_stream_handle` undefined).

- [ ] **Step 3: Implement `src/ump/ump_stream.c`**

- Use the UMP v1.1.2 §7 bit layouts. Word 0 is
  `0xF << 28 | form << 26 | status << 16 | …`.
- Endpoint name and product ID carry 14 bytes per packet (2 in word 0).
  FB names carry 13 bytes (word 0 holds the FB number).
- Unknown stream statuses produce no reply.
- Stage 1 FB words: FB 0 is `0x8033` / `0x00010000` (bidirectional, UI
  hint both, not MIDI 1.0, MIDI-CI `0x00`). FB 1 is `0x813B` /
  `0x01010000` (MIDI 1.0 restricted to 31.25 kbit/s).

- [ ] **Step 4: Run and see them pass**

Run: `make test` → `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add src/ump/ump_stream.c src/ump/ump.h src/platform/platform.h tests/test_ump_stream.c tests/run_tests.c Makefile
git commit -m "ump: add stream responder (endpoint/FB discovery, stream configuration)"
```

---

### Task 4: UMP TX queue

**Files:**
- Create: `src/ump/ump_tx.c`, `src/platform/irq.h`, `tests/test_ump_tx.c`
- Modify: `src/ump/ump.h` (TX section)

**Interfaces:**
- Produces in `irq.h`: `static inline uint32_t irq_save(void)` and
  `static inline void irq_restore(uint32_t s)`.
  - ARM: `mrs %0, primask` then `cpsid i`; restore with `msr primask, %0`.
  - `HOST_TEST`: declare `extern void (*stub_irq_hook)(void);` (defined in
    `tests/stubs/stubs.c`). `irq_save` calls it if it's non-NULL, otherwise
    does nothing.
- Produces in `ump.h`:
  - `enum ump_tx_class { UMP_TX_CONTROL, UMP_TX_NOTE, UMP_TX_PRESSURE };`
  - Sizes:
    - `#define UMP_TX_CONTROL_WORDS 192` (a full 320-byte SysEx fits);
    - `#define UMP_TX_NOTE_WORDS 256`;
    - `#define UMP_TX_PRESSURE_SLOTS 64`.
  - `void ump_tx_reset(void)`
  - `int ump_tx_push(enum ump_tx_class cls, const uint32_t *msg, uint8_t words)`:
    - 0 on success, -1 if full;
    - pushes all-or-nothing, so a multi-message SysEx goes in one call;
    - `UMP_TX_PRESSURE` isn't allowed here (returns -1).
  - `int ump_tx_push_attack(const uint32_t *msg, uint8_t words)`:
    - accepts only if free note words ≥ `words` plus the current reserve
      plus 2;
    - on success, reserve += 2.
  - `int ump_tx_push_release(const uint32_t *msg, uint8_t words)`:
    - always succeeds while the reserve is ≥ `words`, then
      reserve −= `words`;
    - returns -1 only for a release with no matching reserve;
    - drops queued pressure for that group, channel and note before
      queuing.
  - `void ump_tx_push_pressure(uint8_t group, uint8_t ch, uint8_t note, const uint32_t msg[2])`:
    - `note = 0xFF` means channel pressure;
    - coalesces by replacing the pending slot with the same key;
    - silently drops the update if all slots are in use and none matches.
  - `uint8_t ump_tx_fill_packet(uint8_t out[64])`:
    - pops whole messages in priority order (all control, then notes in
      FIFO order, then pressure in slot order) until the next one doesn't
      fit in 64 bytes;
    - writes little-endian words and returns the byte count (a multiple
      of 4, or 0).
  - `uint16_t ump_tx_reserve_words(void)` (for tests).
- Every push, pop and reset runs between `irq_save` and `irq_restore`.

- [ ] **Step 1: Write failing tests**

```c
static void test_tx_priority_and_le(void) {
    uint32_t ctl[4] = {0xF0010101u, 0x82000300u, 0, 0}, on[2] = {0x40903C00u, 0xFFFF0000u};
    uint8_t pkt[64]; ump_tx_reset();
    ump_tx_push_attack(on, 2); ump_tx_push(UMP_TX_CONTROL, ctl, 4);
    CHECK_EQ(ump_tx_fill_packet(pkt), 24);
    CHECK_EQ(pkt[0], 0x01); CHECK_EQ(pkt[3], 0xF0);       /* control first, LE */
    CHECK_EQ(pkt[16], 0x00); CHECK_EQ(pkt[19], 0x40);
    CHECK_EQ(ump_tx_fill_packet(pkt), 0);
}
static void test_tx_pressure_coalesce_and_stale(void) {
    uint32_t p1[2] = {0x40A03C00u, 1}, p2[2] = {0x40A03C00u, 2}, off[2] = {0x40803C00u, 0};
    uint32_t on[2] = {0x40903C00u, 0xFFFF0000u}; uint8_t pkt[64]; ump_tx_reset();
    ump_tx_push_attack(on, 2);
    ump_tx_push_pressure(0, 0, 60, p1); ump_tx_push_pressure(0, 0, 60, p2);
    CHECK_EQ(ump_tx_fill_packet(pkt), 16);                /* note on + one pressure (=2) */
    CHECK_EQ(pkt[12], 2);
    ump_tx_push_pressure(0, 0, 60, p1); ump_tx_push_release(off, 2);
    CHECK_EQ(ump_tx_fill_packet(pkt), 8);                 /* stale pressure dropped */
}
static void test_tx_saturated_release_always_fits(void) {
    uint32_t on[2] = {0x40903C00u, 0x80000000u}, off[2] = {0x40803C00u, 0};
    int accepted = 0; ump_tx_reset();
    while (ump_tx_push_attack(on, 2) == 0) accepted++;    /* host not reading */
    CHECK(accepted > 0); CHECK(accepted <= UMP_TX_NOTE_WORDS / 4);
    for (int i = 0; i < accepted; i++) CHECK_EQ(ump_tx_push_release(off, 2), 0);
    CHECK_EQ(ump_tx_reserve_words(), 0);
}
static void test_tx_push_inside_fill(void) {
    /* stub_irq_hook pushes one control message the first time it runs;
       afterwards the queue must yield exactly the pushed messages, uncorrupted */
}
static void test_tx_sysex_all_or_nothing(void) { /* 200-word push → -1, queue unchanged */ }
```

Fill in the last two tests from their comments.

- [ ] **Step 2: Run and see them fail**

Run: `make test` → compile failure.

- [ ] **Step 3: Implement `src/ump/ump_tx.c` and `src/platform/irq.h`**

Control and note queues are word rings. Pressure is an array of
`{uint8_t used, group, ch, note; uint32_t w[2];}`.

- [ ] **Step 4: Run and see them pass**

Run: `make test` → `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add src/ump/ump_tx.c src/ump/ump.h src/platform/irq.h tests/test_ump_tx.c tests/run_tests.c Makefile
git commit -m "ump: add prioritised TX queue with release reserve and pressure coalescing"
```

---

### Task 5: UMP RX framing and dispatch

**Files:**
- Create: `src/ump/ump_rx.c`, `tests/stubs/stubs.h`, `tests/stubs/stubs.c`
  (app-callback part), `tests/test_ump_rx.c`
- Modify: `src/ump/ump.h` (RX section)

**Interfaces:**
- Consumes: `ump_word_count`, `ump_to_midi1`, `ump_sysex7_rx_feed`,
  `ump_stream_handle`.
  - `app_midi_event(u8 port, u8 status, u8 d1, u8 d2)` and
    `app_sysex_event(u8 port, u8 *data, u16 count)` from `app.h`.
  - `void midi2_ci_sysex_hook(uint8_t group, const uint8_t *data, uint16_t len)`
    is not added in stage 1. Leave a `/* stage 3: MIDI-CI intercept */`
    comment at the SysEx dispatch point.
- Produces:
  - `#define UMP_RX_WORDS 256`
  - `void ump_rx_reset(void)`: empties the ring, drops any partial message,
    and resets both SysEx reassemblers.
  - `uint16_t ump_rx_free_words(void)`
  - `void ump_rx_push_words(const uint32_t *w, uint16_t n)`: called from
    the USB interrupt. Words that don't fit are dropped and counted in
    `ump_rx_overflows()` (the caller prevents this; see Task 8).
  - `void ump_rx_poll(void)`: main-loop context. Pops words, and once a
    full message (by `ump_word_count`) is buffered, dispatches it:
    - MT `0xF` → `ump_stream_handle` (group-less);
    - MT 1, 2 or 4 on group 0 or 1 → `ump_to_midi1`, then one
      `app_midi_event` per result, with port = group;
    - MT 3 on group 0 or 1 → that group's reassembler, then
      `app_sysex_event` when complete;
    - anything else is skipped whole.
  - `uint32_t ump_rx_overflows(void)`
- Produces in `stubs.h`: `struct stub_midi_ev { uint8_t port, status, d1, d2; }`
  - `extern struct stub_midi_ev stub_midi_log[64]; extern int stub_midi_n;`
  - `extern uint8_t stub_sysex_buf[320]; extern uint16_t stub_sysex_len; extern uint8_t stub_sysex_port;`
  - `void stubs_reset(void)`

- [ ] **Step 1: Write failing tests**

```c
static void test_rx_skips_unsupported_between_notes(void) {
    uint32_t w[] = {0x40903C00u, 0xFFFF0000u,
                    0x50000000u, 1, 2, 3,                 /* SysEx8, 128-bit */
                    0x45903C00u, 0xFFFF0000u,             /* group 5: ignored */
                    0x41903E00u, 0x80000000u};
    stubs_reset(); ump_rx_reset(); ump_rx_push_words(w, 10); ump_rx_poll();
    CHECK_EQ(stub_midi_n, 2);
    CHECK_EQ(stub_midi_log[0].port, USBSTANDALONE); CHECK_EQ(stub_midi_log[0].d2, 127);
    CHECK_EQ(stub_midi_log[1].port, USBMIDI);       CHECK_EQ(stub_midi_log[1].d1, 62);
}
static void test_rx_partial_message_across_pushes(void) {
    uint32_t a = 0x40903C00u, b = 0xFFFF0000u;
    stubs_reset(); ump_rx_reset();
    ump_rx_push_words(&a, 1); ump_rx_poll(); CHECK_EQ(stub_midi_n, 0);
    ump_rx_push_words(&b, 1); ump_rx_poll(); CHECK_EQ(stub_midi_n, 1);
}
static void test_rx_interleaved_sysex_groups(void) {
    /* start on group 0, start on group 1, end on group 0, end on group 1 →
       two app_sysex_event calls with the right ports, F0/F7 restored */
}
static void test_rx_stream_routed(void) {
    /* Endpoint Discovery on the wire → ump_stream_handle sees it (count via fake) */
}
static void test_rx_system_clock(void) {
    uint32_t c = 0x10F80000u; stubs_reset(); ump_rx_reset();
    ump_rx_push_words(&c, 1); ump_rx_poll();
    CHECK_EQ(stub_midi_n, 1); CHECK_EQ(stub_midi_log[0].status, 0xF8);
}
```

Fill in the two comment-only tests.

- [ ] **Step 2: Run and see them fail**

Run: `make test` → compile failure.

- [ ] **Step 3: Implement `src/ump/ump_rx.c`**

The ring is written from the interrupt and read from the main loop. Guard
the index updates with `irq_save`/`irq_restore`. The partial-message buffer
is 4 words and belongs to `ump_rx_poll`.

- [ ] **Step 4: Run and see them pass**

Run: `make test` → `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add src/ump/ump_rx.c src/ump/ump.h tests/stubs tests/test_ump_rx.c tests/run_tests.c Makefile
git commit -m "ump: add RX framing and dispatch to app callbacks"
```

---

### Task 6: USB descriptors and Group Terminal Blocks

**Files:**
- Create: `src/usb/usb_desc.h`, `src/usb/usb_desc.c`, `tests/test_usb_desc.c`
- Modify: `Makefile` (golden extraction rule)

**Interfaces:**
- Produces in `usb_desc.h`:
  - `extern const uint8_t usb_device_desc[18];`
  - `extern const uint8_t usb_config_desc_bus[175];` (bMaxPower `0xFA`)
  - `extern const uint8_t usb_config_desc_psu[175];` (bMaxPower `0x01`)
  - `extern const uint8_t usb_gtb_desc[31];`
  - `#define USB_CONFIG_DESC_LEN 175`, `#define USB_ALT0_OFFSET` / `USB_ALT0_LEN`
    (the 133-byte original body within the new descriptor), and
    `#define USB_STR_PORT_STANDALONE 4`, `#define USB_STR_PORT_MIDI 5`.
- The golden data: `make test` runs
  `arm-none-eabi-objcopy -O binary --only-section=.rodata` on `usb_desc.o`
  extracted from `lib/launchpad_pro.a` into
  `build/tests/golden_usb_desc_rodata.bin` (266 bytes: `ConfigDescriptor_psu`
  at 0, `ConfigDescriptor` at `0x85`, each 133 bytes). Its path is passed as
  `-DGOLDEN_DESC=\"…\"`.

The descriptor layout is fixed by the spec:

- Device descriptor:
  `12 01 00 02 00 00 00 08 35 12 51 40 00 03 01 02 03 01`.
- Config descriptor = the original 133 bytes with `wTotalLength` changed to
  `0x00AF`, followed by this 42-byte alt 1 block:

```
09 04 01 01 02 01 03 00 00      interface 1, alt 1, 2 EPs, Audio/MIDIStreaming, protocol 0
07 24 01 00 02 07 00            MS header, bcdMSC 0x0200, wTotalLength 7
07 05 01 02 40 00 00            EP 0x01 bulk 64
06 25 02 02 01 02               MS_GENERAL_2_0, 2 GTBs: 1, 2
07 05 81 02 40 00 00            EP 0x81 bulk 64
06 25 02 02 01 02
```

- GTB descriptor set:

```
05 26 01 1F 00                                  header, wTotalLength 31
0D 26 02 01 00 00 01 04 11 00 00 00 00          block 1: bidir, group 0, 1 group, iBlock 4, MIDI 2.0, BW 0/0
0D 26 02 02 00 01 01 05 03 01 00 01 00          block 2: bidir, group 1, 1 group, iBlock 5, MIDI 1.0 ≤128-bit, BW 1/1
```

  Before freezing `iBlockItem` 4 and 5, confirm which string index holds
  "Standalone Port" and which holds "MIDI Port", using the
  `Device_GetStringDescriptor` disassembly from Task 7 Step 1. Swap them if
  needed. The jack `iJack` fields use 4 for cable 0 and 5 for cable 1.

- [ ] **Step 1: Write failing tests**

```c
static void test_alt0_byte_identical(void) {
    uint8_t g[266]; FILE *f = fopen(GOLDEN_DESC, "rb"); CHECK(f && fread(g, 1, 266, f) == 266);
    CHECK_MEM(usb_config_desc_psu + 4, g + 4, 129);       /* skip wTotalLength */
    CHECK_MEM(usb_config_desc_bus + 4, g + 0x85 + 4, 129);
    CHECK_EQ(usb_config_desc_psu[8], 0x01); CHECK_EQ(usb_config_desc_bus[8], 0xFA);
}
static void test_lengths_walk(void) {
    /* walk bLength from 0 → sum == 175 == wTotalLength; exactly 2 interface-1 alts;
       alt-1 MS header wTotalLength == 7 and both CS EPs list {1,2} */
}
static void test_gtb(void) {
    CHECK_EQ(usb_gtb_desc[3] | usb_gtb_desc[4] << 8, 31);
    CHECK_EQ(usb_gtb_desc[5 + 8], 0x11); CHECK_EQ(usb_gtb_desc[18 + 8], 0x03);
    CHECK_EQ(usb_gtb_desc[18 + 5], 0x01);                 /* block 2 → group index 1 */
}
static void test_device_desc(void) { CHECK_EQ(usb_device_desc[12], 0x00); CHECK_EQ(usb_device_desc[13], 0x03); }
```

Fill in `test_lengths_walk` from its comment.

- [ ] **Step 2: Run and see them fail**

Run: `make test` → compile failure.

- [ ] **Step 3: Implement `src/usb/usb_desc.c`**

Copy the alt 0 body bytes from `build/tests/golden_usb_desc_rodata.bin`
(`xxd -i` output pasted as arrays). Change only `wTotalLength`, then append
the alt 1 block.

- [ ] **Step 4: Run and see them pass**

Run: `make test` → `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add src/usb/usb_desc.* tests/test_usb_desc.c tests/run_tests.c Makefile
git commit -m "usb: add MIDI 2.0 descriptors (alt 1, GTB) keeping alt 0 byte-identical"
```

---

### Task 7: USB control path (`usb_prop.c`) and alternate-setting ownership

**Files:**
- Create: `docs/notes/usb_prop_original.md`, `src/usb/usb_compat.h`,
  `src/usb/usb_prop.c`, `src/usb/usb_midi2.h`, `src/usb/usb_midi2.c`
  (alt-state part), `src/platform/lib_private.h`, `tests/test_usb_prop.c`
- Modify: `tests/stubs/stubs.{h,c}` (ST library and fifo stubs), `Makefile`
  (trimmed archive and wraps)

**Interfaces:**
- Produces in `usb_compat.h`, matching ST USB-FS v3.x and verified against
  the disassembly:
  - `typedef enum { USB_SUCCESS = 0, USB_ERROR, USB_UNSUPPORT, USB_NOT_READY } RESULT;`
  - `DEVICE_INFO` is 28 bytes:
    - bytes 0–7: `USBbmRequestType`, `USBbRequest`, `USBwValues`,
      `USBwIndexs`, `USBwLengths`;
    - bytes 8–12: `ControlState`, `Current_Feature`,
      `Current_Configuration`, `Current_Interface`,
      `Current_AlternateSetting`;
    - `Ctrl_Info` at 16: `Usb_wLength`, `Usb_wOffset`, `PacketSize`,
      `CopyData`.
  - `DEVICE_PROP` holds 10 function pointers, then `RxEP_buffer`, then
    `MaxPacketSize` at offset 44 (48 bytes in all).
  - `USER_STANDARD_REQUESTS` holds 9 function pointers in ST order
    (GetConfiguration, SetConfiguration, GetInterface, SetInterface,
    GetStatus, ClearFeature, SetEndPointFeature, SetDeviceFeature,
    SetDeviceAddress).
  - `DEVICE { uint8_t Total_Endpoint, Total_Configuration; }`,
    `ONE_DESCRIPTOR { uint8_t *Descriptor; uint16_t Descriptor_Size; }`.
  - Plus externs for the ST functions used (`SetEPType`, `SetEPTxStatus`,
    `SetEPRxStatus`, `SetEPRxValid`, `SetEPTxAddr`, `SetEPRxAddr`,
    `SetEPTxCount`, `SetEPRxCount`, `ClearDTOG_RX`, `ClearDTOG_TX`,
    `GetEPTxStatus`, `GetEPRxStatus`, `USB_SIL_Read`, `USB_SIL_Write`,
    `Standard_GetDescriptorData`, `SetDeviceAddress`, `Clear_Status_Out`,
    `NOP_Process`) and `extern DEVICE_INFO *pInformation;`.
- Produces in `lib_private.h` (closed-library internals; the layouts come
  from the disassembly):
  - fifos: `typedef struct { uint8_t buf[256]; uint8_t head, tail; } cx_fifo;`
    (258 bytes);
  - `extern cx_fifo MIDIToDev, MIDIDinIn;`
  - `typedef struct cx_fifo1k cx_fifo1k;` plus `extern cx_fifo1k MIDIToHst;`
  - `void fifo_flush(cx_fifo *)`, `void fifo1k_flush(cx_fifo1k *)`,
    `uint8_t fifo_unempty(cx_fifo *)`, `void fifo_push1(cx_fifo *, uint8_t)`;
  - parser state: `extern uint8_t DinRx[24], USBParserToDev[24];`
    (made global by the build);
  - `void midi_parse_rx_byte_stream(cx_fifo *f, void *parser, uint8_t port);`
  - `extern uint16_t ADC_Max[64];`
  - the `__real_` prototypes for every wrapped function.
- Produces in `usb_midi2.h`:
  - `uint8_t usb_midi2_alt(void)` (0 or 1);
  - `void usb_midi2_set_alt(uint8_t alt)` (interrupt context): quiesce EP1;
    flush `MIDIToDev` and `MIDIToHst`; restore the `USBParserToDev`
    snapshot; call `ClearDTOG_RX(1)` / `ClearDTOG_TX(1)`; reset
    `ump_rx`, `ump_tx`, `ump_stream` and the sounding-note table; set the
    alt; then `SetEPRxValid(1)` with TX left NAK;
  - `void usb_midi2_usb_reset(void)` = `usb_midi2_set_alt(0)`, called from
    `Device_Reset` and `Device_SetConfiguration`;
  - `uint8_t usb_midi2_iface_alt(uint8_t iface)`.
- `Makefile`:
  - `LIB = $(BUILDDIR)/launchpad_pro_trimmed.a`, built by copying the
    original and running
    `arm-none-eabi-ar d $(LIB) usb_desc.o usb_prop.o usb_endp.o`;
  - extract `c_midi.o`, run
    `arm-none-eabi-objcopy --globalize-symbol=DinRx --globalize-symbol=USBParserToDev`
    on it, then `ar r` it back;
  - `LDFLAGS += -Wl,--wrap=hal_send_midi,--wrap=hal_send_sysex,--wrap=usbmidi_poll,--wrap=midi_flush_windows_buffer,--wrap=midi_parsers_poll,--wrap=midi_parsers_init`.
- `usb_prop.c` keeps every exported symbol of the removed object: all `T`
  and `D` symbols listed by `arm-none-eabi-nm` for `usb_prop.o` and
  `usb_desc.o`, including `Device_Property`, `Device_Table`,
  `User_Standard_Requests`, `USBDesc_StringSerial` and `MUTE_DATA` if they
  are referenced.

- [ ] **Step 1: Write `docs/notes/usb_prop_original.md`**

Disassemble every function in `usb_prop.o` and `usb_desc.o`
(`arm-none-eabi-objdump -d -r` on the extracted members). For each
function, record what it reads, writes and calls. Cover:
- the full `Device_Reset` (EP0/EP1 setup and how it picks the bus or PSU
  config);
- `Device_Init`;
- `Device_SetConfiguration` and its `USBConfigured`/`USBUnconfigured`
  calls;
- data and no-data setup;
- `Device_Get_Interface_Setting`;
- how the string descriptors are built from the ASCII buffers, including
  any device-ID suffix on the product string, and which index is
  "Standalone Port".

Also record from `usb_core.o`:
- `Standard_GetInterface` returns `&Current_AlternateSetting` after calling
  `User_GetInterface`;
- interface-recipient `GET_DESCRIPTOR` falls through to
  `Class_Data_Setup`.

Also record from `main.o` whether `midi_parsers_init` runs before
`app_init` and before USB is enabled.

Check: the document has a section per function, and the file is committed
with this task.

- [ ] **Step 2: Write failing tests (host, using recording stubs)**

```c
static void test_set_interface_switches_owner(void) {
    stubs_reset(); usb_midi2_usb_reset(); CHECK_EQ(usb_midi2_alt(), 0);
    setup_request(0x01, 0x0B, 1, 1, 0);                   /* SET_INTERFACE if1 alt1 */
    CHECK_EQ(drive_nodata_setup(), USB_SUCCESS); CHECK_EQ(usb_midi2_alt(), 1);
    CHECK_EQ(stub.fifo_flush_calls, 1); CHECK_EQ(stub.dtog_rx_clears[1], 1);
    CHECK_EQ(stub.ep_rx_status[1], 0x3000);               /* VALID */
}
static void test_get_interface_per_interface(void) {
    /* after alt 1 on if1: GET_INTERFACE wIndex 0 → 0, wIndex 1 → 1 */
}
static void test_invalid_alt_rejected(void) {
    CHECK_EQ(Device_Property.Class_Get_Interface_Setting(0, 1), USB_UNSUPPORT);
    CHECK_EQ(Device_Property.Class_Get_Interface_Setting(1, 2), USB_UNSUPPORT);
    CHECK_EQ(Device_Property.Class_Get_Interface_Setting(1, 1), USB_SUCCESS);
}
static void test_gtb_request_in_alt0(void) {
    usb_midi2_usb_reset();
    setup_request(0x81, 0x06, 0x2601, 1, 5);              /* header only */
    CHECK_EQ(drive_data_setup_len(), 5);
    setup_request(0x81, 0x06, 0x2601, 1, 255);
    CHECK_EQ(drive_data_setup_len(), 31);
    CHECK_MEM(drive_data_setup_bytes(), usb_gtb_desc, 31);
    setup_request(0x81, 0x06, 0x2600, 1, 255);            /* alt 0 has no GTB */
    CHECK_EQ(drive_data_setup_result(), USB_UNSUPPORT);
}
static void test_reset_restores_alt0(void) {
    /* alt 1 then Device_Property.Reset() → alt 0, EP1 PMA addresses TX 0x90 RX 0x50,
       EP0 RX 0x40 TX 0x48, EP1 RX count 64 */
}
static void test_reset_mid_traffic(void) {
    /* alt 1; push half a SysEx7 (start packet) and queue 3 notes; Reset();
       → ump_rx_free_words()==UMP_RX_WORDS, ump_tx_fill_packet()==0,
       then a fresh complete SysEx7 is delivered once; DinRx untouched (memcmp) */
}
```

`setup_request` fills `pInformation`, and the `drive_*` helpers call
`Device_Property.Class_Data_Setup` / `Class_NoData_Setup`, mimicking
`usb_core.o`. Put them in `tests/test_usb_prop.c`. Fill in the
comment-only tests.

- [ ] **Step 3: Run and see them fail**

Run: `make test` → compile failure.

- [ ] **Step 4: Implement `usb_compat.h`, `lib_private.h`, `usb_prop.c`, and the alt part of `usb_midi2.c`**

Reproduce each original behaviour recorded in Step 1, plus:
- `Class_Get_Interface_Setting` accepts `(0,0)`, `(1,0)` and `(1,1)`.
- `User_SetInterface` calls `usb_midi2_set_alt` when `wIndex0 == 1`.
- `User_GetInterface` sets
  `pInformation->Current_AlternateSetting = usb_midi2_iface_alt(wIndex0)`.
- `Class_Data_Setup(GET_DESCRIPTOR)` with interface recipient,
  `wValue1 == 0x26` and `wValue0 == 1` serves `usb_gtb_desc` truncated to
  `wLength` through the `CopyData` mechanism.
- Strings: product "Launchpad Pro MIDI 2.0", keeping the original
  device-ID suffix behaviour if Step 1 found one.

`__wrap_midi_parsers_init` calls `__real_midi_parsers_init()`, then copies
`USBParserToDev` into the snapshot.

- [ ] **Step 5: Run and see them pass**

Run: `make test` → `0 failures`.

- [ ] **Step 6: Make the firmware link**

Run: `make clean && make`
Expected: links with no undefined or duplicate symbols, `check_image.sh`
passes, and `.syx` is produced. (Data-path wraps are stubbed with
pass-throughs until Task 8, and `src/app.c` is unchanged.)

- [ ] **Step 7: Commit**

```bash
git add docs/notes src/usb src/platform/lib_private.h tests Makefile
git commit -m "usb: reimplement usb_prop with MIDI 2.0 alt setting, GTB request and EP1 ownership switch"
```

---

### Task 8: EP1 data path, `c_midi` gating and the `midi2.h` API

**Files:**
- Create: `include/midi2.h`, `src/usb/usb_endp.c`, `src/platform/platform.c`,
  `tests/test_usb_data.c`
- Modify: `src/usb/usb_midi2.c`, `tests/stubs/stubs.{h,c}`

**Interfaces:**
- Consumes: Tasks 2–7.
- Produces `include/midi2.h` (no `app.h` change). It includes `app_defs.h`
  and declares:

```c
u8   midi2_active(void);   /* alt 1 and the Standalone group is in MIDI 2.0 protocol */
u8   midi2_usb_alt(void);  /* 0 or 1; returns usb_midi2_alt() */
void hal_send_note_on2(u8 port, u8 ch, u8 note, u16 velocity);
void hal_send_note_off2(u8 port, u8 ch, u8 note, u16 velocity);
void hal_send_poly_pressure2(u8 port, u8 ch, u8 note, u32 value);
void hal_send_channel_pressure2(u8 port, u8 ch, u32 value);
void midi2_debug_pause_rx(u8 paused);  /* checkpoint only: stop draining UMP RX */
```

- Each `*2` function:
  - with `port == DINMIDI`, alt 0, or (alt 1 with MIDI 1.0 protocol or
    `port == USBMIDI`), sends the 7-bit equivalent: velocity
    `ump_scale_down(v, 16, 7)` with Note On clamped to ≥ 1; pressure
    `ump_scale_down(value, 32, 7)`;
  - in alt 0 and on DIN it goes through `__real_hal_send_midi`; in alt 1 it
    goes as MT 2;
  - otherwise it sends MT 4 through `ump_tx_push_attack`,
    `ump_tx_push_release` or `ump_tx_push_pressure`;
  - Note On/Off on the Standalone group also update the sounding table
    `uint8_t sounding[16][16]` (one bit per channel and note).
- `void midi2_protocol_changing(uint8_t old_proto)` sends Note Off, in the
  old protocol, for every bit set in `sounding`, then clears it.
- Wrapped `main.o` and app call sites in `usb_midi2.c`:
  - `__wrap_usbmidi_poll`: alt 0 → `__real_usbmidi_poll()`. Alt 1 → if
    `GetEPTxStatus(1) != 0x30` (not VALID) and `ump_tx_fill_packet(buf)`
    returns > 0, call `USB_SIL_Write(0x81, buf, n)` and
    `SetEPTxStatus(1, 0x30)`.
  - `__wrap_midi_flush_windows_buffer`: alt 0 → real; alt 1 → no-op.
  - `__wrap_midi_parsers_poll`:
    - alt 0 → real;
    - alt 1 → first `ump_rx_poll()` (unless paused);
    - then, if `GetEPRxStatus(1) == 0x2000` (NAK) and
      `ump_rx_free_words() >= 16`, call `SetEPRxValid(1)`;
    - then `midi_parse_rx_byte_stream(&MIDIDinIn, DinRx, 2)`.
  - `__wrap_hal_send_midi(port, status, d1, d2)`:
    - DIN or alt 0 → `__real_hal_send_midi`;
    - alt 1 → `ump_from_midi1(…, as_midi2 = (port == USBSTANDALONE && proto == MIDI2))`;
    - Note On/Off use attack/release, everything else
      `ump_tx_push(UMP_TX_NOTE…)`, and system messages go in
      `UMP_TX_CONTROL`.
  - `__wrap_hal_send_sysex`: DIN or alt 0 → real; alt 1 →
    `ump_sysex7_pack` into a stack buffer of 110 words, then a single
    `ump_tx_push(UMP_TX_CONTROL, …)`.
- `usb_endp.c` `EP1_OUT_Callback`:
  - alt 0 → the original behaviour exactly (from Task 7's notes);
  - alt 1 → `n = USB_SIL_Read(1, buf)`, truncate `n` down to a multiple of
    4, decode the little-endian words, `ump_rx_push_words`, and return.
    The endpoint stays NAK; the main loop re-arms it.
- `platform.c`: `platform_product_instance_id` returns 24 uppercase hex
  digits read from the STM32 UID at `0x1FFFF7E8` (12 bytes).

- [ ] **Step 1: Write failing tests**

```c
static void test_ep1_out_odd_length(void) {
    uint8_t p1[11] = {0x00,0x3C,0x90,0x40, 0x00,0x00,0xFF,0xFF, 0xAA,0xBB,0xCC};
    uint8_t p2[8]  = {0x00,0x3E,0x90,0x40, 0x00,0x00,0x00,0x80};
    enter_alt1(); stub_rx_packet(p1, 11); EP1_OUT_Callback(); __wrap_midi_parsers_poll();
    stub_rx_packet(p2, 8); EP1_OUT_Callback(); __wrap_midi_parsers_poll();
    CHECK_EQ(stub_midi_n, 2); CHECK_EQ(stub_midi_log[1].d1, 62);
}
static void test_backpressure_nak_until_space(void) {
    enter_alt1(); midi2_debug_pause_rx(1);
    /* feed 64-byte packets until ump_rx_free_words() < 16 */
    __wrap_midi_parsers_poll(); CHECK_EQ(stub.ep_rx_valid_calls, 0);
    midi2_debug_pause_rx(0); __wrap_midi_parsers_poll(); CHECK_EQ(stub.ep_rx_valid_calls, 1);
}
static void test_din_parsed_in_both_alts(void) {
    stubs_reset(); usb_midi2_usb_reset(); __wrap_midi_parsers_poll();
    CHECK_EQ(stub.real_parsers_poll_calls, 1);
    enter_alt1(); __wrap_midi_parsers_poll();
    CHECK_EQ(stub.din_parse_calls, 1); CHECK_EQ(stub.din_parse_last_port, 2);
    CHECK(stub.din_parse_last_parser == DinRx);
}
static void test_legacy_paths_gated_in_alt1(void) {
    enter_alt1(); __wrap_usbmidi_poll(); __wrap_midi_flush_windows_buffer();
    CHECK_EQ(stub.real_usbmidi_poll_calls, 0); CHECK_EQ(stub.real_flush_calls, 0);
}
static void test_note_on2_paths(void) {
    stubs_reset(); usb_midi2_usb_reset();                 /* alt 0 → 7-bit via real */
    hal_send_note_on2(USBSTANDALONE, 0, 60, 0x0100);
    CHECK_EQ(stub.real_midi_last[2], 1);                  /* clamped ≥ 1 */
    enter_alt1(); hal_send_note_on2(USBSTANDALONE, 0, 60, 0xABCD); __wrap_usbmidi_poll();
    CHECK_EQ(stub.last_tx_len, 8); CHECK_EQ(stub.last_tx[2], 0x90); CHECK_EQ(stub.last_tx[6], 0xCD);
    hal_send_note_on2(USBMIDI, 0, 60, 0xFFFF); __wrap_usbmidi_poll();
    CHECK_EQ(stub.last_tx_len, 4); CHECK_EQ(stub.last_tx[0], 0x7F);   /* MT 2, group 1 */
}
static void test_protocol_change_releases_notes(void) {
    /* alt 1 MIDI 2; note_on2 ch0 60; stream-config request to MIDI 1 via RX;
       poll → first TX packet contains MIDI 2.0 Note Off 0x40803C00 before 0xF0060100 */
}
static void test_hal_send_sysex_alt1(void) {
    /* 20-byte F0..F7 → 4 SysEx7 packets on group 0 sent in order through EP1 IN */
}
```

`enter_alt1()` = `stubs_reset(); usb_midi2_usb_reset(); usb_midi2_set_alt(1);`.
Fill in the comment-only tests.

- [ ] **Step 2: Run and see them fail**

Run: `make test` → link failure (wraps undefined).

- [ ] **Step 3: Implement**

Implement `include/midi2.h`, `src/usb/usb_endp.c`, the data part of
`src/usb/usb_midi2.c` and `src/platform/platform.c`, per the Interfaces
above.

- [ ] **Step 4: Run and see them pass**

Run: `make test` → `0 failures`.

- [ ] **Step 5: Firmware build and size**

Run: `make clean && make && arm-none-eabi-size build/launchpad_pro.elf`
Expected: links; `check_image.sh` passes; `bss` grows by ≤ 4096 bytes over
the 6360-byte baseline. Record the numbers for Task 9.

- [ ] **Step 6: Commit**

```bash
git add include/midi2.h src/usb src/platform tests Makefile
git commit -m "usb: add UMP EP1 data path, c_midi gating and midi2.h API"
```

---

### Task 9: Checkpoint app, simulator and hardware procedure

**Files:**
- Modify: `src/app.c`, `Makefile` (simulator sources)
- Create: `tools/sim_midi2.c`, `docs/checkpoint-1-transport.md`

**Interfaces:**
- Consumes: `include/midi2.h`, `ump_scale_up`, `ADC_Max`, `ADC_MAP`.
- Produces the checkpoint app behaviour:
  - Pad press → `hal_send_note_on2(USBSTANDALONE, 0, index, v)`, where
    `v = ump_scale_up(ADC_Max[adc_of(index)] & 0x0FFF, 12, 16)` and
    `adc_of` is the inverse of `ADC_MAP` (built in `app_init`).
  - Pad release → `hal_send_note_off2(…, 0)`.
  - The pad lights while held, with brightness from `v >> 10`.
  - Example app behaviour kept: the bidirectional bridge `USBMIDI` ↔
    `DINMIDI` for channel voice and system messages, and the MIDI clock on
    `USBSTANDALONE`.
  - The Setup LED is updated every 100 ms: green (0,63,0) if
    `midi2_active()`, amber (63,20,0) if alt 1 with MIDI 1.0 protocol, red
    (63,0,0) in alt 0 (using `midi2_usb_alt()` from Task 8).
  - Setup button held → `midi2_debug_pause_rx(1)`; released → `(0)`.
- `tools/sim_midi2.c` defines:
  - `u16 ADC_Max[64]`;
  - every `midi2.h` function, printing its arguments the way
    `tools/simulator.c` prints MIDI;
  - `ump_scale_up` by compiling `src/ump/ump_codec.c` into the simulator.

- [ ] **Step 1: Make the simulator build fail on the new app**

Change `src/app.c` to call the `midi2.h` functions, then run
`make build/simulator`.
Expected: FAIL with undefined `hal_send_note_on2`.

- [ ] **Step 2: Add `tools/sim_midi2.c` and wire the simulator sources**

Run: `make clean && make`
Expected: the simulator runs during `make` without errors, `check_image.sh`
passes, and `build/launchpad_pro.syx` is produced.

- [ ] **Step 3: Full verification**

Run: `make test && make clean && make && arm-none-eabi-size build/launchpad_pro.elf`
Expected: `0 failures`, a clean build, and the size numbers.

- [ ] **Step 4: Write `docs/checkpoint-1-transport.md`**

Sections, with a results table the player fills in for each check:
1. Flashing the bootloader (hold Setup at power-on, send `.syx`) and
   recovery (reflash the stock `resources/Launchpad Pro-1.0.154.syx`).
2. Recording the Windows build (`winver`) and the MIDI driver version
   (Device Manager → driver details; `midi --version`).
3. Endpoint check in the Windows MIDI Services console: `midi endpoint list`
   should show "Launchpad Pro MIDI 2.0", MIDI 2.0 protocol, and Function
   Blocks "Standalone Port" and "MIDI Port".
4. Note check (`midi endpoint monitor`): soft, medium and hard hits on pad
   11 should give 16-bit velocities spread across the range, and release
   should give a Note Off. Also record the raw velocity range seen.
5. Stream Configuration cycle on one alt 1 connection: send `0xF0050100`
   (MIDI 1.0), play, send `0xF0050200` (MIDI 2.0), play. Expect the replies
   `0xF0060100` / `0xF0060200`, notes arriving in each protocol, and the
   Setup LED turning amber, then green.
6. EP1 capture with USBPcap and Wireshark across alt 0 → 1 → 0, made by
   switching the device driver in Device Manager between the in-box USB
   Audio driver and the USB MIDI 2.0 driver, while a DIN source feeds the
   MIDI In breakout. Confirm:
   - DIN input reaches the PC throughout;
   - framing is 4-byte USB-MIDI event packets in alt 0 and UMP words in
     alt 1;
   - with Setup held (RX drain paused) and the host flooding messages, OUT
     transfers stall (NAK) until Setup is released.
7. MIDI 1.0 fallback on a PC without Windows MIDI Services: both ports
   appear, and the bridge works in both directions.
8. Reconnect and USB reset during traffic.
9. What to send back: the results table, the capture files, and any
   console output.

- [ ] **Step 5: Commit**

```bash
git add src/app.c tools/sim_midi2.c Makefile docs/checkpoint-1-transport.md
git commit -m "app: add stage 1 checkpoint app, simulator support and hardware procedure"
```

---

## Self-review notes

- Spec coverage (§1, §2, transport parts of §7, stage 1):
  - descriptors: Task 6;
  - control path and alt ownership: Task 7;
  - UMP RX/TX and stream: Tasks 2–5;
  - wraps, `c_midi` gating and the API: Task 8;
  - flash cap and image check: Task 1;
  - checkpoint items from the review: Task 9 §5–6.
- Deferred to the stage 2 and 3 plans: the pad engine, scales, menu,
  persistence and MIDI-CI. Pressure coalescing is built now (Task 4)
  because it's transport.
- Names used across tasks:
  - `ump_tx_push`, `ump_tx_push_attack`, `ump_tx_push_release`,
    `ump_tx_push_pressure` and `ump_tx_fill_packet`;
  - `ump_rx_push_words`, `ump_rx_poll` and `ump_rx_free_words`;
  - `usb_midi2_set_alt`, `usb_midi2_usb_reset`, `usb_midi2_alt` and
    `midi2_usb_alt`. The last one is the public alias in `midi2.h`, and
    Task 8 defines it as returning `usb_midi2_alt()`.
