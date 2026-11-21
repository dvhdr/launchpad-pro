# Original `usb_prop.o`, `usb_desc.o`, `usb_endp.o` (closed `lib/launchpad_pro.a`)

Recovered with `arm-none-eabi-ar x lib/launchpad_pro.a` (into a scratch
directory, never in-tree), `arm-none-eabi-objdump -d -r`, `objdump -s` and
`readelf -r`. Compiler: GCC 5.4.1 (ARM Embedded 2016q3), `-Os
-ffunction-sections`, short enums (`Tag_ABI_enum_size = small`). The objects
are built on ST's USB-FS device library v3.x; the field offsets below were read
off the code, not taken from ST sources.

Conventions: `pInformation` → `DEVICE_INFO` (28 bytes):

| offset | field |
|---|---|
| 0 | `USBbmRequestType` |
| 1 | `USBbRequest` |
| 2, 3 | `USBwValues`: byte 2 = `wValue1` (high byte), byte 3 = `wValue0` (low byte) |
| 4, 5 | `USBwIndexs`: byte 4 = `wIndex1`, byte 5 = `wIndex0` |
| 6 | `USBwLengths` (not byte-swapped) |
| 8 | `ControlState` |
| 9 | `Current_Feature` |
| 10 | `Current_Configuration` |
| 11 | `Current_Interface` |
| 12 | `Current_AlternateSetting` |
| 16 | `Ctrl_Info.Usb_wLength` |
| 18 | `Ctrl_Info.Usb_wOffset` |
| 20 | `Ctrl_Info.PacketSize` |
| 24 | `Ctrl_Info.CopyData` |

`Setup0_Process` (usb_core.o) stores `ByteSwap(wValue)` and `ByteSwap(wIndex)`
as halfwords, so in memory the high byte of the request field comes first
(`bb1` at offset 0, `bb0` at offset 1); `wLength` is stored as is.

`bDeviceState` is a 32-bit word (usb_pwr.o; `ldr`/`str`): 0 UNCONNECTED,
1 ATTACHED, 4 ADDRESSED, 5 CONFIGURED. `PowerMode` is a byte in c_init.o's
`.data` (`ldrb`).

## `usb_prop.o` data layout (`.data`, 0x9A bytes)

| offset | symbol | contents |
|---|---|---|
| 0x00 | `Device_Property` (48 B) | `Device_Init`, `Device_Reset`, `Device_Status_In`, `Device_Status_Out`, `Device_Data_Setup`, `Device_NoData_Setup`, `Device_Get_Interface_Setting`, `Device_GetDeviceDescriptor`, `Device_GetConfigDescriptor`, `Device_GetStringDescriptor`; `RxEP_buffer` = 0 at +0x28; `MaxPacketSize` = 8 at +0x2C |
| 0x30 | `Device_Descriptor` | `{ DeviceDescriptor, 0x12 }` |
| 0x38 | `USBDescriptors[6]` | pointers: `USBDesc_LangID`, `USBDesc_StringVendor`, `USBDesc_StringSerial`, `USBDesc_StringSerial`, `USBDesc_StringPort1`, `USBDesc_StringPort2` |
| 0x50 | `Config_Descriptor_psu` | `{ ConfigDescriptor_psu, 0x85 }` |
| 0x58 | `Config_Descriptor` | `{ ConfigDescriptor, 0x85 }` |
| 0x60 | `USBDesc_StringSerial` (20 B) | `"Launchpad Open"` then six zero bytes |
| 0x74 | `User_Standard_Requests` (9 ptrs) | `NOP_Process` (GetConfiguration), `Device_SetConfiguration`, `NOP_Process` ×6 (GetInterface, SetInterface, GetStatus, ClearFeature, SetEndPointFeature, SetDeviceFeature), `Device_SetDeviceAddress` |
| 0x98 | `Device_Table` | `{ Total_Endpoint = 2, Total_Configuration = 1 }` |

`.rodata`: `USBDesc_StringPort2` = `"MIDI Port"` (0x00), `USBDesc_StringPort1` =
`"Standalone Port"` (0x0A), `USBDesc_StringVendor` = `"Focusrite A.E. Ltd"`
(0x1A), `USBDesc_LangID` = `09 04` (0x2D). All NUL-terminated ASCII.

`.bss`: `MUTE_DATA` (4 bytes, global, referenced by no other object in the
archive). COMMON: `descriptor_buffer` (64 bytes, only used here).

Symbols other archive members take from these three objects:
`Device_Property` (usb_core, usb_init, usb_istr), `Device_Table` (usb_core),
`User_Standard_Requests` (usb_init), `usb_unconnected` (main.o),
`DeviceDescriptor` and `USBDesc_StringSerial` (device_init.o, which **writes**
both, see below), `EP1_OUT_Callback` (usb_istr.o).

