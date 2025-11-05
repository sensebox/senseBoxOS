# senseBoxOS: platformio edition
Its a ESP32-S2 Pseudocode Interpreter. Currently only serial connection is supported. 
- Multiline scripts, control flow, delay(), OLED output, and stoppable RUNLOOP
- Send lines over Serial, then send RUN (once) or RUNLOOP (repeat) or STOP (to halt)
- Commands: 
    - `display(expr)`
    - `led(on|off|r,g,b)`
    - `delay(ms)`
    - `assignments`
    - `if/else if/else` (brace-aware, indentation fallback)
    - `for(init;cond;step)` (brace-aware, indentation fallback)
    - `while(... )` (indentation fallback)

### Example script:
```
temperatureHDC = sensor:hdc1080:temperature
temperatureBME = sensor:bme680:temperature
if(temperatureHDC > 25) {
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
i = 0
for(i = 0; i < 5; i = i + 1) {
  led(255,0,0)
  delay(200)
  led(0,0,0)
  delay(200)
}
```
