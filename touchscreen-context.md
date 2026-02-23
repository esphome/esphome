# Touchscreen Ghost-Touch Debug Context – LilyGo T5 4.7" Plus

## Ziel

Demo-App: Ein schwarzes 80×80px-Quadrat soll beim Antippen des E-Paper-Displays auf
eine zufällige Position springen (`tests/components/lilygo_t5_47_plus/test.esp32-s3-ard-touch-demo.yaml`).

## Hardware

- Board: LilyGo T5 4.7" Plus
- MCU: ESP32-S3 (QFN56, rev 0.2), 16 MB Flash, 8 MB OPI-PSRAM
- Display: 9.7" E-Paper (960×540), Treiber-IC: ED047TC1, High-Voltage-Driver on-board
- Touchscreen-IC: unbekanntes Modell, I2C-Adresse `0x5D`, INT-Pin: GPIO47
- I2C: SDA=GPIO18, SCL=GPIO17, 400 kHz

## Symptom

Das Quadrat springt **ohne Berührung** ca. alle 4–6 Sekunden von selbst.
Nach jedem Display-Refresh (der selbst ~4s dauert) wird ein Ghost-Touch ausgelöst
und die Automation feuert erneut → Endlosschleife.

## Ursachen-Analyse

### 1. Bug in `update_touches()` – false-touch bei kein Touch (GEFIXT)

```cpp
// ALT – BUG: wenn kein Finger erkannt, wird trotzdem point=1 gesetzt
// und Koordinaten aus uninitialisierten Buffer-Bytes gelesen
if (point == 0)
    point = 1;

// NEU – korrekt: einfach zurückkehren
if (point == 0)
    return;
```

**Datei:** `esphome/components/lilygo_t5_47/touchscreen/lilygo_t5_47_touchscreen.cpp`

### 2. Copy-Paste-Bug in `setup()` – Y-Achse nie initialisiert (GEFIXT)

```cpp
// ALT – BUG: x_raw_max_ wird statt y_raw_max_ gesetzt
if (this->y_raw_max_ == this->y_raw_min_) {
    this->x_raw_max_ = this->display_->get_native_height();  // FALSCH
}

// NEU – korrekt:
if (this->y_raw_max_ == this->y_raw_min_) {
    this->y_raw_max_ = this->display_->get_native_height();  // RICHTIG
}
```

**Datei:** `esphome/components/lilygo_t5_47/touchscreen/lilygo_t5_47_touchscreen.cpp`

### 3. EMI durch E-Paper-Hochspannungstreiber – INT-Pin dauerhaft LOW (KERN-PROBLEM, UNGELÖST)

Das eigentliche Problem: Der E-Paper-Treiber (ED047TC1) arbeitet mit einer
Hochspannungsversorgung. Während der Refresh-Phase (15 Frames × ~200ms = ~3s)
koppelt die Hochspannung elektromagnetisch in das kapazitive Touch-Panel ein.
Folge: Der Touch-IC meldet nach jedem Refresh mehrere Ghost-Touches.

**Beobachtung aus Logs:**

```
[17:48:43] D: Touch data cleared after 20 attempt(s), INT=LOW
[17:48:49] D: Touch data cleared after 20 attempt(s), INT=LOW
[17:48:55] D: Touch data cleared after 19 attempt(s), INT=HIGH  ← nur einmal HIGH!
[17:49:01] D: Touch data cleared after 20 attempt(s), INT=LOW
```

Der INT-Pin (GPIO47) bleibt dauerhaft LOW – auch nach 20 I2C-Lesezyklen mit je 10ms Pause
(= 200ms total). Das bedeutet: Der IC queued intern neue Ghost-Touches schneller als wir
sie leeren können. Das Problem liegt im analogen Schaltungsdesign (kein Schirmung zwischen
HV-Driver und Touch-Panel-Elektroden).

## Bisherige Fix-Versuche (alle unzureichend)

### Versuch 1: `is_refreshing`-Guard + `delay: 500ms` in YAML
→ Delay zu kurz. Ghost-Touch kommt nach Ablauf der 500ms trotzdem durch.

