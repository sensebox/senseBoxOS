# senseBoxOS: platformio edition
Its a ESP32-S2 Pseudocode Interpreter. Serial and BLE are supported. 
- Multiline scripts, control flow, delay(), OLED output, and stoppable LOOP
- Send script lines, execution starts automatically when END_LOOP is received
- Only STOP command needed to halt execution
- Commands: 
    - `display(expr)`
    - `led(on|off|r,g,b)`
    - `delay(ms)`
    - `assignments`
    - `if/else if/else` (brace-aware, indentation fallback)
    - `for(init;cond;step)` (brace-aware, indentation fallback)
    - `while(... )` (indentation fallback)

## Sending Data Back to the Host

Measured values can be sent back to the connected host (via BLE or Serial)
so they can be stored or processed there. An optional identifier is included
so the host can map the value to the right channel/variable.

- `sendBLE(expr, id)` — sends the value tagged with a numeric identifier as an
  8-byte packet `[identifier(float)][value(float)]` (BLE notify).
- `sendSerial(expr, id)` — prints the value over Serial as `DATA:<id>=<value>`.

`expr` can be a sensor reading, a variable, or any arithmetic expression.
The identifier is optional; if omitted, the value is sent on its own (BLE) or
the expression itself is used as the label (Serial).

Example usage:

```
sendBLE(sensor:bme680:temperature, 1)
sendSerial(sensor:hdc1080:temperature, temp)
```

Over Serial this produces a line the host can parse:

```
DATA:temp=23.40
```

### Receiving on the Host

#### BLE

The device exposes one GATT service with a notify characteristic that carries
the returned values:

| Role                     | UUID                               |
| ------------------------ | ---------------------------------- |
| Service                  | `CF06A218F68EE0BEAD048EBC1EB0BC84` |
| RX (host → device write) | `2CDF217435BEFDC44CA26FD173F8B3A8` |
| TX (device → host notify)| `9D8F1B2C3A4D5E6F70819293A4B5C6D7` |

To receive data, **subscribe to notifications** on the TX characteristic.
Each `sendBLE()` call triggers one notification with a raw binary payload:

- `sendBLE(expr, id)` → **8 bytes**: `float identifier` (bytes 0–3) followed by
  `float value` (bytes 4–7).
- `sendBLE(expr)` (no identifier) → **4 bytes**: `float value`.

All floats are **IEEE-754 32-bit, little-endian** (ESP32-S2 byte order).
Decide how to interpret the packet by its **length** (8 vs. 4 bytes).

Web Bluetooth example:

```js
const SERVICE = "cf06a218-f68e-e0be-ad04-8ebc1eb0bc84";
const TX      = "9d8f1b2c-3a4d-5e6f-7081-9293a4b5c6d7";

const device = await navigator.bluetooth.requestDevice({
  filters: [{ services: [SERVICE] }],
});
const server = await device.gatt.connect();
const service = await server.getPrimaryService(SERVICE);
const tx = await service.getCharacteristic(TX);

await tx.startNotifications();
tx.addEventListener("characteristicvaluechanged", (e) => {
  const dv = e.target.value; // DataView
  if (dv.byteLength >= 8) {
    const id = dv.getFloat32(0, true);    // little-endian
    const value = dv.getFloat32(4, true);
    console.log(`channel ${id}: ${value}`);
  } else if (dv.byteLength >= 4) {
    console.log(`value: ${dv.getFloat32(0, true)}`);
  }
});
```

> Note: 128-bit UUIDs must be written in canonical dashed form on the host
> (e.g. `cf06a218-f68e-...`), while the firmware stores them without dashes.

#### Serial

Open the port at **115200 baud**. The firmware prints a lot of debug output,
so the host must **filter for lines beginning with `DATA:`** and ignore the
rest. Each matching line has the form:

```
DATA:<identifier>=<value>
```

The `<identifier>` is the text you passed to `sendSerial(expr, id)` (or the
expression itself if no id was given); `<value>` is a decimal number with two
fractional digits.

Python (pyserial) example:

```python
import serial

with serial.Serial("/dev/ttyACM0", 115200, timeout=1) as ser:
    for raw in ser:
        line = raw.decode(errors="ignore").strip()
        if line.startswith("DATA:"):
            identifier, _, value = line[len("DATA:"):].partition("=")
            print(identifier, float(value))
```

### Things to keep in mind

- **A host must be connected.** If nobody is subscribed/connected, `sendBLE()`
  is a no-op and logs `no host`; the value is not buffered.
- **Distinguish packets by length**, not by content — a 4-byte packet has no
  identifier.
- **Endianness matters**: always decode floats as little-endian.
- **Identifiers over BLE are numbers** (evaluated on the device). Compare them
  with rounding/tolerance if you map channels by integer id.
- **Serial `DATA:` lines are interleaved** with debug logs; never assume the
  next line after a command is your value — always filter by prefix.
- The 20-byte BLE characteristic limit means at most two floats per
  notification; send multiple `sendBLE()` calls for more values.

## Light Sensor Integration

The light sensor is now available as `sensor:board:light`.

Example usage:

```
lightBoard = sensor:board:light
display(lightBoard)
```

This will read the analog value from the light sensor pin (PD_SENSE).
### Example script:
```
temperatureHDC=sensor:hdc1080:temperature
temperatureBME=sensor:bme680:temperature
if(temperatureHDC>25) {
  led(255,0,0)
} 
else {
  led(0,0,0)
}
display(temperatureHDC)
display(temperatureBME)
delay(1000)
```
for-loop example (now brace-aware):
```
for(i=0;i<5;i=i+1) {
led(255,0,0)
delay(200)
led(0,0,0)
delay(200)
}
```

Long lines have to be divided and send in smaller parts when communicating via BLE.