## `usb_desc.o`

`.data` `DeviceDescriptor` (18 bytes, writable):
`12 01 00 02 00 00 00 08 35 12 51 40 00 02 01 02 03 01` — iManufacturer 1,
iProduct 2, iSerialNumber 3. `.rodata` `ConfigDescriptor_psu` at 0 and
`ConfigDescriptor` at 0x85 (133 bytes each; bMaxPower 0x01 and 0xFA). No code.
Only `DeviceDescriptor` is used outside usb_prop.o.

## Functions

### `Device_Init`
Reads `bDeviceState` into a register and never uses it (dead load), calls
`PowerOn()`, then tail-calls `USB_SIL_Init()`. Does **not** clear
`Current_Configuration` or set `bDeviceState`.

### `Device_Reset`
1. If `bDeviceState == 5` (CONFIGURED) → `USBUnconfigured()` (main.o; sets two
   flags in main.o `.bss` for `remember_USBUnconfigured`).
2. `desc = (PowerMode == 1) ? ConfigDescriptor : ConfigDescriptor_psu`;
   `Current_Feature = desc[7]` (bmAttributes);
   `Current_Configuration = 0`; `Current_Interface = 0`.
   `Current_AlternateSetting` is not touched.
3. `SetBTABLE(0)`.
4. EP0: `SetEPType(0, 0x200 CONTROL)`, `SetEPTxStatus(0, 0x10 STALL)`,
   `SetEPRxAddr(0, 0x40)`, `SetEPTxAddr(0, 0x48)`, `Clear_Status_Out(0)`,
   `SetEPRxCount(0, Device_Property.MaxPacketSize)` (8), `SetEPRxValid(0)`.
5. EP1: `SetEPType(1, 0 BULK)`, `SetEPTxAddr(1, 0x90)`, `SetEPTxCount(1, 64)`,
   `SetEPTxStatus(1, 0x20 NAK)`, `SetEPRxAddr(1, 0x50)`, `SetEPRxCount(1, 64)`,
   `SetEPRxStatus(1, 0x3000 VALID)`.
6. `SetEPRxValid(0)` (again), `SetDeviceAddress(0)`, `bDeviceState = 1`
   (ATTACHED).

Note the two different power-mode tests: Reset picks the bus descriptor only
for `PowerMode == 1`; `Device_GetConfigDescriptor` picks the PSU descriptor only
for `PowerMode == 2`. Both descriptors have bmAttributes 0x80, so the
difference is invisible today.

### `Device_SetConfiguration` (User_SetConfiguration)
If `Device_Info.Current_Configuration != 0` → `bDeviceState = 5` and tail-call
`USBConfigured()` (main.o; sets both flags to 1). Otherwise does nothing (no
`USBUnconfigured`, no state change). The only `USBUnconfigured` calls are in
`Device_Reset` and `Device_SetDeviceAddress`.

### `Device_SetDeviceAddress` (User_SetDeviceAddress)
If `bDeviceState == 5` → `USBUnconfigured()`. Then `bDeviceState = 4`
(ADDRESSED).

### `Device_Status_In`, `Device_Status_Out`
Empty (`bx lr`).

### `Device_Data_Setup(RequestNo)` (Class_Data_Setup)
```
if ((RequestNo & 0x7F) == 1) {          /* bRequest 0x01 or 0x81 */
    pInformation->Ctrl_Info.CopyData = 0;
    pInformation->Ctrl_Info.Usb_wOffset = 0;
    ((fn)0)(0);                          /* blx r4 with r4 = 0 */
    return USB_SUCCESS;
}
return USB_UNSUPPORT;
```
A copy routine that is always NULL: a class request with bRequest 0x01/0x81
(e.g. audio SET_CUR/GET_CUR) with a data stage would branch to address 0
(HardFault). Everything else, including every interface-recipient
GET_DESCRIPTOR, returns USB_UNSUPPORT (→ STALL). The reimplementation does
not keep the jump to 0: it returns USB_UNSUPPORT for everything except the
Group Terminal Block request.

### `Device_NoData_Setup(RequestNo)` (Class_NoData_Setup)
Returns USB_UNSUPPORT (2) for everything.

### `Device_Get_Interface_Setting(Interface, AlternateSetting)`
```
if (AlternateSetting > 1) return USB_UNSUPPORT;
if (Interface > 1) return USB_UNSUPPORT;
return USB_SUCCESS;
```
So the original already accepted alt 1 on both interfaces (0,1) and (1,1),
although its descriptor has no alt 1. New rule: only (0,0), (1,0), (1,1).

