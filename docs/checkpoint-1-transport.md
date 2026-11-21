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
- The pad lights while held, brighter for harder hits.
- The "MIDI" port (UMP group 1) is bridged to the DIN port in both directions.
  MIDI clock is sent on the Standalone port.
- The Setup LED shows the USB state: green = MIDI 2.0 active, amber = UMP
  alternate setting with the MIDI 1.0 Protocol, red = alternate setting 0
  (plain USB MIDI 1.0).
- Holding the Setup button pauses receiving of UMP from the host (for the
  backpressure test in section 6). Releasing it resumes.

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

- an endpoint named "Launchpad Pro MIDI 2.0" (possibly with the device suffix);
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
  as would come from 7-bit scaling), with soft hits low and hard hits high;
- each release gives a Note Off;
- the pad lights, brighter for harder hits;
- MIDI clock arrives on group 0 about every 20 ms.

Record the raw velocity range you saw (lowest and highest), and for each
softness a typical value.

## 5. Stream Configuration cycle (one alt 1 connection)

Without unplugging, on the same MIDI 2.0 connection, with the monitor running
in a second window:

1. Send the Stream Configuration Request `0xF0050100` (MIDI 1.0 Protocol).
   For example `midi endpoint send-message 0xF0050100` (check
   `midi endpoint --help`; the command may need the endpoint id, and the
   message may need to be given as a word list).
2. Expected: a reply `0xF0060100`, the Setup LED turns amber, and pad notes
   arrive as MIDI 1.0 Protocol (MT 2) messages.
3. Send `0xF0050200` (MIDI 2.0 Protocol).
4. Expected: a reply `0xF0060200`, the Setup LED turns green, and pad notes
   arrive as MIDI 2.0 Protocol (MT 4) messages with 16-bit velocity.

Record the replies seen, the LED colours, and whether notes arrived in each
protocol. Also say whether discovery replies (Endpoint, Function Block info)
still work after each change, for example by running `midi endpoint list`
again.

## 6. EP1 capture across alt 0, alt 1, alt 0

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
- backpressure: with Setup held (receive paused) and the host flooding
  messages (for example repeated `midi endpoint send-message`, or a MIDI
  tool sending many notes to the endpoint), the OUT transfers on EP1 stall
  (NAK) once the receive queue fills, and continue after Setup is released.
  Describe what you saw in the capture (NAK or timeout, and how long).

Save the capture files (.pcapng).

## 7. MIDI 1.0 fallback

On a PC without Windows MIDI Services (or with the service stopped, or a VM),
connect the Launchpad. Expected to verify:

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
2. Also with traffic running, in Device Manager disable and enable the
   Launchpad device (this resets the device on the bus). Record the same.
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
| 3a | Endpoint name | Launchpad Pro MIDI 2.0 (suffix " 16") | | |
| 3b | Protocol | MIDI 2.0 | | |
| 3c | Function Blocks | "Standalone Port" and "MIDI Port" | | |
| 4a | Pad 11 Note On, 16-bit velocity | Soft low, hard high, many distinct values | | |
| 4b | Raw velocity range seen | (record low and high) | | |
| 4c | Pad 11 release gives Note Off | Yes | | |
| 4d | Clock on group 0 | About every 20 ms | | |
| 5a | Request `0xF0050100` reply | `0xF0060100`, LED amber, notes as MIDI 1.0 Protocol | | |
| 5b | Request `0xF0050200` reply | `0xF0060200`, LED green, notes as MIDI 2.0 Protocol | | |
| 6a | DIN input continues across alt 0, 1, 0 | Yes | | |
| 6b | Framing in alt 0 | 4-byte USB-MIDI event packets | | |
| 6c | Framing in alt 1 | UMP 32-bit words | | |
| 6d | Setup held plus flood | OUT stalls (NAK) until Setup released | | |
| 7a | Fallback: both ports visible | Yes, LED red | | |
| 7b | Fallback: USB to DIN | Works | | |
| 7c | Fallback: DIN to USB | Works | | |
| 8a | Replug x3 during traffic | Recovers each time | | |
| 8b | Disable and enable during traffic | Recovers | | |

## 10. What to send back

- The filled results table (section 9).
- The capture files (.pcapng) from section 6.
- Any console output from the `midi` commands (copy and paste the text),
  including any command whose syntax differed from this document.
- The Windows build and driver versions from section 2.
- Anything odd: stuck notes, wrong LED colour, hangs, resets.
