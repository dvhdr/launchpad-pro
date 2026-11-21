# Checkpoint 1: USB MIDI 2.0 transport, hardware procedure

This is the stage 1 hardware checkpoint. The firmware is a small test app on
top of the new USB MIDI 2.0 transport. You flash it, run the checks below on
Windows, fill in the results table and send it back with the capture files.

Download the firmware file `launchpad_pro_stage1.syx` that you were sent (it
is the `build/launchpad_pro.syx` of this commit). It is not stored in the
repository.

## What the test app does

- Pad press: a MIDI 2.0 Note On on the Standalone port (UMP group 0), channel
  1, note number = pad index (pad 11 = note 11), with a 16-bit velocity scaled
  up from the 12-bit pad pressure peak. Pad release: Note Off.
- The pad lights while held, brighter for harder hits. Pad pressure while
  held is sent as Poly Pressure on the Standalone port (32-bit in the MIDI 2.0
  Protocol).
- The "MIDI" port (UMP group 1) is bridged to the DIN port in both directions.
  MIDI clock is sent on the Standalone port.
- The Setup LED shows the USB state: green = MIDI 2.0 active, amber = UMP
  alternate setting with the MIDI 1.0 Protocol, red = alternate setting 0
  (plain USB MIDI 1.0).
- Holding the Setup button pauses receiving of UMP from the host (for the
  backpressure test in section 6). Releasing it resumes.
- The UMP Endpoint Name the firmware reports is "Launchpad Pro MIDI 2.0",
  with no suffix. Only the USB product name (see Expected identity) gets the
  " 16".

## Expected identity

| Item | Stock firmware | Open firmware, expected (verify) |
|---|---|---|
| VID | 0x1235 | 0x1235 |
| PID | 0x0060 | 0x4060 |
| Product name | "Launchpad Pro 16" | "Launchpad Pro MIDI 2.0 16" |
| Driver | wdma_usb.inf 10.0.28000.3531 (MIDI 1.0 class driver) | USB MIDI 2.0 class driver (verify) |

Why: the bootloader device ID is 16 on this unit. The boot code patches the
PID low byte to base + (id - 1) and appends " 16" to the product name. The
stock base is 0x0051 (0x0051 + 15 = 0x0060). The open firmware base is 0x4051,
so 0x4051 + 15 = 0x4060, and "Launchpad Pro MIDI 2.0" + " 16" = "Launchpad Pro
MIDI 2.0 16" (25 characters, the limit). See `docs/notes/usb_prop_original.md`.
Windows may show a slightly different friendly name; record what you see.

## Your setup (pre-filled)

- Windows 11 build 10.0.28020 (26H1)
- Windows MIDI Services Console: SDK Release Candidate 4 (GitHub Preview)
  1.0.17-rc.4.25

## Notes on the console commands

I could not run the Windows MIDI Services console, so the commands below are
my best understanding of its syntax. If a command is rejected, run
`midi --help` or `midi endpoint --help` and use the equivalent. Report any
syntax that differed. Do not guess at output: paste what you actually see.

## 1. Flashing, and recovery

Warning: do not unplug the Launchpad Pro, or close the SysEx sender, while the
upload is running. The bootloader is protected so the unit cannot be bricked,
but an interrupted upload leaves it in bootloader mode until you reflash.

1. Unplug the Launchpad Pro from USB (and the DIN cables if any).
2. Hold the Setup button and, while holding it, plug in the USB cable (plug it
   into the PC itself, not a virtual machine). The unit starts in bootloader
   mode.
3. In a SysEx sender (for example MIDI-OX: Actions > Send > SysEx File), send
   `launchpad_pro_stage1.syx` to the Launchpad Pro MIDI port. The grid scrolls
   "upgrading...". A short delay between SysEx buffers is fine; a low value
   keeps the upload quick, but if the upload fails, raise it.
4. Wait until the unit reboots by itself.

If the unit does not boot, or you want the factory firmware back: repeat the
steps with `resources/Launchpad Pro-1.0.154.syx` from this repository.

Recovery notes:

- Always enter the bootloader by holding Setup while plugging in, as above.
  Updater tools that reboot the unit into the bootloader with a SysEx command
  do not work with this firmware while it runs the MIDI 2.0 setting (alt 1):
  that SysEx no longer reaches the library. The Setup-held power-on method
  always works, because the bootloader runs before the firmware.
- If MIDI-OX cannot send the file to the bootloader through Windows MIDI
  Services, switch the device to the legacy driver (Device Manager, as in
  section 6 step 2) or use a Windows PC without Windows MIDI Services, and
  send it from there.

Result: flashed OK? (yes/no), notes.

## 2. Record the Windows build and driver version

1. Run `winver`. Record the build (expected 10.0.28020).
2. Run `midi --version`. Record the output (expected 1.0.17-rc.4.25).
3. Device Manager > find the Launchpad Pro (under Sound, video and game
   controllers, or Universal Serial Bus devices) > Properties > Driver >
   Driver Details. Record the driver file and version. Before flashing the
   stock unit showed wdma_usb.inf 10.0.28000.3531.
