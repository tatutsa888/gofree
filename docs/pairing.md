# Pairing, explained

The SCALA1 does not pair the usual way. It took a few hours to find out why a standard Bluetooth pairing request always fails, so here is what actually happens.

## What the pump advertises

The pump advertises service data `0xFE5D`:

```
03 01 00 <family> <type> <version> <mode> <flags> <id hi> <id lo>
```

`family 0x39 / type 0x01` is a SCALA1. The id is two bytes, shown as four hex characters (for example `2F3F`). The flags byte tells you what the pump is doing:

| Flag | Meaning |
|---|---|
| `0x01` | a client is connected |
| `0x02` | alarm |
| `0x04` | warning |
| `0x08` | the LED is blinking (identify / pairing request received) |
| `0x10` | the blue button was just pressed: pairing is accepted for about 3 seconds |

## The four steps

1. **Ask.** The client advertises manufacturer data with the Grundfos company id `0x0328` and the ASCII text `GF` + pump id + `02` (for example `GF2F3F02`). The pump's LED starts blinking (flag `0x08`).
2. **Accept.** Someone presses the blue button. Flag `0x10` appears for about 3 seconds.
3. **Pair.** The client connects during that window. The pump starts pairing on its own. The client must *not* request encryption at the same time, or both requests collide and pairing fails. Outside that window, any pairing attempt is rejected ("pairing not supported").
4. **Reconnect.** From then on, the client must request encryption as soon as it connects, using the stored key. Without it, the pump hangs up after about a second.

GoFree does all of this automatically: it targets the nearest pump, waits for the button, pairs, remembers the pump across reboots and stops advertising the request once paired.

## Re-pairing

Press **Forget pump** in Home Assistant, then the blue button on the pump. Use it after replacing the ESP32, or if the pump was reset.
