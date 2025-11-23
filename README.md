# FOSSEE--Arduino-Based-Tasks
PotMatch is a fast, simple sensor actuator, analog precision game built on Arduino Nano. Match the hidden value using a potentiometer while LEDs react in real time, along with Buzzers and LCD feedback. Features stable input detection, multi-stage feedback logic, timers, tries, and non blocking actuator control.
PotMatch 

PotMatch is a compact sensor–actuator embedded system built around an Arduino Nano.
It uses a potentiometer as an analog input device and provides layered feedback using LEDs, a passive buzzer, and a 16×2 I2C LCD. With the primary simple sensor actuator system being the potentiometer and the LED respectively, with an LCD and buzzer connected to this output. The system evaluates how closely the user matches a hidden analog target value and progressively indicates proximity through a multi-stage feedback pattern. It is a compact, carriable game, ready to provide challenge and fun. 

This project demonstrates real time signal acquisition, stable input detection, non-blocking actuator control, and level based interaction logic in a small, self-contained module.


---

1. Features

Analog Input Handling

Reads a potentiometer (A0) continuously.

Accepts a value only when the input is stable for 500 ms.

Supports a wide input range and removes jitter during knob movement.


Multi-Stage LED Feedback

The system uses six LEDs:

3 red (far)

1 red (mid)

2 green (close)

3 green (exact)


LED patterns update based on the user’s proximity to the hidden target value.

Audio Cues

A passive buzzer generates:

Distinct tones for each “distance band”

A structured, non-blocking win melody on correct match


Display Output

A 16×2 I2C LCD shows:

Level selection

Real-time timer (levels with countdown)

Number of tries (levels with limited attempts)

Text feedback matching the LED states

Win and fail messages


Difficulty Levels (1–5)

Each level modifies:

Time limit

Width of the “winning zone”

Allowed number of attempts

Target value reset per round.


Levels unlock sequentially after successful completion.

Button Interaction

Short presses: cycle through levels

Long press: confirm selected level

Idle long press: reset to welcome screen

Menu and in-game logic are fully non-blocking


Automatic Idle Sleep:

If unused for 36 seconds, the LCD turns off and the system enters a low activity mode.
Pressing the button wakes it.


---

2. Hardware Used

Arduino Nano

10k potentiometer

6× LEDs (3 red, 3 green)

6× 330Ω resistors

Passive piezo buzzer

Push button 

16×2 LCD with I2C backpack

Jumper wires

Breadboard (full-length recommended)

USB power source



---

3. Pin Mapping

Arduino → LEDs

Function	Pin

Red LED 1 (far)	D3
Red LED 2	D4
Red LED 3	D5
Green LED 1	D6
Green LED 2	D7
Green LED 3 (exact)	D8


Arduino → Other Components

Component	Pin

Potentiometer (signal)	A0
Button	D2
Buzzer	D9
LCD SDA	A4
LCD SCL	A5
LCD VCC	5V rail
LCD GND	GND rail



---

4. System Logic Summary

1. User selects a level using button short presses.


2. Long press begins the level.


3. A random target value is generated.


4. Potentiometer input is monitored until stable.


5. System determines proximity state based on difference:

Far → 3 red LEDs + low tone

Mid → 1–2 red LEDs

Close → 1–2 green LEDs

Exact → 3 green LEDs + win melody



6. LCD displays explanatory text for each state.


7. Timer and attempts decrease per rules of the selected level.


8. Win unlocks next level.


9. After inactivity, system enters sleep mode.




---

5. Schematic

A clean PDF schematic is included in this repository.


---

6. How to Run

1. Connect all components following the schematic.


2. Power the Arduino through USB.


3. Upload the sketch using Arduino IDE.


4. Turn the potentiometer and watch LEDs, buzzer, and LCD respond.


5. Select levels and demonstrate different difficulty behaviors.




---

7. Applications / Learning Outcomes

PotMatch is intentionally simple in hardware, but demonstrates several important microcontroller concepts:

Analog signal sampling

Stable input detection without blocking

Time-based event handling

Multimodal feedback generation

Non-blocking state-machine design

Sensor–actuator interaction patterns

Menu navigation with a single button

Level-based embedded game logic



---

8. Notes

LCD address defaults to 0x27. If the display shows nothing, try 0x3F.

Win window sizes differ per level for practical usability.

All tones and animations run non-blockingly, keeping the system responsive.



---

9. Credits

Designed and implemented as part of a sensor-actuator demonstration module.
All wiring, code, and diagrams were prepared specifically for this submission.