4. Properties > Details > Hardware Ids: record VID and PID (expected
   VID_1235 and PID_4060).

## 3. Endpoint check

Run `midi endpoint list` (check `midi --help` if the syntax differs).
Expected, to verify:

- an endpoint named "Launchpad Pro MIDI 2.0" (the UMP Endpoint Name has no
  " 16" suffix; Windows may show the USB product name "Launchpad Pro MIDI 2.0
  16" in other places, so record which name appears where);
- native protocol MIDI 2.0;
- two Function Blocks, "Standalone Port" and "MIDI Port".

Also run `midi endpoint list --verbose` or `midi endpoint <id> properties` if
available (check `midi endpoint --help`), and paste the output.

## 4. Note check

Start the monitor on the Launchpad endpoint, for example
`midi endpoint monitor` (check `midi endpoint --help`; you may need to pass the
endpoint id or name from step 3). Then hit pad 11 softly, medium and hard,
several times each, releasing each time.

Expected, to verify:

- each hit gives a Note On with a 16-bit velocity (not only multiples of 512
  as would come from 7-bit scaling), with soft hits low and hard hits high.
  The softest hit that triggers a note gives about 0x1500 (the pad trigger
  threshold), not a value near 0. Below 0x8000 the values step by 16 (the pad
  peak is 12 bits); above it the steps are 16 or 17;
- each release gives a Note Off;
- the pad lights, brighter for harder hits;
- MIDI clock arrives on group 0 about every 20 ms.

Record the raw velocity range you saw (lowest and highest), and for each
softness a typical value.

Then check the "MIDI Port" (UMP group 1) bridge to DIN in this MIDI 2.0
setting. Connect a DIN MIDI sound module or MIDI monitor to the DIN MIDI Out,
and a DIN MIDI source to the DIN MIDI In.

1. USB to DIN: send a Note On to group 1, channel 1, note 60, velocity 100,
   then its Note Off, for example
   `midi endpoint send-message 0x21903C64` and then
   `midi endpoint send-message 0x21803C00` (same syntax notes as section 5).
   Expected: the note sounds on (or shows in) the DIN device, then stops.
   Group 1 is always MIDI 1.0 (MT 2), so these single words are right.
2. DIN to USB: play a few notes on the DIN source. Expected: in the monitor
   they appear as MIDI 1.0 Protocol (MT 2) messages on group 1, for example
   `0x219nkkvv` for a Note On (n = channel - 1, kk = note, vv = velocity).

## 5. Stream Configuration cycle (one alt 1 connection)

Without unplugging, on the same MIDI 2.0 connection, with the monitor running
in a second window:

A Stream message is four 32-bit words; send all four.

1. Hold pad 11 down and keep holding it through step 2.
2. Send the Stream Configuration Request for the MIDI 1.0 Protocol, the four
   words `0xF0050100 0x00000000 0x00000000 0x00000000`. For example
   `midi endpoint send-message 0xF0050100 0x00000000 0x00000000 0x00000000`
   (check `midi endpoint --help`; the command may need the endpoint id, and
   the words may need a different separator).
3. Expected: first a MIDI 2.0 Protocol (MT 4) Note Off for note 11 (the held
   pad, in the old protocol), then the reply `0xF0060100` (Stream
   Configuration Notification). The Setup LED turns amber. Release pad 11:
   a second Note Off for note 11, now as MIDI 1.0 Protocol (MT 2), is
   expected and harmless. Pad notes now arrive as MIDI 1.0 Protocol (MT 2)
   messages.
4. Hold pad 11 again and send the request for the MIDI 2.0 Protocol,
   `0xF0050200 0x00000000 0x00000000 0x00000000`.
5. Expected: first a MIDI 1.0 Protocol (MT 2) Note Off for note 11, then the
   reply `0xF0060200`. The Setup LED turns green. Release pad 11 (again a
   second Note Off, now MT 4). Pad notes now arrive as MIDI 2.0 Protocol
   (MT 4) messages with 16-bit velocity.

Poly Pressure messages for the held pad may appear in between; they are
expected. Record the replies seen and their order relative to the Note Off,
the LED colours, and whether notes arrived in each protocol. Also say whether
discovery replies (Endpoint, Function Block info) still work after each
change, for example by running `midi endpoint list` again.

## 6. EP1 capture across alt 1, alt 0, alt 1

Needs USBPcap and Wireshark. Also feed the MIDI In breakout (DIN) from a DIN
MIDI source (keyboard or other controller) sending a steady stream, for
example notes or clock.

Capture the Launchpad's USB bus with USBPcap and filter on EP1 (the MIDI
streaming bulk endpoints). During the capture:

1. Start in the state Windows selected (alt 1, UMP, MIDI Services).
2. In Device Manager, switch the Launchpad's driver to the in-box USB Audio
   driver (Update driver > Browse > Let me pick), which uses alt 0 (USB MIDI
   1.0). Then switch it back to the USB MIDI 2.0 driver, which selects alt 1.
3. Play DIN input and pads throughout.

Confirm and record:

- DIN input reaches the PC throughout, including across both switches;
- the framing in alt 0 is 4-byte USB-MIDI event packets, and in alt 1 it is
  UMP 32-bit words (little-endian);
- backpressure, in alt 1: hold Setup (receive paused) and send a burst of
  messages from the host (for example repeated `midi endpoint send-message`,
  or a MIDI tool sending notes to the endpoint). The device NAKs EP1 OUT once
  its receive queue fills. USBPcap does not show NAK handshakes; what you
  should see is the last OUT URB staying pending (no completion) while Setup
  is held, and the sending command blocking or timing out. Release Setup:
  the pending URB completes and the rest follows. Describe what you saw (URB
  pending, command blocked or timed out, and for how long). Keep the burst
  short: flooding while paused can back up the shared Windows MIDI service
  and delay other MIDI apps until Setup is released.

Save the capture files (.pcapng).

## 7. MIDI 1.0 fallback

Get the Launchpad onto alternate setting 0 (USB MIDI 1.0) in one of two ways:

- on this PC, switch its driver to the in-box legacy USB Audio driver in
  Device Manager, as in section 6 step 2; or
- connect it to a Windows PC without Windows MIDI Services.

Stopping the Windows MIDI Service is not enough: the USB MIDI 2.0 class
driver still binds to the device and selects alt 1 (Setup LED green or
amber). Check that the Setup LED is red before going on. Expected to verify:

- two MIDI ports appear: "Standalone Port" and "MIDI Port" (names may be
  prefixed with the device name);
- the Setup LED is red (alt 0);
- pads send note on and off on the Standalone port (7-bit velocity);
- bridge, USB to DIN: a note sent to the "MIDI Port" comes out of the DIN
  MIDI Out;
- bridge, DIN to USB: a note sent into the DIN MIDI In arrives on the
  "MIDI Port".

## 8. Reconnect and USB reset during traffic

1. With the DIN source sending, the pads in use and the monitor running,
   unplug and replug USB three times. Record whether the endpoint comes back,
   whether notes still arrive, and the Setup LED colour after each.
   Unplug and replug is the USB bus reset check.
2. Also with traffic running, in Device Manager disable and enable the
   Launchpad device. Windows does not always reset the bus for this; it does
   re-select the configuration and the alternate setting. Record the same.
3. Record any hang, any stuck note, any missed note, any lasting red LED.

## 9. Results table

Fill in "Result" and "Notes". "Expected" is what I predict.

| # | Check | Expected (verify) | Result | Notes |
|---|---|---|---|---|
| 1 | Flash the stage 1 image | Flashes and reboots | | |
| 1b | Recovery image flashes (only if tested) | Stock firmware boots | | |
| 2a | Windows build | 10.0.28020 (26H1) | | |
| 2b | `midi --version` | 1.0.17-rc.4.25 | | |
| 2c | Driver file and version | MIDI 2.0 class driver (stock was wdma_usb.inf 10.0.28000.3531) | | |
| 2d | VID and PID | VID_1235 PID_4060 | | |
| 3a | Endpoint name | Launchpad Pro MIDI 2.0 (no suffix; the USB product name is "... 16") | | |
| 3b | Protocol | MIDI 2.0 | | |
| 3c | Function Blocks | "Standalone Port" and "MIDI Port" | | |
| 4a | Pad 11 Note On, 16-bit velocity | Soft low, hard high, many distinct values | | |
| 4b | Raw velocity range seen | (record low and high) | | |
| 4c | Pad 11 release gives Note Off | Yes | | |
| 4d | Clock on group 0 | About every 20 ms | | |
| 4e | Alt 1: group 1 note to DIN Out | Sounds on the DIN device, then stops | | |
| 4f | Alt 1: DIN In to USB | MT 2 messages on group 1 | | |
| 5a | Request `0xF0050100` reply | MT 4 Note Off for held pad 11, then `0xF0060100`; LED amber; notes as MIDI 1.0 Protocol | | |
| 5b | Request `0xF0050200` reply | MT 2 Note Off for held pad 11, then `0xF0060200`; LED green; notes as MIDI 2.0 Protocol | | |
| 6a | DIN input continues across alt 1, 0, 1 | Yes | | |
| 6b | Framing in alt 0 | 4-byte USB-MIDI event packets | | |
| 6c | Framing in alt 1 | UMP 32-bit words | | |
| 6d | Setup held plus burst | Last OUT URB pending / sender blocks until Setup released | | |
| 7a | Fallback: both ports visible | Yes, LED red | | |
| 7b | Fallback: USB to DIN | Works | | |
| 7c | Fallback: DIN to USB | Works | | |
| 8a | Replug x3 during traffic | Recovers each time | | |
| 8b | Disable and enable during traffic (may not reset the bus) | Recovers | | |

## 10. What to send back

- The filled results table (section 9).
- The capture files (.pcapng) from section 6.
- Any console output from the `midi` commands (copy and paste the text),
  including any command whose syntax differed from this document.
- The Windows build and driver versions from section 2.
- Anything odd: stuck notes, wrong LED colour, hangs, resets.
