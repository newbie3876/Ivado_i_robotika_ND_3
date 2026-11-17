#include <Arduino.h>
#include <EEPROM.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

// Sujungimai:
constexpr uint8_t DATA_PIN = 4;
constexpr uint8_t LATCH_PIN = 7;
constexpr uint8_t CLOCK_PIN = 8;
constexpr uint8_t BUTTON_PIN = 2;
constexpr uint8_t RELAY_PIN = 9;
constexpr uint8_t POT_PIN = A0;

// Konstantos:
constexpr uint8_t LED_COUNT = 5;
constexpr int EEPROM_DIRECTION_ADDR = 0;
constexpr uint32_t RELAY_PERIOD_MS = 5000UL;
constexpr uint32_t POT_PERIOD_MS = 100UL;
constexpr uint32_t DEBOUNCE_MS = 30UL;

// Pertraukčių kintamieji:
volatile uint32_t tickMs = 0;
volatile bool buttonEdgePending = false;

// Laiko žymos:
uint32_t lastLedMs = 0;
uint32_t lastRelayMs = 0;
uint32_t lastPotMs = 0;
uint32_t buttonStateMs = 0;

// Programos būsenos:
uint16_t ledPeriodMs = 500;
uint8_t ledPosition = 0;

bool forwardDirection = true;
bool relayOn = false;

// Mygtuko būsenos:
enum ButtonState {WAIT_PRESS, DEBOUNCE_PRESS, WAIT_RELEASE, DEBOUNCE_RELEASE};
ButtonState buttonState = WAIT_PRESS;

// Timer1: 16 MHz / 64 / 250 = 1000 Hz:
ISR(TIMER1_COMPA_vect) {
  ++tickMs;
}

// INT0: užfiksuojamas paspaudimo įvykis:
void onButtonFalling() {
  buttonEdgePending = true;
}

// Atomiškai nuskaitomas 32 bitų laikas:
uint32_t systemTimeMs() {
  uint32_t result;

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    result = tickMs;
  }

  return result;
}

// Timer1 CTC konfigūracija:
void configureTimer1() {
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    TCCR1A = 0;
    TCCR1B = 0;
    TIMSK1 = 0;

    TCNT1 = 0;
    OCR1A = 249;

    TIFR1 = _BV(OCF1A);
    TIMSK1 = _BV(OCIE1A);
    TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10);}
}

// 74HC595 valdymas per MMIO, D4 = PD4, D7 = PD7, D8 = PB0:
void writeShiftRegister(uint8_t value) {
  PORTD &= ~_BV(PD7); // LATCH LOW

  for (int8_t bit = 7; bit >= 0; --bit) {
    if (value & (uint8_t(1) << bit)) {
      PORTD |= _BV(PD4);
    } else {
      PORTD &= ~_BV(PD4);
    }

    PORTB |= _BV(PB0);  // CLOCK HIGH
    PORTB &= ~_BV(PB0); // CLOCK LOW
  }

  PORTD |= _BV(PD7); // LATCH HIGH
}

// Potenciometro nuskaitymas kas 100 ms:
void updatePotentiometer(uint32_t now) {
  if (now - lastPotMs < POT_PERIOD_MS) {
    return;
  }

  lastPotMs = now;

  int reading = analogRead(POT_PIN);
  ledPeriodMs = static_cast<uint16_t>(map(reading, 0, 1023, 50, 1000));
}

// Krypties pakeitimas ir išsaugojimas:
void changeDirection() {
  forwardDirection = !forwardDirection;

  EEPROM.update(EEPROM_DIRECTION_ADDR, forwardDirection ? 1 : 0);

  Serial.print(F("Kryptis: "));
  Serial.println(forwardDirection ? F("PIRMYN") : F("ATGAL"));
}

// Mygtuko kontaktų virpėjimo filtravimas:
void updateButton(uint32_t now) {
  bool edge;

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    edge = buttonEdgePending;
    buttonEdgePending = false;
  }

  bool pressed = (digitalRead(BUTTON_PIN) == LOW);

  switch (buttonState) {
    case WAIT_PRESS:
      if (edge && pressed) {
        buttonStateMs = now;
        buttonState = DEBOUNCE_PRESS;
      }
      break;

    case DEBOUNCE_PRESS:
      if (!pressed) {
        buttonState = WAIT_PRESS;
      } else if (now - buttonStateMs >= DEBOUNCE_MS) {
        changeDirection();
        buttonState = WAIT_RELEASE;
      }
      break;

    case WAIT_RELEASE:
      if (!pressed) {
        buttonStateMs = now;
        buttonState = DEBOUNCE_RELEASE;
      }
      break;

    case DEBOUNCE_RELEASE:
      if (pressed) {
        buttonState = WAIT_RELEASE;
      } else if (now - buttonStateMs >= DEBOUNCE_MS) {
        buttonState = WAIT_PRESS;
      }
      break;
  }
}

// Einamosios LED pozicijos rodymas:
void showLed() {
  writeShiftRegister(uint8_t(1) << ledPosition);
  Serial.print(F("LED: "));
  Serial.println(ledPosition + 1);
}

// LED judėjimas pagal pasirinktą kryptį:
void updateLed(uint32_t now) {
  if (now - lastLedMs < ledPeriodMs) {
    return;
  }

  lastLedMs = now;

  if (forwardDirection) {
    ledPosition = (ledPosition + 1) % LED_COUNT;
  } else {
    ledPosition =
      (ledPosition + LED_COUNT - 1) % LED_COUNT;
  }

  showLed();
}

// Rėlės perjungimas kas 5 sekundes:
void updateRelay(uint32_t now) {
  if (now - lastRelayMs < RELAY_PERIOD_MS) {
    return;
  }

  lastRelayMs = now;
  relayOn = !relayOn;

  digitalWrite(RELAY_PIN, relayOn ? HIGH : LOW);
  Serial.print(F("Rele: "));
  Serial.println(relayOn ? F("IJUNGTA") : F("ISJUNGTA"));
}

void setup() {
  // Saugi pradinė rėlės būsena:
  digitalWrite(RELAY_PIN, LOW);
  pinMode(RELAY_PIN, OUTPUT);

  // 74HC595 kontaktai:
  pinMode(DATA_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);

  // Įvestys:
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(POT_PIN, INPUT);
  writeShiftRegister(0);

  // Krypties atkūrimas iš EEPROM:
  uint8_t saved = EEPROM.read(EEPROM_DIRECTION_ADDR);

  if (saved <= 1) {
    forwardDirection = (saved == 1);
  } else {
    forwardDirection = true;
    EEPROM.update(EEPROM_DIRECTION_ADDR, 1);
  }

  Serial.begin(9600);

  // Laikmačio konfigūracija:
  configureTimer1();

  // Išorinė pertrauktis INT0:
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onButtonFalling, FALLING);

  Serial.println(F("Timer1 CTC: 1 ms"));
  Serial.println(F("INT0: mygtukas D2"));
  Serial.println(F("MMIO: SN74HC595N"));
  Serial.println(F("Rele: kas 5000 ms"));
  Serial.print(F("Atkurta kryptis: "));
  Serial.println(forwardDirection ? F("PIRMYN") : F("ATGAL"));

  // Pirmasis LED įsijungia iš karto:
  ledPosition = 0;
  showLed();
}

void loop() {
  uint32_t now = systemTimeMs();

  updatePotentiometer(now);
  updateButton(now);
  updateLed(now);
  updateRelay(now);
}
