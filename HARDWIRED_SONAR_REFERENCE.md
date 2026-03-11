# Hardwired Sonar Reference

Last updated: 11-03-2026

This file collects the known reference data for the hardwired underwater ultrasonic sensor used by `esp32_sonar_bridge`.

Important:
- This reference is being built incrementally from user-provided text and images.
- RS-485 variants are intentionally ignored here.
- UART-only data is kept.
- The exact final model still needs confirmation from the sensor label and/or datasheet.

## Known Product Family

- Serial No.: `GL04`
- Product Name: `Submarine Underwater Ultrasonic Sensor Module`
- Housing protection: `IP68`
- Intended working media: `Clean Water`
- Max underwater depth: `10 m`
- Cable length: `1 m`
- Housing color: `Grey`
- Ambient temperature: `-15 C to 55 C`
- Baud rate: up to `115200 bps` and adjustable

## UART Models From The Listing

The listing shows two UART-controlled models:

| Model | Range | Blind Zone | Output | Response Time | Working Voltage | Detection Angle |
|---|---:|---:|---|---:|---|---|
| `GL041MT` | `5-600 cm` | `5 cm` | `UART Ctrl` | `14 ms` | `5-24 V DC` | `8-16 deg` |
| `GL042MT` | `2-300 cm` | `2 cm` | `UART Ctrl` | `10 ms` | `5-24 V DC` | `5-10 deg` |

Common accuracy rule shown in the listing:

- if `S < 200` then accuracy is `+- (0.5 + S * 0.5%)`
- if `S >= 200` then accuracy is `+- (1 + S * 1%)`
- `S` = measured distance

## Listing Remarks

- The maximum detectable distance (`300 cm` / `600 cm`) is based on a large flat object.
- Detectable distance is shorter for non-flat objects.
- Output response time is based on `115200 bps`.
- Accuracy was estimated with the sensor working `30 cm` below a static water surface, at `25 C`, with no water flow.
- The listing refers to a later `Section 6` for more detail, but that section has not been captured yet.

## Current Wiring Clues

From the previously extracted product photo label and the later wire table:

- Red: `VCC` / `V+`
- Black: `GND` / `V-`
- Yellow: `RX`
- White: `TX`

This wire-color mapping was visible on a product label image reading:

- `Red V+`
- `Black V-`
- `White TX`
- `Yellow RX`

The later image gives the same mapping more explicitly:

| Color | No. | Name | UART Meaning |
|---|---:|---|---|
| Red | `1` | `VCC` | connect to positive supply |
| Black | `2` | `GND` | connect to negative supply / ground |
| Yellow | `3` | `RX / B` | for UART: connect to controller `TX` |
| White | `4` | `TX / A` | for UART: connect to controller `RX` |

## Mechanical / Mounting Data

From the user-provided dimension drawing:

- Unit: `mm`
- Cable length drawing callout: `1000 +- 30`
- Cable spec callout: `26 AWG * 4`
- Cable outer diameter: `4 +- 0.2 mm`
- Exposed wire lead length: `30 +- 5 mm`
- Individual lead diameter/spacing callout near wire end: `3-5 mm`

Main body dimensions visible on the drawing:

- Front housing outer diameter: `28 +- 0.5 mm`
- Front sensing face diameter: `19 +- 0.2 mm`
- Front projection length: `11.2 +- 0.3 mm`
- Thread size: `M19 x 1.5`
- Rear threaded section outer diameter callout: `26 +- 0.3 mm`
- Overall body depth shown in the drawing: `34 +- 0.5 mm`
- Additional body step dimensions shown:
  - `5 +- 0.3 mm`
  - `1.5 +- 0.2 mm`
  - another rear section `5 +- 0.3 mm`

Mounting notes from the drawing:

- Installation hole size: `19.5 +0.2 mm`
- Supported panel / wall thickness `T`: `1-5 mm`
- Drawing labels show:
  - waterproof ring
  - tighten nut

Practical mounting interpretation:

- The sensor is designed to pass through a round panel hole and clamp in place with the supplied sealing ring and nut.
- The panel opening should be approximately `19.5 mm`.
- The panel thickness should stay within `1-5 mm`.
- The sensing face should remain unobstructed and aligned with the intended measurement direction.

## ESP32 Integration Notes

For the current project firmware, the expected ESP32-side default pins are:

- ESP `GPIO17` -> sonar `RX` / trigger input
- sonar `TX` -> ESP `GPIO16`
- common ground required

Practical mapping using the listing wire colors:

- sonar red -> sensor supply positive
- sonar black -> ESP ground / supply ground
- sonar white (`TX`) -> ESP `GPIO16` (`RX`)
- sonar yellow (`RX`) -> ESP `GPIO17` (`TX`)

## UART Protocol Basics

UART-controlled output data now confirmed from the user-provided text:

