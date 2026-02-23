# TODO: LILYGO T5 4.7" Plus - Upstream Integration

Stand: 2026-02-23 | Basis: ESPHome 2026.3.0-dev

## ✅ Erledigt

- Dateien umbenannt: `t547.h/cpp` → `lilygo_t5_47_plus_display.h/cpp`
- C++ Coding Conventions: `this->` Präfixe, `protected` Fields, Namespace
- Version-Guards `#if ESPHOME_VERSION_CODE` entfernt (2023.12+ ist Baseline)
- Nicht implementierte `clean()` Deklaration entfernt
- Logging-Level: `ESP_LOGI` in `setup()` → `ESP_LOGV`/`ESP_LOGD`
- `board_config.yaml` bereinigt: redundante `platformio_options` und `sdkconfig_options`
  entfernt (werden von `psram:`-Komponente automatisch gesetzt)
- `__init__.py`: `MULTI_CONF = False`, `DEPENDENCIES = ["esp32", "psram"]`
- `display.py`: Duplikat `DEPENDENCIES`, ungenutzter `pins`-Import entfernt
- Rebase auf `upstream/dev` (ESPHome 2026.2.x), keine Konflikte
- Kompiliert erfolgreich (ESP32-S3, Arduino, 46.6% Flash, 12.3% RAM)
- Board geflasht und getestet – Display-Refresh funktioniert (~4s, normal für E-Paper)
- **Refactor: Datei- und Klassennamen** (2026-02-23):
  - `utilities.h` → `board_pins.h`
  - `board-config.yaml` → `board_config.yaml`
  - `lilygo_t5_47_plus_display.{cpp,h}` → `display.{cpp,h}`
  - `sensor/lilygo_t5_47_plus_battery.{cpp,h}` → `sensor/battery.{cpp,h}`
  - `touchscreen/lilygo_t5_47_plus_touchscreen.{cpp,h}` → `touchscreen/touchscreen.{cpp,h}`
  - Klasse `LilygoT547Plus` → `LilygoT547PlusDisplay`
  - Klasse `LilygoT547PlusBatterySensor` → `LilygoT547PlusBattery`
- **Battery Sensor Sub-Platform** (`sensor/`): Spannung + SoC aus GPIO14 ADC, LP103454-Kurve,
  konfigurierbare `min_voltage` / `max_voltage` (Defaults: 3.0 V / 4.2 V)
- **Touchscreen Sub-Platform** (`touchscreen/`): Eigener GT911-Treiber (16-bit Register),
  GPIO47-Wakeup-Sequenz, Polling-Modus (kein Interrupt), `transform: swap_xy/mirror_y`
- `TODO.md` und `UPSTREAM_ANALYSIS.md` aus git-Tracking entfernt (bleiben lokal)

---

## 🔴 Kritisch (vor PR)

