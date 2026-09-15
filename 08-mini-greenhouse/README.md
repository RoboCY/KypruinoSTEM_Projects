# Smart Mini Greenhouse

A desktop greenhouse that waters and ventilates itself. An AHT10 sensor checks temperature and air humidity every five minutes: if it gets too hot or too humid, a fan switches on for ventilation (with hysteresis so it does not chatter); if humidity drops too low, a micro submersible pump in the front water tank runs a short burst to top up the water channel. Fan and pump are switched by N-channel MOSFETs, never straight off an I/O pin. No soil moisture sensor needed.

<p>
  <img src="images/mini-greenhouse-1.jpeg" width="360" alt="Mini greenhouse assembled">
  <img src="images/mini-greenhouse-2.jpeg" width="360" alt="Mini greenhouse open with electronics">
</p>

More photos in [images/](images/).

## Build

| | |
|---|---|
| **Code** | [`code/MiniGreenhouse/MiniGreenhouse.ino`](code/MiniGreenhouse/MiniGreenhouse.ino) |
| **3D parts** | [`stl/mini-greenhouse.stl`](stl/mini-greenhouse.stl) |
| **Hardware** | Kypruino, AHT10 temp/humidity sensor, 5 V DC brushless fan (LD3007MS-style), micro submersible 5 V pump + tubing, 2× P16NF06L logic-level N-channel MOSFETs, 2× 220 Ω resistors, 2× 10 kΩ resistors, small breadboard, jumper wires, USB-C cable |
| **Wiring** | AHT10 → I2C (VIN/GND/SDA/SCL) · Pump MOSFET gate → D10 · Fan MOSFET gate → D11<br>Each gate: D-pin through a 220 Ω resistor, plus a 10 kΩ pull-down to GND · MOSFET source → GND · drain → fan/pump negative · fan/pump positive → 5 V<br>**Never drive the fan or pump directly from an I/O pin — the pins only switch the MOSFET gates. All grounds must be common.** D8 and D9 are left free for the onboard NeoPixels and buzzer. |
| **Libraries** | Adafruit AHTX0, Adafruit BusIO, Adafruit Unified Sensor |
| **Config** | `CHECK_INTERVAL_MS` (5 min), `PUMP_TIME_MS = 1500`, `HUMIDITY_WATER_THRESHOLD = 55.0`, `TEMP_FAN_ON = 30.0`, `TEMP_FAN_OFF = 28.0`, `HUMIDITY_FAN_ON = 85.0`, `HUMIDITY_FAN_OFF = 80.0` |
| **Print** | Greenhouse body with plant compartments, front water tank, fan opening, overflow channel and transparent cover |
| **Guide** | [Read the full build](https://robo.com.cy/blogs/blog/kypruino-mini-smart-greenhouse) |

P16NF06L pinout, flat labelled side facing you and pins down: 1 = gate, 2 = drain, 3 = source. The metal tab is also drain.

Open the Serial Monitor at 9600 baud to watch the readings. If the sketch prints "Could not find AHT10 sensor", it stops there on purpose: check the I2C wiring and reset the board. Start with the 1.5 s pump burst and shorten or lengthen `PUMP_TIME_MS` to suit your tank and tubing, then tune `HUMIDITY_WATER_THRESHOLD` after a day inside the actual greenhouse.

Keep the board, breadboard and MOSFETs away from the water tank and tubing, and check for leaks before leaving the project powered.