- Baud rate: `115200 bps`
- Data bits: `8`
- Stop bits: `1`
- Parity / verification: `None`

That means the current default serial format should be treated as:

- `115200 8N1`

Trigger behavior:

- The module measures once when the trigger input pin `RX` receives:
  - a trigger pulse with a falling edge, or
  - any serial-port data
- After one trigger event, the output pin `TX` emits one measurement data packet

Minimum trigger period:

| Model | Minimum trigger cycle `T1` |
|---|---|
| `GL041MT` | greater than `19 ms` |
| `GL042MT` | greater than `14 ms` |

Practical firmware implication:

- The ESP32 should not trigger the sensor faster than the model-specific minimum cycle.
- For the likely `GL041MT` path, a safe initial trigger interval is `>= 19 ms`.
- Sending any UART byte may be sufficient to trigger one measurement.
- The vendor PC-tool screenshot strongly suggests that sending a single byte `0xFF` is a valid trigger in practice.

Timing-diagram values from the later image:

| Model | `T1` minimum trigger cycle | `T2` approximate output latency |
|---|---|---|
| `GL041MT` | `>= 19 ms` | `~ 13 ms` |
| `GL042MT` | `>= 14 ms` | `~ 9 ms` |

Interpretation:

- `T1` is the minimum interval between trigger events.
- `T2` is the approximate time from trigger to UART output.

## UART Output Frame

The later protocol image confirms that one measurement is returned as a fixed `4-byte` UART frame:

| Byte Order | Field | Meaning |
|---:|---|---|
| `1` | `0xFF` | frame header |
| `2` | `Data_H` | high 8 bits of distance |
| `3` | `Data_L` | low 8 bits of distance |
| `4` | `SUM` | communication checksum |

Distance decoding:

- `distance_mm = Data_H * 256 + Data_L`

Checksum rule:

- `SUM = (0xFF + Data_H + Data_L) & 0x00FF`
- Only the lower 8 bits of the accumulated value are kept.

Worked example from the image:

| Field | Value |
|---|---|
| Header | `0xFF` |
| `Data_H` | `0x07` |
| `Data_L` | `0xA1` |
| `SUM` | `0xA7` |

Example calculation:

- `SUM = (0xFF + 0x07 + 0xA1) & 0x00FF = 0xA7`
- `distance_mm = 0x07 * 256 + 0xA1 = 0x07A1 = 1953 mm`

Additional worked example from the later PC-tool screenshot:

| Field | Value |
|---|---|
| Header | `0xFF` |
| `Data_H` | `0x07` |
| `Data_L` | `0x5F` |
| `SUM` | `0x65` |

Example calculation:

- `SUM = (0xFF + 0x07 + 0x5F) & 0x00FF = 0x65`
- `distance_mm = 0x07 * 256 + 0x5F = 0x075F = 1887 mm`

Practical parser rule:

- accept a frame only if:
  - byte `0` is `0xFF`
  - byte `3` equals `(byte0 + byte1 + byte2) & 0xFF`

Practical firmware implication:

- The sensor's documented UART response format is `4 bytes`, not an arbitrary text/NMEA payload.
- Any project code expecting a different binary frame length should be re-checked against this reference.

## UART Model Modbus Access Window

This section is relevant to the UART models `GL041MT` and `GL042MT` only.

- `GL041MT` and `GL042MT` support Modbus communication only within `500 ms` after power-on.
- The vendor recommendation for register access is to repeat the command every `100 ms` before power-on.
- The command interval must be longer than the addressed register's response time so the full response frame can be received.

Interpretation:

- This looks like a short boot-time configuration window for UART models.
- It may matter later if we want to change sensor settings such as baud rate or other registers.
- It does not change the normal measurement-mode UART frame described above.

Communication parameters inside this short UART-model Modbus window:

- Data bits: `8`
- Stop bit: `1`
- Parity: `None`
- Default baud rate: `115200 bps`
- Protocol: `Modbus RTU`
- Verification: `CRC-16/Modbus`
- Default sensor address: `0x01`
- Read function code: `0x03`
- Write function code: `0x06`

Observed Modbus frame layout from the examples:

- Read request:
  - `SlaveAddr Function RegHi RegLo CountHi CountLo CRC16Lo CRC16Hi`
- Read response:
  - `SlaveAddr Function ByteCount Data... CRC16Lo CRC16Hi`
- Write request:
  - `SlaveAddr Function RegHi RegLo DataHi DataLo CRC16Lo CRC16Hi`
- Write response:
  - echoes the write request payload

## UART-Model Register Map

These registers are relevant because the UART models expose them during the short post-power-on Modbus window.