### Versuch 2: `is_refreshing = true` auch in `on_boot` + `delay: 2s`
→ Besser, aber beim Ablauf des Delays ist INT noch LOW → nächster Ghost-Touch direkt danach.

### Versuch 3: `clear_touch_data()` – I2C-Drain-Schleife bis INT=HIGH
```cpp
while (!this->interrupt_pin_->digital_read() && attempts < 20) {
    // I2C lesen + CLEAR_FLAGS senden
    delay(10);
    this->store_.touched = false;
}
```
→ INT bleibt LOW nach 20 Versuchen. Das IC bleibt im Ghost-Touch-Modus.

### Versuch 4: `suppress_for(ms)` + `loop()` Override (aktueller Stand)

```cpp
// lilygo_t5_47_touchscreen.h
void suppress_for(uint32_t ms) { this->suppress_until_ = millis() + ms; }

// lilygo_t5_47_touchscreen.cpp
void LilygoT547Touchscreen::loop() {
  if (millis() < this->suppress_until_) {
    this->store_.touched = false;  // ISR-Flag aggressiv verwerfen
    return;
  }
  Touchscreen::loop();
}
```

YAML ruft `id(my_touch).suppress_for(6000)` vor dem Refresh auf.

**Ergebnis:** In 60s Logs **kein Ghost-Touch** in der Ruhephase (nach Boot-Refresh).
Aber das Quadrat springt beim Benutzer trotzdem noch – unklar ob durch echte Touches
während der Suppress-Window oder wegen eines anderen Timing-Problems.

## Aktueller Code-Stand

### `esphome/components/lilygo_t5_47/touchscreen/lilygo_t5_47_touchscreen.h`

```cpp
#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

#include <vector>

namespace esphome {
namespace lilygo_t5_47 {

using namespace touchscreen;

class LilygoT547Touchscreen : public Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void loop() override;

  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }

  /// Suppresses all touch events for the given duration in milliseconds.
  /// Call this before a display refresh to prevent spurious touches caused
  /// by electromagnetic interference from the e-paper high-voltage driver.
  void suppress_for(uint32_t ms) { this->suppress_until_ = millis() + ms; }

 protected:
  void update_touches() override;

  InternalGPIOPin *interrupt_pin_;
  uint32_t suppress_until_{0};
};

}  // namespace lilygo_t5_47
}  // namespace esphome
```

### `esphome/components/lilygo_t5_47/touchscreen/lilygo_t5_47_touchscreen.cpp`

