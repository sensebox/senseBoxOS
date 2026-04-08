# senseBoxOS: platformio edition
Its a ESP32-S2 Pseudocode Interpreter. Serial and BLE are supported. 
- Multiline scripts, control flow, delay(), OLED output, and stoppable LOOP
- Send lines over Serial, then send RUN (once) or LOOP (repeat) or STOP (to halt)
- Commands: 
    - `display(expr)`
    - `led(on|off|r,g,b)`
    - `delay(ms)`
    - `assignments`
    - `if/else if/else` (brace-aware, indentation fallback)
    - `for(init;cond;step)` (brace-aware, indentation fallback)
    - `while(... )` (indentation fallback)
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
