# senseBoxOS
ESP32-Pseudocde Interpreter Firmware for the senseBox MCU-S2

## Examples

```
temperature = readSensor
if(temperature > 25) {
  led(255,0,0)
} else {
  led(0,0,0)
}
display(temperature)
delay(1000)
```


```
i = 0
for(i = 0; i < 5; i = i + 1) {
  led(255,0,0)
  delay(200)
  led(0,0,0)
  delay(200)
}
```

Upload the firmware to the senseBox MCU-S2, connect to it via Serial (see Pseudocode_gui), copy and paste the example code send to the device. 