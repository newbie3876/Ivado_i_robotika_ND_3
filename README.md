# Arduino HW3 — Interrupt-Driven LED Controller and Relay

An Arduino Uno R3 project demonstrating concurrent embedded-system tasks, hardware interrupts, EEPROM persistence, direct GPIO register access, and relay control.

The circuit was designed in **Autodesk Tinkercad Circuits** and is intended for implementation using an Arduino Starter Kit.

## Features

- **8-LED running light** controlled by a 74HC595 shift register.
- **Adjustable animation speed** using a potentiometer.
- **Direction control** using a pushbutton and an external interrupt.
- **Relay switching** every 5 seconds using a TIP120 transistor.
- **Timer1 hardware interrupt** providing a 1 ms system tick.
- **EEPROM persistence** of the selected LED animation direction.
- **Direct GPIO register access (MMIO)** for communicating with the shift register.
- **Non-blocking scheduling** without `delay()`.

## Hardware

### Components

| Component | Quantity |
|---|---:|
| Arduino Uno R3 | 1 |
| 74HC595 shift register | 1 |
| Red LEDs | 8 |
| 1 kOhm LED resistors | 8 |
| 10 kOhm potentiometer | 1 |
| Pushbutton | 1 |
| TIP120 Darlington transistor | 1 |
| 1 kOhm transistor base resistor | 1 |
| 5 V SPDT relay | 1 |
| Flyback diode | 1 |
| Breadboard and jumper wires | As needed |

### Arduino Pin Mapping

| Arduino Pin | Function |
|---|---|
| D2 | Pushbutton (external interrupt INT0) |
| D4 | 74HC595 data (DS) |
| D7 | 74HC595 latch (ST_CP) |
| D8 | 74HC595 clock (SH_CP) |
| D9 | Relay transistor control |
| A0 | Potentiometer input |
| 5V | Circuit power |
| GND | Common ground |

### Shift Register Connections

The 74HC595 controls eight LEDs using only three Arduino GPIO pins.

| 74HC595 Pin | Connection |
|---|---|
| 14 (DS) | Arduino D4 |
| 12 (ST_CP) | Arduino D7 |
| 11 (SH_CP) | Arduino D8 |
| 16 (VCC) | 5 V |
| 8 (GND) | GND |
| 10 (MR) | 5 V |
| 13 (OE) | GND |
| QA–QH | LEDs through current-limiting resistors |

### Relay Driver

The Arduino D9 pin controls the TIP120 transistor through a 1 kOhm base resistor.

- TIP120 base (B): Arduino D9 through 1 kOhm.
- TIP120 collector (C): one relay coil terminal.
- TIP120 emitter (E): GND.
- Other relay coil terminal: +5 V.
- Flyback diode: connected across the relay coil, with its cathode (striped end) toward +5 V.

For the **Tinkercad LU-5-R relay**, the identified coil connections are:

- Terminal 5: +5 V.
- Terminal 8: TIP120 collector.

The relay switching contacts are left unconnected because the project demonstrates relay actuation without an external load.

**Note:** Physical relay pinouts must be verified against the specific component's datasheet. Tinkercad terminal numbers must not be assumed to match those of another relay model.

## Software Architecture

The application uses an interrupt-driven time base and a non-blocking main loop.

### 1. Timer1 Hardware Interrupt

Timer1 operates in Clear Timer on Compare Match (CTC) mode.

Configuration:

- CPU frequency: 16 MHz
- Prescaler: 64
- Compare register: `OCR1A = 249`
- Interrupt period: 1 ms

The timer interrupt increments a shared millisecond counter.

The main loop uses this counter to schedule independent tasks.

### 2. External Interrupt

The pushbutton is connected to Arduino D2 using `INPUT_PULLUP`.

A falling-edge interrupt sets a shared event flag.

The main loop processes the event, applies a 200 ms debounce interval, and reverses the LED animation direction.

The interrupt service routine is intentionally short.

### 3. EEPROM

The selected animation direction is stored in EEPROM address 0.

The program reads this value during startup and restores the previously saved direction.

`EEPROM.update()` is used to avoid unnecessary writes when the stored value is unchanged.

### 4. Memory-Mapped I/O (MMIO)

The shift register is controlled through the ATmega328P GPIO registers:

- `PORTD`, bit PD4: data output (Arduino D4).
- `PORTD`, bit PD7: latch output (Arduino D7).
- `PORTB`, bit PB0: clock output (Arduino D8).

This replaces `digitalWrite()` for shift-register communication.

### 5. Concurrent Tasks

| Task | Timing |
|---|---|
| Timer1 interrupt | Every 1 ms |
| Potentiometer sampling | Every 100 ms |
| LED animation | Every 50–1000 ms |
| Button handling | Event-driven |
| Relay state change | Every 5000 ms |

No blocking `delay()` calls are used.

## Running the Project

1. Open Autodesk Tinkercad Circuits.
2. Build the circuit according to the connection tables.
3. Open the Arduino code editor in Text mode.
4. Paste the project source code.
5. Start the simulation.
6. Verify the LED animation, potentiometer, pushbutton, and relay.

The same logic is intended for an Arduino Uno R3. Physical relay wiring should be checked separately.

## Testing Checklist

- [ ] All eight LEDs illuminate in sequence.
- [ ] Potentiometer changes the animation speed.
- [ ] Button reverses the animation direction.
- [ ] Relay switches every five simulated seconds.
- [ ] Timer1-based scheduling works.
- [ ] Direction is restored after restarting the controller.
- [ ] The program runs without `delay()`.

The basic LED and relay behaviors were tested during circuit development. The complete final firmware should be tested against this checklist before submission.

## Project Structure

```text
arduino-hw3/
├── README.md
├── arduino_hw3.ino
└── docs/
    └── technical_report.tex
```

## Safety and Limitations

The demonstration uses a low-voltage 5 V relay coil.

The relay contacts are not connected to an external load. Do not connect mains voltage to the breadboard circuit.

Tinkercad simulation performance may be slower than real time, especially when frequent hardware timer interrupts are enabled.

## Learning Outcomes

This project demonstrates:

- Interrupt-driven programming
- Hardware timer configuration
- Non-blocking task scheduling
- EEPROM-based state persistence
- GPIO register-level programming
- Transistor-driven relay control
- Shift-register communication

## License

Educational project for an Introduction to Robotics laboratory assignment.
