# Serial protocol v1

Binary, little-endian, sent from the PC bridge to the display over USB CDC
serial (baud rate is irrelevant for native USB; 115200 for UART boards).

## Frame

| Offset | Size | Field   | Notes                                       |
|--------|------|---------|---------------------------------------------|
| 0      | 1    | SOF     | `0xA5`                                      |
| 1      | 1    | type    | message type                                |
| 2      | 1    | len     | payload length, max 64                      |
| 3      | len  | payload |                                             |
| 3+len  | 2    | crc     | CRC-16/CCITT-FALSE over `type, len, payload`|

The receiver parses byte by byte. A bad CRC or oversize length drops the
frame and the parser hunts for the next `0xA5`, so a cable glitch never
locks up the display.

## Messages

### `0x01` HELLO — 1 byte

| Field            | Type |
|------------------|------|
| protocol_version | u8 (= 1) |

Sent once on connect and every few seconds.

### `0x10` TELEMETRY — 38 bytes, sent at 20–30 Hz

| Field          | Type | Unit / meaning                                     |
|----------------|------|----------------------------------------------------|
| current_lap_ms | u32  | running lap time                                   |
| last_lap_ms    | u32  | 0 = none                                           |
| best_lap_ms    | u32  | 0 = none                                           |
| last_pit_ms    | u32  | duration of last pit stop, 0 = none                |
| delta_ms       | i32  | current vs best, negative = faster                 |
| lap_number     | u16  |                                                    |
| position       | u8   | 0 = unknown                                        |
| gear           | i8   | -1 = R, 0 = N, 1.. = gears                          |
| speed_kmh      | u16  |                                                    |
| fuel_laps_x10  | u16  | fuel remaining in laps × 10, `0xFFFF` = unknown    |
| track_pos      | u16  | 0..10000 = normalized position on lap              |
| air_temp_x10   | i16  | °C × 10                                            |
| track_temp_x10 | i16  | °C × 10                                            |
| flags          | u16  | see below                                          |
| session        | u8   | 0 unknown, 1 practice, 2 qualifying, 3 race, 4 hotlap |
| race_flag      | u8   | 0 none, 1 green, 2 yellow, 3 blue, 4 white, 5 black, 6 chequered, 7 red, 8 safety car, 9 virtual safety car / full course yellow |

`race_flag` was appended after the first draft; 37-byte payloads without it
are still accepted and decode as `none`.

Flags: bit0 pit limiter, bit1 in pit lane, bit2 lap invalid, bit3 engine
running, bit4 ignition on. Other bits reserved (send 0).

### `0x11` TRACK_INFO — 1..48 bytes

UTF-8 track name, no terminator. Sent on change and every few seconds.

### Slower messages

Sent at their own rate by the bridge scheduler. All little-endian; "n/a"
values mean the game doesn't provide it.

**`0x12` ENGINE — 6 bytes, every frame:** `rpm u16`, `shift_rpm u16`
(0 = unknown, no shift lights), `max_rpm u16`.

**`0x13` TYRES — 35 bytes, 5 Hz:** 4 × wheel (FL, FR, RL, RR) of
`temp_x10 i16` (°C × 10), `pressure_psi_x100 u16`, `wear_pct u8`
(100 = new), `brake_temp_c i16`; then `opt_min_x10 i16`, `opt_max_x10 i16`
(compound's optimal window), `target_psi_x100 u16`, `compound u8`
(0 unknown, 1 soft, 2 medium, 3 hard, 4 inter, 5 wet).

**`0x14` CAR — 7 bytes, 5 Hz:** `abs u8`, `tc u8`, `tc_cut u8`,
`engine_map u8` (each `0xFF` = n/a), `brake_bias_x10 u16` (% front × 10,
0 = n/a), `damage_pct u8`.

**`0x15` FUEL — 8 bytes, 2 Hz:** `liters_x10 u16`, `per_lap_x100 u16`
(0 = not known yet), `capacity_x10 u16`, `to_finish_x10 i16` (extra liters
needed to finish incl. half a lap margin; ≤ 0 = enough; `0x7FFF` = unknown).

**`0x16` RACE — 21 bytes, 5 Hz:** `gap_ahead_ms i32`, `gap_behind_ms i32`
(`0x7FFFFFFF` = unknown), `time_left_s u32` (`0xFFFFFFFF` = n/a),
`laps_left u16` (`0xFFFF` = n/a), `car_count u8`, `penalty u8` (0 none,
1 drive through, 2 stop & go, 3 time penalty, 4 disqualified),
`penalty_s u8`, `track_limits u8`, `track_limits_max u8` (0 = unknown),
`pit_window u8` (0 n/a, 1 closed, 2 open), `bits u8` (bit0 mandatory stop
done, bit1 last lap).

**`0x17` TIMING — 36 bytes, on change / 1 Hz:** `sector u8` (0..2),
`sector_ms u32 ×3` (current lap, 0 = not driven yet), `best_sector_ms u32 ×3`,
`sector_state u8 ×3` (0 none, 1 slower, 2 personal best, 3 session best),
`optimal_lap_ms u32`, `session_best_ms u32`.

**`0x18` WEATHER — 9 bytes, 1 Hz:** `rain u8`, `rain_in_10 u8`,
`rain_in_30 u8`, `wetness u8` (levels 0 dry, 1 drizzle, 2 light, 3 medium,
4 heavy, 5 storm), `grip_pct u8`, `wind_kmh u8`, `wind_dir_deg u16`,
`clouds_pct u8`.

**`0x19` LAP_HISTORY — 22 bytes, on change:** `count u8` (≤ 5),
`lap_ms u32 ×5` (newest first), `valid_mask u8` (bit i = lap i valid).

**`0x20` CONFIG — 4 bytes, on connect / every few seconds:**
`page_mask u16` (bit i = page i: current, delta, tyres, car, race, weather,
track, laps), `brightness u8`, `units u8` (bit0 mph, bit1 °F, bit2 bar).

The bridge computes fuel per lap, fuel to finish, sector times / colours and
lap history itself when a game doesn't provide them (`rigdash.derived`).

## Compatibility

Unknown message types are ignored. New fields are only ever appended to a
payload; receivers accept payloads longer than they know. Breaking changes
bump `protocol_version`.

Reference frames: [`protocol/vectors.txt`](../protocol/vectors.txt)
(generated by `python -m rigdash.vectors`).