### 1. C-Treiber-Dateien: Lizenz & Attribution
- `epd_driver.c/h`, `ed047tc1.c/h`, `i2s_data_bus.c/h`, `rmt_pulse.c/h`, `font.c`
- Stammen aus dem LilyGo-Fork von [epdiy](https://github.com/vroland/epdiy)
  (Branch `esp32s3` von `Xinyuan-LilyGO/LilyGo-EPD47`)
- **Lizenzlage** (korrigiert 2026-02-23):
  - epdiy original: **LGPL-3.0**
  - LilyGo-Fork (`Xinyuan-LilyGO/LilyGo-EPD47`): **GPL-3.0**
  - ESPHome C/C++ Code (.c, .cpp, .h): **GPLv3** (siehe `LICENSE` im Repo-Root)
  - ESPHome Python Code: **MIT**
  - → **GPL-3.0 und LGPL-3.0 sind beide kompatibel mit GPLv3** → kein Lizenz-Blocker
- **Verbleibendes Problem: Fehlende Attribution**
  - Keine der 9 C-Dateien enthält Copyright-Header oder Lizenz-Hinweis
  - (L)GPL verlangt Erhalt von Copyright-Vermerk und Lizenznotiz
  - **TODO:** Copyright-Header des Originalautors (Valentin Roland / vroland) und
    Lizenzhinweis (LGPL-3.0) in alle Dateien einfügen. Ggf. Hinweis auf Modifikationen
    durch LilyGo-Fork (GPL-3.0) und eigene Anpassungen.
- **Versionsstand der C-Dateien** (recherchiert 2026-02-23):
  - Die 9 Dateien stammen aus dem **LilyGo-Fork** (`esp32s3`-Branch), der auf der
    **alten epdiy v1.x API** basiert (monolithische Dateien: `epd_driver.c`, `ed047tc1.c`,
    `i2s_data_bus.c`, `rmt_pulse.c`, `font.c`)
  - Letzter LilyGo-Fork-Commit mit Änderungen an diesen Dateien: ~2 Jahre alt
    (diverse „Fix compilation warning" und „Fix v3.0.5 compilation error"-Commits)
  - **epdiy v2.0.0** (released Nov 2023) hat die Architektur komplett umgebaut:
    - Alte monolithische Dateien existieren nicht mehr
    - Neue modulare Struktur: `board/`, `output_i2s/`, `output_lcd/`, `output_common/`,
      `waveforms/`
    - Board-/Display-Konfiguration dynamisch statt compile-time
    - Lizenz: weiterhin **LGPL-3.0**
  - **⚠️ Hardware-Kompatibilität – kein passendes Board in epdiy v2.x!**
    Pin-Vergleich (ESP32-S3) zeigt: unser Board ist **keines** der epdiy-v2-Boards:
    - `epd_board_lilygo_t5_47`: Nur ESP32 (original), nicht S3. Shift-Register + I2S.
      Pins: CFG_DATA=23, CFG_CLK=18, CKV=25, D0=33...D7=22
    - `lilygo_board_s3` (PR #383, Jan 2025): Das ist die **LilyGo T5 S3 E-Paper Pro**,
      ein völlig anderes Board! Nutzt PCA9555 I/O-Expander + TPS65185 PMIC über I2C
      (SDA=39, SCL=40), LCD-Peripheral, komplett andere Pinbelegung:
      CKV=48, STH=41, CKH=4, D0=5...D7=8
    - **Unser Board** (T5 4.7" Plus, ESP32-S3): Shift-Register-Steuerung über
      CFG_DATA=13, CFG_CLK=12, CFG_STR=0 + I2S-Datenbus.
      CKV=38, STH=40, CKH=41, D0=8, D1=1...D7=7
    - → Das T5 4.7" Plus ist eine ESP32-S3-Portierung des originalen T5 4.7" mit
      identischer Architektur (Shift-Register + I2S) aber auf S3-GPIOs umgemappt.
      Für epdiy v2.x müsste ein **neues Board-Definition-File** geschrieben werden
      (basierend auf `epd_board_lilygo_t5_47.c` + output_i2s, mit S3-Pin-Mapping).
  - **Bewertung der Optionen (aktualisiert 2026-02-23):**
    - **Option A** – epdiy v2.x als externe Library: ❌ **Nicht machbar.**
      Kein passendes Board-Definition-File für T5 4.7" Plus. I2S-Output-Pfad auf S3
      müsste komplett neu geschrieben werden (epdiy v2 nutzt LCD-Peripheral, nicht I2S).
    - **Option B** – LilyGo-Fork als externe PlatformIO-Library: ❌ **Getestet und gescheitert.**
      `cg.add_library()` mit Git-URL lädt die Bibliothek nach `.piolibdeps/`, aber
      ESPHome's Build-System (Arduino+ESP-IDF combined mode) setzt `lib_ldf_mode = off`
      und generiert `EXTRA_COMPONENT_DIRS` nur für `src/`. Die Include-Pfade der
      externen Library werden nicht in den CMake-Build aufgenommen.
      `add_idf_component()` nutzt den ESP-IDF Component Manager (Semver), nicht
      Git-Branches. → **Kein sauberer Weg, eine Git-Branch-Library einzubinden.**
      (Getestet auf Branch `feature_lilygo_t5_47_plus_ext_lib`, verworfen.)
    - **Option C** – Eingebettete Dateien beibehalten + Attribution: ✅ **Gewählt.**
      Copyright-Header ergänzen, funktioniert sofort, kein Migrationsrisiko.
      Code ist stabil und auf Hardware getestet. Gängiges Muster in Embedded-Projekten.
  - **Status**: **Option C gewählt.** Attribution-Header in alle 9 C-Dateien ergänzen.
    TODO: Copyright-Header mit Originalautoren (Valentin Roland / vroland, LGPL-3.0)
    und LilyGo-Fork-Hinweis (GPL-3.0) in jede Datei einfügen.

### 2. Component Tests erweitern ✅
- `test.esp32-s3-ard.yaml` – greyscale, mit Lambda ✅
- `test.esp32-s3-ard-greyscale-off.yaml` – binary mode ✅
- `test.esp32-s3-ard-minimal.yaml` – ohne Lambda, ohne Netzwerk ✅
- `test.esp32-s3-ard-battery.yaml` – Battery Sensor (Spannung + Level) ✅
- `test.esp32-s3-ard-touch-demo.yaml` – Touchscreen Demo ✅
- Script-Test: `test_build_components -c lilygo_t5_47_plus -t esp32-s3-ard` → **3/3 passed** ✅
- Auf Hardware geflasht und verifiziert: greyscale-off, touch-demo ✅
- [ ] Grouping-Test noch offen: `./script/test_component_grouping.py -e config --all`

### 3. Pre-commit Linting
- [ ] `python3 script/run-in-env.py pre-commit run --all-files`
- Erwartet: clang-format, ruff, yamllint Prüfungen bestehen

---

## 🟡 Wichtig (vor PR, oder begründet überspringen)

### 4. Touch-Dokumentation im Test/README ✅
- Eigene Touchscreen Sub-Platform unter `touchscreen/` (nicht `gt911`, nicht `lilygo_t5_47`)
- GT911 16-bit Register, reines Polling, GPIO47-Wakeup in `setup()`
- `update_interval: 300ms` – reduziert EMI-Ghost-Touches
- `transform: swap_xy: true, mirror_y: true` für korrekte Koordinaten
- Funktionierendes Beispiel: `test.esp32-s3-ard-touch-demo.yaml` ✅

### 5. ESP-IDF Support
- Aktuell: Arduino-only (`cv.only_with_arduino`) wegen I2S/RMT-Treibern in den C-Dateien
- **Entscheidung dokumentieren**: Arduino-only ist OK, wenn in README begründet
- Keine Aktion nötig, aber im PR erwähnen

### 6. ESP32 (Original) Support
- `utilities.h` hat ESP32-Defines – aber keine Tests, nur ESP32-S3 Board
- **Entscheidung**: Nur ESP32-S3 unterstützt (Hardware-Unterschiede MCU/PSRAM)
- Nicht unterstützte Plattform aus `utilities.h` entfernen oder kommentieren

### 7. `millis()` durch ESPHome-Äquivalent ersetzen ✅
- `display.cpp` nutzte `millis()` an **9 Stellen** (in `update()` und `display()`)
- Ersetzt durch `esphome::millis()` für konsistenten Namespace-Zugriff
- Logging-Level: `ESP_LOGI` → `ESP_LOGD` (Timing-Logs sind Debug-Info, keine Info-Meldungen)

### 8. Buffer-Allokation: `free()` in `setup()`
- `setup()` ruft `free(this->buffer_)` auf falls bereits allokiert
- Da `setup()` nur einmal läuft: OK – aber im PR erwähnen

### 9. Tote Methoden und Felder entfernen ✅
- `eink_on_()`, `eink_off_()`, `panel_on_`, `temperature_`, `get_panel_state()` entfernt

---

## 🟢 Nice-to-Have (nach erstem PR)

- **Partial Refresh**: `epd_hl_update_area()` für schnellere Updates
- **Deep Sleep Integration**: Batterie-Support des Boards nutzen
- **Buttons als Binary Sensors**: `BUTTON_1/2/3` aus `board_pins.h` dokumentieren
- ~~**Battery ADC Sensor**~~ ✅ Umgesetzt als `sensor/`-Sub-Platform

---

## 📋 PR-Checkliste

- [ ] Alle 🔴 Kritischen Punkte abgeschlossen
- [ ] pre-commit läuft sauber durch
- [ ] Component Tests erfolgreich
- [ ] `PULL_REQUEST_TEMPLATE.md` ausgefüllt
- [ ] PR gegen `dev` Branch
- [ ] PR-Titel: `[lilygo_t5_47_plus] Add LILYGO T5 4.7" Plus E-Paper display`
- [ ] Touch-Hinweis im PR: Eigene `touchscreen/`-Sub-Platform + GPIO47-Wakeup in `setup()` + 300ms Polling

---

## 🔗 Referenzen

- Eigene Touchscreen-Platform: `esphome/components/lilygo_t5_47_plus/touchscreen/` (GT911, 16-bit Register, Polling)
- Upstream `gt911`-Komponente: `esphome/components/gt911/` (interrupt-driven, anderes Protokoll)
- `lilygo_t5_47/touchscreen/`: anderer IC (8-bit Register), nicht verwandt
- epdiy Library: https://github.com/vroland/epdiy
- ESPHome Contribution Guide: https://developers.esphome.io/contributing/
- ESPHome Breaking Changes Policy: https://developers.esphome.io/contributing/code/#public-api-and-breaking-changes