```cpp
#include "lilygo_t5_47_touchscreen.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lilygo_t5_47 {

static const char *const TAG = "lilygo_t5_47.touchscreen";

static const uint8_t POWER_REGISTER = 0xD6;
static const uint8_t TOUCH_REGISTER = 0xD0;

static const uint8_t WAKEUP_CMD[1] = {0x06};
static const uint8_t READ_FLAGS[1] = {0x00};
static const uint8_t CLEAR_FLAGS[2] = {0x00, 0xAB};
static const uint8_t READ_TOUCH[1] = {0x07};

#define ERROR_CHECK(err) \
  if ((err) != i2c::ERROR_OK) { \
    ESP_LOGE(TAG, "Failed to communicate!"); \
    this->status_set_warning(); \
    return; \
  }

void LilygoT547Touchscreen::setup() {
  this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  this->interrupt_pin_->setup();

  this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);

  if (this->write(nullptr, 0) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to communicate!");
    this->interrupt_pin_->detach_interrupt();
    this->mark_failed();
    return;
  }

  this->write_register(POWER_REGISTER, WAKEUP_CMD, 1);
  if (this->display_ != nullptr) {
    if (this->x_raw_max_ == this->x_raw_min_) {
      this->x_raw_max_ = this->display_->get_native_width();
    }
    if (this->y_raw_max_ == this->y_raw_min_) {
      this->y_raw_max_ = this->display_->get_native_height();  // BUGFIX: war x_raw_max_
    }
  }
}

void LilygoT547Touchscreen::loop() {
  if (millis() < this->suppress_until_) {
    // Aggressively discard ISR-set flags while suppressed so that ghost
    // touches caused by e-paper high-voltage EMI cannot reach the automations.
    this->store_.touched = false;
    return;
  }
  Touchscreen::loop();
}

void LilygoT547Touchscreen::update_touches() {
  uint8_t point = 0;
  uint8_t buffer[40] = {0};

  i2c::ErrorCode err;
  err = this->write_register(TOUCH_REGISTER, READ_FLAGS, 1);
  ERROR_CHECK(err);

  err = this->read(buffer, 7);
  ERROR_CHECK(err);

  if (buffer[0] == 0xAB) {
    this->write_register(TOUCH_REGISTER, CLEAR_FLAGS, 2);
    return;
  }

  point = buffer[5] & 0xF;

  if (point == 1) {
    err = this->write_register(TOUCH_REGISTER, READ_TOUCH, 1);
    ERROR_CHECK(err);
    err = this->read(&buffer[5], 2);
    ERROR_CHECK(err);
  } else if (point > 1) {
    err = this->write_register(TOUCH_REGISTER, READ_TOUCH, 1);
    ERROR_CHECK(err);
    err = this->read(&buffer[5], 5 * (point - 1) + 3);
    ERROR_CHECK(err);
  }

  this->write_register(TOUCH_REGISTER, CLEAR_FLAGS, 2);

  if (point == 0)  // BUGFIX: war "point = 1" → false-touch aus uninit. Buffer
    return;

  uint16_t id, x_raw, y_raw;
  for (uint8_t i = 0; i < point; i++) {
    id = (buffer[i * 5] >> 4) & 0x0F;
    y_raw = (uint16_t) ((buffer[i * 5 + 1] << 4) | ((buffer[i * 5 + 3] >> 4) & 0x0F));
    x_raw = (uint16_t) ((buffer[i * 5 + 2] << 4) | (buffer[i * 5 + 3] & 0x0F));
    this->add_raw_touch_position_(id, x_raw, y_raw);
  }

  this->status_clear_warning();
}

void LilygoT547Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "Lilygo T5 47 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
}

}  // namespace lilygo_t5_47
}  // namespace esphome
```

### `tests/components/lilygo_t5_47_plus/test.esp32-s3-ard-touch-demo.yaml`

```yaml
---
esphome:
  name: lilygo-t547-touch-demo
  on_boot:
    priority: -100
    then:
      - lambda: 'id(my_touch).suppress_for(6000);'
      - component.update: my_display

packages:
  board: !include ../../../esphome/components/lilygo_t5_47_plus/board-config.yaml

logger:
  hardware_uart: USB_SERIAL_JTAG

i2c:
  sda: GPIO18
  scl: GPIO17
  frequency: 400kHz

globals:
  - id: sq_x
    type: int
    initial_value: '440'
  - id: sq_y
    type: int
    initial_value: '220'
  - id: is_refreshing
    type: bool
    initial_value: 'false'

touchscreen:
  - platform: lilygo_t5_47
    id: my_touch
    interrupt_pin: GPIO47
    address: 0x5D
    display: my_display
    on_touch:
      - if:
          condition:
            lambda: 'return !id(is_refreshing);'
          then:
            - lambda: |-
                id(is_refreshing) = true;
                id(sq_x) = esp_random() % (960 - 80);
                id(sq_y) = esp_random() % (540 - 80);
                id(my_touch).suppress_for(6000);
            - component.update: my_display
            - lambda: 'id(is_refreshing) = false;'

display:
  - platform: lilygo_t5_47_plus
    id: my_display
    greyscale: false
    update_interval: never
    lambda: |-
      it.fill(Color(0, 0, 0));
      it.filled_rectangle(id(sq_x), id(sq_y), 80, 80, Color(255, 255, 255));
```

## Offenes Problem / Hypothesen für nächste Schritte

Das Quadrat springt trotz `suppress_for(6000)` noch. Mögliche verbleibende Ursachen:

### Hypothese A: suppress_for zu kurz
Der INT-Pin war nach 200ms I2C-Drains noch LOW. 6s könnten immer noch zu kurz sein
wenn das kapazitive Panel sehr lange braucht um Ladung abzubauen.
→ Testen: `suppress_for(10000)` oder `suppress_for(15000)`.