### `Device_GetDeviceDescriptor(Length)`
Tail-calls `Standard_GetDescriptorData(Length, &Device_Descriptor)` — i.e. the
RAM `DeviceDescriptor` (so the PID patch below is what the host sees).

### `Device_GetConfigDescriptor(Length)`
`Standard_GetDescriptorData(Length, PowerMode == 2 ? &Config_Descriptor_psu :
&Config_Descriptor)`.

### `Device_GetStringDescriptor(Length)`
`idx = pInformation->USBwValue0` (byte 3). `idx > 5` → return NULL (STALL).
The descriptor is built into `descriptor_buffer` (64 bytes):
- idx 0: bytes 2,3 = `USBDescriptors[0][0..1]` (`09 04`), length 4.
- idx 1..5: ASCII string `USBDescriptors[idx]` → UTF-16LE: char *i* at
  bytes `2+2i` (char) and `3+2i` (0). Stops at NUL or when the length would
  pass 64 (at most 31 characters); length = 2 + 2·chars.
- byte 0 = length, byte 1 = 3 (STRING); then
  `Standard_GetDescriptorData(Length, &{descriptor_buffer, length})` with the
  ONE_DESCRIPTOR on the stack.

The descriptor is rebuilt from offset 0 on every call (including the
`Length != 0` copy calls), so the multi-packet transfer reads a stable buffer.

Index map: 0 LangID 0x0409, 1 "Focusrite A.E. Ltd", **2 and 3 both
`USBDesc_StringSerial`** ("Launchpad Open[ N]"), **4 "Standalone Port"**,
**5 "MIDI Port"**.

### Device-ID suffix (`device_setuppid`, device_init.o)
`init_device` (c_init.o, called from `main` before USB is started) reads the
64-byte block at 0x0801FC00 and calls `device_setuppid(block[1])`
(BootloaderID):
```
if (id > 15) id = 0;
DeviceDescriptor[10] = 0x51 + id;               /* idProduct low byte */
if (id != 0) {
    USBDesc_StringSerial[14] = ' ';
    if (id <= 8) USBDesc_StringSerial[15] = '1' + id;         /* " 2".." 9" */
    else { USBDesc_StringSerial[15] = '1';
           USBDesc_StringSerial[16] = '0' + id - 9; }         /* " 10".." 16" */
}
```
So the product string carries the device number (id + 1) when id ≠ 0, and the
PID becomes 0x4051 + id. No terminator is written: the buffer's zero
initialisation provides it. This only works because `USBDesc_StringSerial` is
a 20-byte writable buffer that starts with the 14-character "Launchpad Open".

### `usb_unconnected`
`bDeviceState = 0`. Called from `main` before `init_usbconfig`.

## `usb_endp.o`: `EP1_OUT_Callback`
```
n = (uint16_t)USB_SIL_Read(1, Bulk_Data_Buff);   /* Bulk_Data_Buff: 64-byte COMMON */
if (fifo_unempty(&MIDIToDev)) { SetEPRxStatus(1, 0x2000 NAK); return; }  /* packet dropped */
for (i = 0; i < n; i++) fifo_push1(&MIDIToDev, Bulk_Data_Buff[i]);
/* returns with EP1 RX NAK (hardware); midi_parsers_poll re-arms it */
```

## `usb_core.o` (kept in the archive)
- `Standard_SetInterface`: `r = pProperty->Class_Get_Interface_Setting(wIndex0,
  wValue0)`; if `Current_Configuration != 0 && r == 0 && wIndex1 == 0 &&
  wValue1 == 0` → `pUser_Standard_Requests->User_SetInterface()`, then
  `Current_Interface = wIndex0`, `Current_AlternateSetting = wValue0`, return
  SUCCESS; else UNSUPPORT, and NoData_Setup0 then falls back to
  `Class_NoData_Setup(bRequest)`.
- `Standard_GetInterface(Length)`: `Length == 0` → `Usb_wLength = 1`, return
  NULL; else `User_GetInterface()` then return `&Current_AlternateSetting`.
  GET_INTERFACE is only reached after `Current_Configuration != 0`,
  `wValue == 0`, `wIndex1 == 0` and `Class_Get_Interface_Setting(wIndex0, 0)
  == SUCCESS`. GET_STATUS(interface) uses the same `(wIndex0, 0)` check.
- `Standard_SetConfiguration`: if `wValue0 <= Device_Table.Total_Configuration
  && wValue1 == 0 && wIndex == 0` → `Current_Configuration = wValue0`,
  `User_SetConfiguration()`, SUCCESS.