| Address | Name | Access | Data Type | Meaning / Notes |
|---|---|---|---|---|
| `0x0100` | Processed Value | Read only | unsigned 16-bit | processed distance in `mm`; response time `101-121 ms` |
| `0x0101` | Real-Time Value | Read only | unsigned 16-bit | single-measurement distance in `mm`; response time `22-26 ms` |
| `0x0102` | Temperature | Read only | signed 16-bit | unit `0.1 C`; resolution `0.1 C`; response time about `15 ms` |
| `0x0200` | Slave Address | Read/write | unsigned 16-bit | range `0x01-0xFE`; default `0x01`; `0xFF` is broadcast address |
| `0x0201` | Baud Rate | Read/write | unsigned 16-bit | default `0x09` = `115200 bps` |
| `0x0208` | Angle Grade | Read/write | unsigned 16-bit | default `0x02`; range `0x01-0x04`; higher grade = wider angle |

Baud-rate register `0x0201` mapping from the provided table:

| Register Value | Baud Rate |
|---|---:|
| `0x01` | `2400` |
| `0x02` | `4800` |
| `0x03` | `9600` |
| `0x04` | `14400` |
| `0x05` | `19200` |
| `0x06` | `38400` |
| `0x07` | `57600` |
| `0x08` | `76800` |
| `0x09` | `115200` |

## UART-Model Modbus Examples

These examples are relevant to the UART models only during the short post-power-on Modbus window.

Read processed value:

- Host TX: `01 03 01 00 00 01 85 F6`
- Sensor RX: `01 03 02 02 F2 38 A1`
- Decoded value: `0x02F2` = `754 mm`

Read real-time value:

- Host TX: `01 03 01 01 00 01 D4 36`
- Sensor RX: `01 03 02 02 EF F8 A8`
- Decoded value: `0x02EF` = `751 mm`

Read temperature:

- Host TX: `01 03 01 02 00 01 24 36`
- Sensor RX: `01 03 02 01 2C B8 09`
- Decoded value: `0x012C` = `30.0 C`
- Vendor note: this temperature comes from the internal circuit of the sensor module, not the surrounding air/water, and is used as a reference for distance calculation.

Change sensor / slave address:

- Host TX: `01 06 02 00 00 05 48 71`
- Sensor RX: `01 06 02 00 00 05 48 71`
- Meaning: address changed from `0x01` to `0x05`

Look up the current address using broadcast `0xFF` when only one sensor is connected:

- Host TX: `FF 03 02 00 00 01 90 6C`
- Sensor RX: `01 03 02 00 01 79 84`
- Meaning: actual sensor address is `0x01`

Read current baud rate:

- Host TX: `01 03 02 01 00 01 D4 72`
- Sensor RX: `01 03 02 00 09 F8 45`
- Meaning: `0x0009` = `115200 bps`

Set baud rate to `2400 bps`:

- Host TX: `01 06 02 01 00 01 99 B3`
- Sensor RX: `01 06 02 01 00 01 99 B3`

Read angle grade for UART-controlled output:

- Full command confirmed by the later screenshot: `01 03 02 08 00 01 04 70`
- Vendor procedure:
  - set repeated sending every `200 ms`
  - start sending before power-on
  - power on the sensor
  - wait for the reply
- Full reply confirmed by the later screenshot: `01 03 02 00 02 39 85`
- Meaning: angle grade = `0x02`
- This confirms the register is intended to be accessed inside the UART model's short boot-time Modbus window.

## Laboratory Detection Range Reference

The listing provides a non-contractual laboratory reference for how angle grade affects detection range.

Test setup from the vendor notes:

- Target object under water: PVC tube
- Tube diameter: `7.5 cm`
- Tube height: `100 cm`
- Sensor position: `30 cm` below static water surface
- Water temperature: `25 C`
- Water flow: none
- The plotted curves represent the path of the PVC tube center

Angle-grade color legend used in the lab graph:

| Color | Angle Grade |
|---|---:|
| Red | `4` |
| Green | `3` |
| Blue | `2` |
| Purple | `1` |

Vendor notes from this section:

- At a distance of `70 cm` in front of the sensor, the detection angle is:
  - `8-16 deg` for `GL041MT` and `GL041M4`
  - `5-10 deg` for `GL042MT` and `GL042M4`
- At angle grade `2` and `1`, the farthest detectable distance is:
  - angle grade `2` -> `160 cm`
  - angle grade `1` -> `120 cm`
- The vendor says these curves are laboratory reference data only and not contractual performance guarantees.
- The vendor explicitly recommends keeping enough margin in real applications.

Practical interpretation for the UART models:

- Angle grade changes the beam width / detection envelope.
- Higher angle grade means a wider detection angle.
- Narrower angle grades appear to reduce the maximum practical detection distance for the reference target.
- The default angle grade remains `0x02` unless changed through the boot-window Modbus procedure.

## Arduino Donor Comparison

The Arduino donor was reviewed against the collected UART reference:

- donor file: `donors/arduino_sonar.ino`
- likely origin: earlier custom Arduino/NMEA bridge experiment, not a vendor datasheet implementation

### What matches the collected reference

- UART speed matches:
  - donor uses `115200`
  - seller docs confirm `115200 8N1`
- A `0xFF` header is expected on receive in the donor.
- The donor converts the binary distance into `$SDDBT` NMEA for downstream use.

### What does not match the collected reference

- Trigger byte mismatch:
  - donor sends `0x55`
  - seller docs say any serial data can trigger the UART model
  - vendor PC tool strongly suggests `0xFF` is a valid trigger byte in practice
- Frame length mismatch:
  - donor reads one `0xFF` header byte and then reads `4` more bytes
  - that means donor expects `5 total bytes`
  - seller docs and vendor screenshots show `4 total bytes`:
    - `FF Data_H Data_L SUM`
- Distance-field mismatch:
  - donor calculates distance from the second and third bytes after the header
  - seller docs say the distance is simply `Data_H * 256 + Data_L` immediately after the header
- Checksum mismatch:
  - donor checksum logic uses the three bytes after the header
  - seller docs say checksum is:
    - `(0xFF + Data_H + Data_L) & 0xFF`
  - meaning the documented checksum explicitly includes the header byte

### Practical conclusion

- The Arduino donor does **not** cleanly follow the vendor UART frame format collected for the `GL041MT` / `GL042MT` family.
- It may still work with some closely related sonar variant, or with a slightly different firmware revision, but it should not be treated as authoritative for this sensor.
- The current ESP32 hardwired implementation appears to have inherited the donor's assumptions.

### Current ESP32 implication

Current `firmware/main/danevi_sonar.c` still mirrors the donor-style behavior:

- sends trigger `0x55`
- reads `5` bytes total
- checks checksum as sum of bytes `1..3`
- decodes distance from bytes `1..2`

Based on the collected seller documentation, the more likely correct behavior for this UART sensor is:

- trigger with a simple UART byte, with `0xFF` the strongest known candidate
- read a `4-byte` frame:
  - `FF Data_H Data_L SUM`
- validate:
  - `SUM == (0xFF + Data_H + Data_L) & 0xFF`
- decode:
  - `distance_mm = Data_H * 256 + Data_L`

### Next firmware review target

Before the next hardwired-sonar flash/test, re-check `firmware/main/danevi_sonar.c` against this reference and be prepared to change:

- trigger byte
- expected frame length
- checksum formula
- distance-byte positions

## Open Questions

- Exact sensor model still needs confirmation:
  - the listing family includes both `GL041MT` and `GL042MT`
  - we should confirm which one is physically on the desk
- UART electrical level still not confirmed:
  - whether serial lines are `3.3 V TTL` or `5 V TTL`
- UART framing and command protocol still missing:
  - serial format is now confirmed as `115200 8N1`
  - `0xFF` now looks like a vendor-used trigger byte, but we still do not know whether it is required or just one acceptable trigger value
  - exact trigger pulse timing details beyond the published `T1/T2`
  - whether every byte triggers equally or whether the vendor recommends a specific trigger byte
  - update rate behavior during continuous triggering
- Donor mismatch still unresolved:
  - we have not yet proven whether the physical unit on the desk behaves like the vendor docs or like the older Arduino donor assumptions
- Modbus-side details still partially open:
  - we have not yet verified whether these Modbus commands work exactly as documented on the physical UART unit on the desk
- Power/current draw still missing
- Mounting/orientation details still incomplete

## Source Batch 1

This file currently contains data ingested from:

- user-provided listing text on `11-03-2026`
- one user-provided listing table image on `11-03-2026`

## Source Batch 2

- one user-provided mechanical / installation drawing image on `11-03-2026`

## Source Batch 3

- user-provided UART output description on `11-03-2026`
- one user-provided wire-definition image on `11-03-2026`

## Source Batch 4

- one user-provided UART timing-diagram image on `11-03-2026`
- one user-provided UART frame-format image on `11-03-2026`

## Source Batch 5

- user-provided note about UART-model Modbus access within `500 ms` after power-on on `11-03-2026`
- one user-provided PC-tool screenshot showing UART trigger `ff` and returned frame `FF 07 5F 65` on `11-03-2026`

## Source Batch 6

- user-provided Modbus examples and register descriptions relevant to UART models on `11-03-2026`
- one user-provided Modbus function/format image on `11-03-2026`
- one user-provided register-map image on `11-03-2026`

## Source Batch 7

- user-provided Section 6 laboratory detection-range notes on `11-03-2026`
- one user-provided screenshot confirming the full angle-grade command and reply with CRC on `11-03-2026`
- one user-provided laboratory detection-range graph image on `11-03-2026`

## Source Batch 8

- donor comparison against `donors/arduino_sonar.ino` completed on `11-03-2026`