### Hypothese B: suppress_until_ überläuft millis() nach Boot
`millis()` startet bei 0. `suppress_for(6000)` in `on_boot` mit `priority: -100`
könnte vor der Setup-Phase aufgerufen werden wenn `millis()` noch sehr klein ist.
`suppress_until_` = z.B. 200 + 6000 = 6200ms. Das würde funktionieren.
→ Wahrscheinlich kein Problem, aber prüfen.

### Hypothese C: on_touch feuert während Suppress-Window durch on_boot-Timing
Das `loop()` override greift nur wenn `millis() < suppress_until_`.
Der `on_boot` Sequence-Schritt `lambda: 'id(my_touch).suppress_for(6000);'` läuft
**vor** `component.update: my_display`. Der Display-Refresh selbst dauert ~4s.
Also: suppress_until_ = T + 6s, Refresh endet bei ca. T + 4s, Suppress läuft bis T + 6s.
Das sollte 2s Puffer geben. Aber wenn der Ghost-Touch nach T + 6s noch kommt...
→ Prüfen ob das Timing durch Logging bestätigt werden kann.

### Hypothese D: on_touch-Trigger ist nicht interrupt-basiert sondern polling
Beim Start warnt der Logger:
```
[W][touchscreen:032]: Touch Polling Stopped. You can safely remove the 'update_interval:' variable
```
Das zeigt: Die Basisklasse erkennt den Interrupt und stoppt den Poller. Aber WANN genau?
Wenn der Poller **vor** dem Interrupt-Setup noch läuft und dabei durch den uninitialisierten
Store-State einen Touch meldet – das wäre ein weiterer Pfad für Ghost-Touches.

### Hypothese E: `update_touches()` wird trotz suppress_until_ aufgerufen
Die `loop()` override verhindert nur dass `store_.touched` zur Basisklasse durchkommt.
Aber: `update_touches()` wird von `Touchscreen::loop()` aufgerufen, nicht direkt.
Das sollte also korrekt geblockt sein. Zur Sicherheit prüfen ob evtl. ein
`PollingComponent::update()` separat feuert.

### Hypothese F: Touch-IC Adresse / Protokoll falsch
Die YAML nutzt Adresse `0x5D`. Der Standard-WAKEUP-Command schreibt `0x06` an Register `0xD6`.
Es ist nicht klar welches Touch-IC-Modell verbaut ist. Falls das Protokoll falsch ist
könnten Lesefehler dazu führen dass immer `point > 0` zurückgegeben wird.
→ I2C-Scan durchführen und IC-Datenblatt identifizieren.

## Relevante Dateipfade

- Touchscreen-Treiber: `esphome/components/lilygo_t5_47/touchscreen/`
- Display-Treiber: `esphome/components/lilygo_t5_47_plus/`
- E-Paper IC-Treiber: `esphome/components/lilygo_t5_47_plus/epd_driver.c`
- Board-Config: `esphome/components/lilygo_t5_47_plus/board-config.yaml`
- Test-YAML: `tests/components/lilygo_t5_47_plus/test.esp32-s3-ard-touch-demo.yaml`
- Touchscreen-Basisklasse: `esphome/components/touchscreen/touchscreen.cpp`

## Wichtige Erkenntnisse zur Touchscreen-Basisklasse

`Touchscreen` erbt von `PollingComponent`. `update()` setzt `store_.touched = true`
solange kein Interrupt registriert ist. Sobald ein Interrupt erkannt wird, stoppt der Poller.

`loop()` prüft `store_.touched` und ruft dann `update_touches()` → `send_touches_()` → Trigger.

`on_touch` feuert nur beim **ersten** Touch-Event (wenn `first_touch_` == true, d.h. touches_ war leer).

Der ISR `TouchscreenInterrupt::gpio_intr` setzt nur `store_.touched = true` – simpel und schnell.
Das `loop()` override in `LilygoT547Touchscreen` setzt dieses Flag auf `false` solange
`millis() < suppress_until_`.

## Logger-Hinweis

USB_SERIAL_JTAG muss in der YAML aktiviert sein (`hardware_uart: USB_SERIAL_JTAG`) da der
ESP32-S3 bei diesem Board USB-CDC verwendet. Ohne diese Einstellung ist kein Log-Output sichtbar.