- `Standard_GetDescriptorData(Length, pDesc)`: `Length == 0` →
  `Usb_wLength = pDesc->Descriptor_Size - Usb_wOffset`, return NULL; else
  return `pDesc->Descriptor + Usb_wOffset`.
- Data_Setup0 (inlined in `Setup0_Process`), GET_DESCRIPTOR: only when
  `(bmRequestType & 0x7F) == 0` (standard, device) are types 1/2/3 sent to
  `GetDeviceDescriptor`/`GetConfigDescriptor`/`GetStringDescriptor`. Any
  other recipient (interface: the GTB request) **and** any other device
  descriptor type fall through to `Class_Data_Setup(bRequest)`. For class
  data setup the core does not install a copy routine itself:
  `Class_Data_Setup` must set `CopyData`, `Usb_wOffset = 0` and call
  `CopyData(0)` to set `Usb_wLength`. The core then stalls on USB_UNSUPPORT
  or `Usb_wLength == 0`, pauses on USB_NOT_READY, and for IN requests clamps
  `Usb_wLength` to `USBwLength` (wLength truncation is done by the core) and
  starts `DataStageIn`, which calls `CopyData(n)` for each packet of up to
  `MaxPacketSize` bytes and advances `Usb_wOffset`.

## `main.o` order
`main`: … `init_device` (→ `device_setuppid`) … `init_midi`,
**`midi_parsers_init`**, `usb_unconnected`, **`init_usbconfig`**
(`Set_USBClock`, `USB_Init`, NVIC for the USB IRQs, `USB_Cable_Config(1)`),
`adc_init`, `pad_setcalibration`, **`app_init`**, then the main loop
(`usbmidi_poll`, `midi_parsers_poll`, `midi_din_poll`, …,
`midi_flush_windows_buffer`). So `midi_parsers_init` runs before USB is
enabled and before `app_init`: a snapshot of `USBParserToDev` taken right after
it is the parser's initial state, and no USB interrupt can run before it.

## `c_midi.o` (kept, wrapped)
Local `.bss`: `USBParserToHst` +0 (8), `USBParserToDev` +0x08 (0x18),
`DataInUSB` +0x20 (0x140), `DinTx` +0x160 (8), `DinRx` +0x168 (0x18),
`DataInDIN` +0x180 (0x140). `midi_parsers_init` fills `USBParserToDev`
(buffer `DataInUSB`, size 320, `MIDIData`, `MIDISysex`, …) and `DinRx`
(`DataInDIN`, 320, port 2, …). `midi_parsers_poll` =
`midi_parse_rx_usb(&MIDIToDev, &USBParserToDev)`; if EP1 RX is NAK and
`MIDIToDev`, `MIDIToHst`, `MIDIDinOut` are empty → `SetEPRxValid(1)`; then
tail-calls `midi_parse_rx_byte_stream(&MIDIDinIn, &DinRx, 2)`. `usbmidi_poll`:
if `bDeviceState == 5` → `usbmidi_tx`. `midi_flush_windows_buffer` =
`midi_sendsysex_usb(WINBUGFLUSHMSG, 6)`. `hal_send_midi`/`hal_send_sysex`:
port ≤ 1 → USB, 2 → DIN, else nothing.

fifos (cx_fifo.o): `{ u8 buf[256]; u8 head @0x100 (pop); u8 tail @0x101
(push); }`, `fifo_flush` zeroes both, `fifo_unempty` = head != tail.
cx_fifo1k.o: two 32-bit indices at +0 and +4 (0x408 bytes in all);
`fifo1k_flush` copies +0 to +4.

## Decisions for the reimplementation (`src/usb/usb_prop.c`)
- Every behaviour above is kept, except the jump to address 0 in
  `Device_Data_Setup`.
- `DeviceDescriptor` stays a writable 18-byte global (device_init.o patches
  byte 10) holding the Task 6 bytes (bcdDevice 0x0300); `Device_Descriptor`
  points at it.
- `USBDesc_StringSerial` stays a writable 20-byte global holding
  "Launchpad Open" (device_init.o writes the suffix at bytes 14–16).
- idx 2 (product) = "Launchpad Pro MIDI 2.0" followed by the device-ID suffix
  copied from `USBDesc_StringSerial + 14` (empty, " 2" … " 16"), at most 25
  characters. The suffix cannot be written into a buffer holding the new
  product name, because `device_setuppid` writes at fixed offsets 14–16.
- idx 3 (serial) = `USBDesc_StringSerial` exactly as before
  ("Launchpad Open[ N]"), so each unit keeps the iSerialNumber the host has
  already seen, and the number stays distinct per device ID.
- `Device_Reset` additionally clears `Current_AlternateSetting` and resets the
  MIDI 2.0 alt state; `Device_SetConfiguration` resets the alt state.
