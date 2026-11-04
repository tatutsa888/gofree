# How the SCALA1 talks over Bluetooth

Everything here was observed on a real SCALA1 (GENI family 57, type 1) and cross-checked against the behaviour of the official app. It is published for interoperability. GoFree is not affiliated with or endorsed by Grundfos.

## Link

| | |
|---|---|
| GATT service | `0xFE5D` |
| Characteristic | `859cffd1-036e-432a-aa28-1a0085b87ba9` (write + notify) |
| Security | the pump drops any link that is not paired and encrypted, after about one second |
| Connections | one at a time (the official app and GoFree cannot be connected together) |

Requests and replies are GENIbus telegrams. Replies longer than 20 bytes arrive as several notifications: the first one starts with `0x24`, and the second byte gives the length, so the full telegram is `length + 4` bytes long.

## Telegram

```
27 <len> E7 F8 <APDU…> <CRC hi> <CRC lo>      request (start 0x27)
24 <len> F8 E7 <APDU…> <CRC hi> <CRC lo>      reply   (start 0x24)
```

`len` counts the bytes from the destination (`E7`) to the end of the APDU. The CRC is CRC-16/CCITT (polynomial `0x1021`, initial value `0xFFFF`, final value inverted), computed from `len` to the end of the APDU. `tools/geni.py` builds telegrams and checks the CRC.

| APDU | Meaning |
|---|---|
| `0A 03 <id> <sub hi> <sub lo>` | read the class 10 object `id/sub` |
| `03 81 <cmd>` | command: `2` reset alarms, `5` stop, `6` auto |
| `07 01 <id>` | read a text (product name, serial number, versions) |

## Class 10 replies

```
24 len F8 E7 0A <l> 00 <type hi> <type lo> <version> 00 00 <size> <data…> CRC
```

The reply carries the **type** of the object, not the id you asked for. Data starts at byte 13 and is big-endian. Consecutive booleans are packed into the bits of one byte, lowest bit first.

| Request | Type | Size | Content |
|---|---|---|---|
| `91/164` | 420 | 11 | status: 2 bytes of flags (below), mode, speed (float), spare float |
| `88/25` | 266 | 6 | fault status: warning code, alarm code, highest priority code (u16) |
| `93/1` | 248 | 28 | history: starts, starts last hour, starts last 24 h, running time, running time 24 h, last cycle, power-ups, power-on time (seconds) |
| `88/114` | 408 | 15 | dry-run protection: enabled, detection times (priming, normal, flow), auto reset, interval, attempts |
| `82/60` | 413 | 6 | external input: enabled / stops when closed (bits), delay, forced restart, its duration |

Status flags (object 91/164):

| Byte | Bit 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| 1 | available | local stop | stopped by command | external stop | external forced start | external priority | water demand | pressure detected |
| 2 | flow detected | running | priming | primed | system alarm | local alarm | spare | spare |

Observed values: `81 08` idle with pressure, `41 0A` water drawn and pump starting, `C1 0B` running with flow, `85 08` stopped by command.

## Things that may differ on your pump

- Object versions: the SCALA1 sends the 6-byte version of the fault status, other products send a longer one.
- Other Grundfos products use the same transport and pairing, but other objects. Captures and pull requests are welcome.
