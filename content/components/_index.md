---
description: "main documentation"
title: "ESPHome Docs"
params:
  seo:
    description: ESPHome main documentation
    image: logo-docs.svg
---

{{< html_file file="images/logo-docs.svg" >}}

This is the top-level ESPHome documentation index. Browse the tables below, use the sidebar menu, or the search
function to find the information you're looking for.

### Help improve this documentation

If you find any errors in this site, corrections are welcome. You can submit a *Pull Request* (PR) in the
[GitHub repo](https://github.com/esphome/esphome-docs) with corrections. If you don't know how to create a PR you
can just use the "Edit this page on GitHub" link on the page in question which will take you to the source file
for that page.

Alternatively, post in the *Documentation* channel in the [Discord](https://discord.gg/KhAMKrd) server.

## ESPHome Configuration

ESPHome is configured in YAML files - use these links for basic and advanced
information about ESPHome configuration files.

{{< imgtable >}}
"YAML Configuration","guides/yaml","description.svg","dark-invert"
"Packages","components/packages","description.svg","dark-invert"
"Substitutions","components/substitutions","description.svg","dark-invert"
"External Components","components/external_components","external_components.svg","dark-invert"
{{< /imgtable >}}
{{< anchor "devices" >}}

## Supported Microcontrollers

{{< imgtable >}}
"ESP32","components/esp32","esp32.svg",""
"ESP8266","components/esp8266","esp8266.svg",""
"RP2040","components/rp2040","rp2040.svg",""
"BK72xx","components/libretiny","bk72xx.svg",""
"RTL87xx","components/libretiny","rtl87xx.svg",""
"LN882x","components/libretiny","ln882x.svg",""
"Host","components/host","host.svg","dark-invert"
"NRF52","components/nrf52","nrf52.svg",""
{{< /imgtable >}}

## Microcontroller Peripherals

Peripherals which directly support the operation of the microcontroller's processor(s).

{{< imgtable >}}
"PSRAM","components/psram","psram.svg",""
"Deep Sleep","components/deep_sleep","hotel.svg","dark-invert"
"ESP32-P4 LDO regulator","components/esp_ldo","ldo.svg","dark-invert"
{{< /imgtable >}}

## ESPHome Automations

> *"When this happens, I want it to do that..."*

Automations are how we customize ESPHome devices to respond/behave exactly how you want them to.

{{< imgtable >}}
"Overview","automations/index","description.svg","dark-invert"
"Actions, Triggers, Conditions","automations/actions","description.svg","dark-invert"
"Templates","automations/templates","description.svg","dark-invert"
{{< /imgtable >}}

## ESPHome Components

ESPHome-specific components or components supporting ESPHome device provisioning post-installation.

{{< imgtable >}}
"Core","components/esphome","cloud-circle.svg","dark-invert"
"Captive Portal","components/captive_portal","wifi-strength-alert-outline.svg","dark-invert"
"Copy","components/copy","content-copy.svg","dark-invert"
"Demo","components/demo","description.svg","dark-invert"
"External Components","components/external_components","external_components.svg","dark-invert"
"Globals","components/globals","description.svg","dark-invert"
"Improv via BLE","components/esp32_improv","improv.svg","dark-invert"
"Improv via Serial","components/improv_serial","improv.svg","dark-invert"
"Interval","components/interval","description.svg","dark-invert"
"JSON","components/json","json.svg","dark-invert"
"Mapping","components/mapping","mapping.svg","dark-invert"
"XXTEA","components/xxtea","xxtea.svg",""
"Script","components/script","description.svg","dark-invert"
"Factory Reset","components/factory_reset","restart-alert.svg","dark-invert"
{{< /imgtable >}}

## Network Hardware

{{< imgtable >}}
"WiFi","components/wifi","network-wifi.svg","dark-invert"
"ESP32 Ethernet","components/ethernet","ethernet.svg","dark-invert"
"ESP32 Hosted","components/esp32_hosted","network-wifi.svg","dark-invert"
"OpenThread","components/openthread","openthread.png",""
{{< /imgtable >}}

## Network Protocols

{{< imgtable >}}
"Network Core","components/network","server-network.svg","dark-invert"
"Native API","components/api","server-network.svg","dark-invert"
"MQTT","components/mqtt","mqtt.png",""
"ESP-NOW","components/espnow","esp-now.svg",""
"HTTP Request","components/http_request","connection.svg","dark-invert"
"mDNS","components/mdns","radio-tower.svg","dark-invert"
"WireGuard","components/wireguard","wireguard_custom_logo.svg","dark-invert"
"StatsD","components/statsd","connection.svg","dark-invert"
"UDP","components/udp","udp.svg",""
"Packet Transport","components/packet_transport/index","packet_transport.svg","dark-invert"
{{< /imgtable >}}

## Bluetooth/BLE

{{< imgtable >}}
"ESP32 BLE Beacon","components/esp32_ble_beacon","bluetooth.svg","dark-invert"
"ESP32 BLE Client","components/ble_client","bluetooth.svg","dark-invert"
"ESP32 BLE Tracker","components/esp32_ble_tracker","bluetooth.svg","dark-invert"
"ESP32 BLE Server","components/esp32_ble_server","bluetooth.svg","dark-invert"
"Bluetooth Proxy","components/bluetooth_proxy","bluetooth.svg","dark-invert"
"Improv via BLE","components/esp32_improv","improv.svg","dark-invert"
"Nordic UART Service (NUS)","components/ble_nus","uart.svg",""
{{< /imgtable >}}

## Management and Monitoring

{{< imgtable >}}
"Debug","components/debug","bug-report.svg","dark-invert"
"Logger","components/logger","file-document-box.svg","dark-invert"
"Syslog","components/syslog","file-document-box.svg","dark-invert"
"Prometheus","components/prometheus","prometheus.svg",""
"StatsD","components/statsd","connection.svg","dark-invert"
"Safe Mode","components/safe_mode","restart-alert.svg","dark-invert"
"Web Server","components/web_server","http.svg",""
"ESP32 Camera Web Server","components/esp32_camera_web_server","camera.svg","dark-invert"
{{< /imgtable >}}

## Update Installation

Install updates over-the-air (OTA).

{{< imgtable >}}
"OTA Core","components/ota/index","system-update.svg","dark-invert"
"OTA Updates","components/ota/esphome","system-update.svg","dark-invert"
"OTA Updates via HTTP Request","components/ota/http_request","system-update.svg","dark-invert"
{{< /imgtable >}}

## Update Management

Create update entities simplifying management of OTA updates.

{{< imgtable >}}
"Update Core","components/update/index","system-update.svg","dark-invert"
"Managed Updates","components/update/http_request","system-update.svg","dark-invert"
{{< /imgtable >}}

## Hardware Peripheral Interfaces/Busses

{{< imgtable >}}
"1-Wire","components/one_wire/index","one-wire.svg",""
"CAN Bus","components/canbus/index","canbus.svg",""
"I²C Bus","components/i2c","i2c.svg",""
"I²S Audio","components/i2s_audio","i2s_audio.svg",""
"OpenTherm","components/opentherm","opentherm.png",""
"SPI Bus","components/spi","spi.svg",""
"TinyUSB","components/tinyusb","usb.svg","dark-invert"
"UART","components/uart","uart.svg",""
"USB CDC-ACM","components/usb_cdc_acm","usb.svg","dark-invert"
"USB Host","components/usb_host","usb.svg","dark-invert"
"USB UART","components/usb_uart","usb.svg","dark-invert"
{{< /imgtable >}}

## I/O Expanders/Multiplexers

{{< imgtable >}}
"CH422G","components/ch422g","ch422g.svg"
"MAX6956 - I²C Bus","components/max6956","max6956.jpg"
"MCP230XX - I²C Bus","components/mcp230xx","mcp230xx.svg"
"MCP23SXX - SPI Bus","components/mcp23Sxx","mcp23sxx.svg"
"PCA6416A","components/pca6416a","pca6416a.svg"
"PCA9554","components/pca9554","pca9554a.jpg"
"PCF8574","components/pcf8574","pcf8574.jpg"
"PI4IOE5V6408","components/pi4ioe5v6408","pca9554a.jpg"
"SN74HC165","components/sn74hc165","sn74hc595.jpg"
"SN74HC595","components/sn74hc595","sn74hc595.jpg"
"SX1509","components/sx1509","sx1509.jpg"
"TCA9548A I²C Multiplexer","components/tca9548a","tca9548a.jpg"
"TCA9555","components/tca9555","tca9555.svg"
"WeiKai SPI/I²C UART/IO Expander","components/weikai","wk2168.jpg"
"XL9535","components/xl9535","xl9535.svg"
{{< /imgtable >}}

## 1-Wire Bus

Platforms which specifically support or extend the {{< docref "/components/one_wire" >}}, allowing communication with
1-Wire-based devices.

{{< imgtable >}}
"DS2484","components/one_wire/ds2484","ds2484.svg"
"GPIO","components/one_wire/gpio","gpio.svg"
{{< /imgtable >}}

## CAN Bus

Platforms which specifically support or extend the {{< docref "/components/canbus" >}}, allowing communication with
CAN-based devices.

{{< imgtable >}}
"ESP32 CAN","components/canbus/esp32_can","esp32.svg"
"MCP2515","components/canbus/mcp2515","mcp2515.svg"
{{< /imgtable >}}

## Sensor Components

Sensors are organized into categories; if a given sensor fits into more than one category, it will appear multiple times.

### Core

{{< imgtable >}}
"Sensor Core","components/sensor/index","folder-open.svg","dark-invert"
"Template Sensor","components/sensor/template","description.svg","dark-invert"
"Home Assistant","components/sensor/homeassistant","home-assistant.svg","dark-invert"
"MQTT Subscribe","components/sensor/mqtt_subscribe","mqtt.png",""
"Uptime Sensor","components/sensor/uptime","timer.svg","dark-invert"
"WiFi Signal Strength","components/sensor/wifi_signal","network-wifi.svg","dark-invert"
{{< /imgtable >}}

### Air Quality

{{< imgtable >}}
"AGS10","components/sensor/ags10","ags10.jpg","Volatile organics","",""
"AirThings BLE","components/sensor/airthings_ble","airthings_logo.png","Radon","CO₂","Volatile organics"
"CCS811","components/sensor/ccs811","ccs811.jpg","eCO₂ & Volatile organics","",""
"CM1106","components/sensor/cm1106","cm1106.png","CO₂","",""
"EE895","components/sensor/ee895","EE895.png","CO₂ & Temperature & Pressure","",""
"ENS160","components/sensor/ens160","ens160.jpg","eCO₂ & Air Quality","",""
"GCJA5","components/sensor/gcja5","gcja5.svg","Particulate","",""
"GP2Y1010AU0F","components/sensor/gp2y1010au0f","gp2y1010au0f.png","Particulate","",""
"Grove Multichannel Gas V2","components/sensor/grove_gas_mc_v2","grove-gas-mc-v2.png","NO₂ & CO & Ethanol & Volatile organics","",""
"HC8","components/sensor/hc8","hc8.png","CO₂","",""
"HM3301","components/sensor/hm3301","hm3301.jpg","Particulate","",""
"iAQ-Core","components/sensor/iaqcore","iaqcore.jpg","eCO₂ & Volatile organics","",""
"MH-Z19","components/sensor/mhz19","mhz19.jpg","CO₂ & Temperature","",""
"MiCS-4514","components/sensor/mics_4514","mics_4514.jpg","NO₂ & CO & H₂ & Ethanol & Methane & Ammonia","",""
"PM1006 Sensor","components/sensor/pm1006","pm1006.jpg","Particulate","",""
"PM2005 Sensor","components/sensor/pm2005","pm2005.png","Particulate","",""
"PMSA003I","components/sensor/pmsa003i","pmsa003i.jpg","Particulate","",""
"PMSX003","components/sensor/pmsx003","pmsx003.svg","Particulate","",""
"RadonEye BLE","components/sensor/radon_eye_ble","radon_eye_logo.png","Radon","",""
"SCD30","components/sensor/scd30","scd30.jpg","CO₂ & Temperature & Humidity","",""
"SCD4X","components/sensor/scd4x","scd4x.jpg","CO₂ & Temperature & Humidity","",""
"SDS011 Sensor","components/sensor/sds011","sds011.jpg","Particulate","",""
"SEN0321","components/sensor/sen0321","sen0321.jpg","Ozone","",""
"SEN5x","components/sensor/sen5x","sen54.jpg","Particulate & Volatile organics & NOx & Temperature & Humidity","",""
"SenseAir","components/sensor/senseair","senseair_s8.jpg","CO₂","",""
"SFA30","components/sensor/sfa30","sfa30.jpg","Formaldehyde","",""
"SGP30","components/sensor/sgp30","sgp30.jpg","eCO₂ & Volatile organics","",""
"SGP4x","components/sensor/sgp4x","sgp40.jpg","Volatile organics & NOx","",""
"SM300D2","components/sensor/sm300d2","sm300d2.jpg","Particulate & Volatile organics & eCO₂ & equivalent Formaldehyde & Temperature & Humidity","",""
"SPS30","components/sensor/sps30","sps30.jpg","Particulate","",""
"T6613/15","components/sensor/t6615","t6615.jpg","CO₂","",""
"ZyAura","components/sensor/zyaura","zgm053.jpg","CO₂ & Temperature & Humidity","",""
{{< /imgtable >}}

### Analogue

{{< imgtable >}}
"ADC","components/sensor/adc","flash.svg","ESP internal","dark-invert"
"ADC128S102","components/sensor/adc128s102","adc128s102.png","8-channel ADC",""
"ADS1115","components/sensor/ads1115","ads1115.jpg","4-channel ADC",""
"ADS1118","components/sensor/ads1118","ads1118.jpg","4-channel ADC",""
"CD74HC4067","components/sensor/cd74hc4067","cd74hc4067.jpg","16-channel analog multiplexer",""
"MCP3008","components/sensor/mcp3008","mcp3008.jpg","8-channel ADC",""
"MCP3204 / MCP3208","components/sensor/mcp3204","mcp3204.jpg","4-channel ADC",""
"MCP3221","components/sensor/mcp3221","mcp3221.png","ADC",""
"NAU7802","components/sensor/nau7802","nau7802.jpg","ADC",""
"Resistance","components/sensor/resistance","omega.svg","dark-invert",""
{{< /imgtable >}}

### Bluetooth Low Energy (BLE)

{{< imgtable >}}
"Alpha3","components/sensor/alpha3","alpha3.jpg",""
"AM43","components/sensor/am43","am43.jpg","Lux & Battery level"
"BLE Client Sensor","components/sensor/ble_client","bluetooth.svg","dark-invert"
"BLE RSSI","components/sensor/ble_rssi","bluetooth.svg","dark-invert"
"HHCCJCY10 (MiFlora Pink)","components/sensor/xiaomi_hhccjcy10","xiaomi_hhccjcy10.jpg","Soil moisture & Temperature & Light"
"Inkbird IBS-TH1 Mini","components/sensor/inkbird_ibsth1_mini","inkbird_isbth1_mini.jpg","Temperature & Humidity"
"Mopeka Pro Check LP","components/sensor/mopeka_pro_check","mopeka_pro_check.jpg","Tank level"
"Mopeka Standard Check LP","components/sensor/mopeka_std_check","mopeka_std_check.jpg","Tank level"
"RuuviTag","components/sensor/ruuvitag","ruuvitag.jpg","Temperature & Humidity & Accelerometer"
"ThermoPro BLE","components/sensor/thermopro_ble","thermopro_tp357.jpg","Temperature & Humidity"
"Xiaomi BLE","components/sensor/xiaomi_ble","xiaomi_mijia_logo.jpg","Various"
{{< /imgtable >}}

### Digital Signals

{{< imgtable >}}
"Duty Cycle","components/sensor/duty_cycle","percent.svg","dark-invert"
"Pulse Counter","components/sensor/pulse_counter","pulse.svg","dark-invert"
"Pulse Meter","components/sensor/pulse_meter","pulse.svg","dark-invert"
"Pulse Width","components/sensor/pulse_width","pulse.svg","dark-invert"
{{< /imgtable >}}

### Distance

{{< imgtable >}}
"A01NYUB","components/sensor/a01nyub","a01nyub.jpg","Acoustic distance"
"A02YYUW","components/sensor/a02yyuw","a02yyuw.jpg","Acoustic distance"
"GL-R01 Time of Flight Sensor","components/sensor/gl_r01","gl_r01.jpg","IR optical distance"
"HRXL MaxSonar WR","components/sensor/hrxl_maxsonar_wr","hrxl_maxsonar_wr.jpg","Acoustic distance"
"JSN-SR04T","components/sensor/jsn_sr04t","jsn-sr04t-v3.jpg","Acoustic distance"
"TOF10120","components/sensor/tof10120","tof10120.jpg","IR optical distance"
"Ultrasonic Sensor","components/sensor/ultrasonic","ultrasonic.jpg","Acoustic distance"
"VL53L0x","components/sensor/vl53l0x","vl53l0x.jpg","IR optical distance"
"Zio Ultrasonic Sensor","components/sensor/zio_ultrasonic","zio_ultrasonic.jpg","Acoustic distance"
{{< /imgtable >}}

### Electricity

{{< imgtable >}}
"ADE7880","components/sensor/ade7880","ade7880.svg","Voltage & Current & Power"
"ADE7953","components/sensor/ade7953","ade7953.svg","Power"
"ATM90E26","components/sensor/atm90e26","atm90e26.jpg","Voltage & Current & Power"
"ATM90E32","components/sensor/atm90e32","atm90e32.jpg","Voltage & Current & Power"
"BL0906","components/sensor/bl0906","bl0906.png","Voltage & Current & Power & Energy"
"BL0939","components/sensor/bl0939","bl0939.png","Voltage & Current & Power & Energy"
"BL0940","components/sensor/bl0940","bl0940.png","Voltage & Current & Power & Energy"
"BL0942","components/sensor/bl0942","bl0942.png","Voltage & Current & Power"
"CS5460A","components/sensor/cs5460a","cs5460a.png","Voltage & Current & Power"
"CSE7761","components/sensor/cse7761","cse7761.svg","Voltage & Current & Power"
"CSE7766","components/sensor/cse7766","cse7766.svg","Voltage & Current & Power"
"CT Clamp","components/sensor/ct_clamp","ct_clamp.jpg","AC Current"
"Daly BMS","components/sensor/daly_bms","daly_bms.jpg","Voltage & Current & Power"
"DSMR","components/sensor/dsmr","dsmr.svg","Electrical counter"
"HLW8012","components/sensor/hlw8012","hlw8012.svg","Voltage & Current & Power"
"HLW8032","components/sensor/hlw8032","hlw8032.png","Voltage & Current & Power"
"INA219","components/sensor/ina219","ina219.jpg","DC Current"
"INA226","components/sensor/ina226","ina226.jpg","DC Current & Power"
"INA228","components/sensor/ina2xx","ina228.jpg","DC Voltage & Current & Power & Charge"
"INA229","components/sensor/ina2xx","ina2xx.jpg","DC Voltage & Current & Power & Charge"
"INA237","components/sensor/ina2xx","ina2xx.jpg","DC Voltage & Current & Power"
"INA238","components/sensor/ina2xx","ina2xx.jpg","DC Voltage & Current & Power"
"INA239","components/sensor/ina2xx","ina2xx.jpg","DC Voltage & Current & Power"
"INA260","components/sensor/ina260","ina260.jpg","DC Current & Power"
"INA3221","components/sensor/ina3221","ina3221.jpg","3-Ch DC current"
"Kamstrup KMP","components/sensor/kamstrup_kmp","kamstrup_kmp.jpg","District Heating Meter"
"MAX9611","components/sensor/max9611","max9611.jpg","Voltage & Current & Power & Temperature"
"PZEM AC","components/sensor/pzemac","pzem-ac.jpg","Voltage & Current & Power"
"PZEM DC","components/sensor/pzemdc","pzem-dc.jpg","Voltage & Current & Power"
"PZEM004T","components/sensor/pzem004t","pzem004t.svg","Voltage & Current & Power"
"SDM Meter","components/sensor/sdm_meter","sdm220m.jpg","Modbus energy monitor"
"Selec Meter","components/sensor/selec_meter","selec_meter_em2m.jpg","Modbus energy monitor"
"Teleinfo","components/sensor/teleinfo","teleinfo.jpg","Electrical counter"
"Total Daily Energy","components/sensor/total_daily_energy","sigma.svg","dark-invert"
{{< /imgtable >}}

### Environmental

{{< imgtable >}}
"Absolute Humidity","components/sensor/absolute_humidity","water-drop.svg","dark-invert",""
"AHT10 / AHT20 / AHT21 / DHT20","components/sensor/aht10","aht10.jpg","Temperature & Humidity",""
"AirThings BLE","components/sensor/airthings_ble","airthings_logo.png","Temperature & Humidity & Pressure",""
"AM2315C","components/sensor/am2315c","am2315c.jpg","Temperature & Humidity",""
"AM2320","components/sensor/am2320","am2320.jpg","Temperature & Humidity",""
"b-parasite","components/sensor/b_parasite","b_parasite.jpg","Moisture & Temperature & Humidity & Light",""
"BH1900NUX","components/sensor/bh1900nux","bh1900nux-evk-001.png","Temperature",""
"BME280","components/sensor/bme280","bme280.jpg","Temperature & Humidity & Pressure",""
"BME68x via BSEC2","components/sensor/bme68x_bsec2","bme680.jpg","Temperature & Humidity & Pressure & Gas",""
"BME680 via BSEC","components/sensor/bme680_bsec","bme680.jpg","Temperature & Humidity & Pressure & Gas",""
"BME680","components/sensor/bme680","bme680.jpg","Temperature & Humidity & Pressure & Gas",""
"BMP085","components/sensor/bmp085","bmp180.jpg","Temperature & Pressure",""
"BMP280","components/sensor/bmp280","bmp280.jpg","Temperature & Pressure",""
"BMP388 and BMP390","components/sensor/bmp3xx","bmp388.jpg","Temperature & Pressure",""
"BMP581","components/sensor/bmp581","bmp581.jpg","Temperature & Pressure",""
"Dallas DS18B20","components/sensor/dallas_temp","dallas.jpg","Temperature",""
"DHT","components/sensor/dht","dht.jpg","Temperature & Humidity",""
"DHT12","components/sensor/dht12","dht12.jpg","Temperature & Humidity",""
"DPS310","components/sensor/dps310","dps310.jpg","Temperature & Pressure",""
"EMC2101","components/emc2101","emc2101.jpg","Temperature",""
"ENS160","components/sensor/ens160","ens160.jpg","eCO₂ & Air Quality",""
"ENS210","components/sensor/ens210","ens210.jpg","Temperature & Humidity",""
"HC8","components/sensor/hc8","hc8.png","CO₂",""
"HDC1080","components/sensor/hdc1080","hdc1080.jpg","Temperature & Humidity",""
"HDC2010","components/sensor/hdc2010","hdc2010.png","Temperature & Humidity",""
"HHCCJCY10 (MiFlora Pink)","components/sensor/xiaomi_hhccjcy10","xiaomi_hhccjcy10.jpg","Soil moisture & Temperature & Light",""
"Honeywell ABP","components/sensor/honeywellabp","honeywellabp.jpg","Pressure & Temperature",""
"Honeywell ABP2 I2C","components/sensor/honeywellabp2_i2c","honeywellabp.jpg","Pressure & Temperature",""
"Honeywell HIH I2C","components/sensor/honeywell_hih_i2c","honeywellhih.jpg","Temperature & Humidity",""
"HTE501","components/sensor/hte501","HTE501.png","Temperature & Humidity",""
"HTU21D / Si7021 / SHT21","components/sensor/htu21d","htu21d.jpg","Temperature & Humidity",""
"HTU31D","components/sensor/htu31d","htu31d.jpg","Temperature & Humidity",""
"Hydreon Rain Sensor","components/sensor/hydreon_rgxx","hydreon_rg9.jpg","Rain",""
"HYT271","components/sensor/hyt271","hyt271.jpg","Temperature & Humidity",""
"Inkbird IBS-TH1 Mini","components/sensor/inkbird_ibsth1_mini","inkbird_isbth1_mini.jpg","Temperature & Humidity",""
"Internal Temperature","components/sensor/internal_temperature","thermometer.svg","Temperature","dark-invert"
"LM75B","components/sensor/lm75b","lm75b.jpg","Temperature",""
"LPS22","components/sensor/lps22","lps22.webp","Temperature & Barometric Pressure",""
"MCP9808","components/sensor/mcp9808","mcp9808.jpg","Temperature",""
"MH-Z19","components/sensor/mhz19","mhz19.jpg","CO₂ & Temperature",""
"MLX90614","components/sensor/mlx90614","mlx90614.jpg","Temperature",""
"MPL3115A2","components/sensor/mpl3115a2","mpl3115a2.jpg","Temperature & Pressure",""
"MS5611","components/sensor/ms5611","ms5611.jpg","Pressure",""
"MS8607","components/sensor/ms8607","ms8607.jpg","Temperature & Humidity & Pressure",""
"NPI-19","components/sensor/npi19","npi19.jpg","Pressure",""
"NTC Thermistor","components/sensor/ntc","ntc.jpg","Temperature",""
"PMWCS3","components/sensor/pmwcs3","pmwcs3.jpg","Soil moisture & Temperature",""
"QMP6988","components/sensor/qmp6988","qmp6988_env3.png","Temperature & Pressure",""
"RadonEye BLE","components/sensor/radon_eye_ble","radon_eye_logo.png","Radon",""
"RuuviTag","components/sensor/ruuvitag","ruuvitag.jpg","Temperature & Humidity & Accelerometer",""
"SCD30","components/sensor/scd30","scd30.jpg","CO₂ & Temperature & Humidity",""
"SCD4X","components/sensor/scd4x","scd4x.jpg","CO₂ & Temperature & Humidity",""
"SDP3x / SDP800 Series","components/sensor/sdp3x","sdp31.jpg","Pressure",""
"SFA30","components/sensor/sfa30","sfa30.jpg","Formaldehyde",""
"SHT3X-D","components/sensor/sht3xd","sht3xd.jpg","Temperature & Humidity",""
"SHT4X","components/sensor/sht4x","sht4x.jpg","Temperature & Humidity",""
"SHTCx","components/sensor/shtcx","shtc3.jpg","Temperature & Humidity",""
"SMT100","components/sensor/smt100","smt100.jpg","Moisture & Temperature",""
"STS3X","components/sensor/sts3x","sts3x.jpg","Temperature",""
"STTS22H","components/sensor/stts22h","stts22h.jpg","Temperature",""
"TC74","components/sensor/tc74","tc74.jpg","Temperature",""
"TEE501","components/sensor/tee501","TEE501.png","Temperature",""
"TE-M3200","components/sensor/tem3200","tem3200.jpg","Temperature & Pressure",""
"TMP102","components/sensor/tmp102","tmp102.jpg","Temperature",""
"TMP1075","components/sensor/tmp1075","tmp1075.jpg","Temperature",""
"TMP117","components/sensor/tmp117","tmp117.jpg","Temperature",""
"WTS01","components/sensor/wts01","wts01.png","Temperature",""
"XGZP68xx Series","components/sensor/xgzp68xx","6897d.jpg","Differential Pressure",""
{{< /imgtable >}}

### Health/Safety

{{< imgtable >}}
"Seeed Studio MR60BHA2 mmWave","components/seeed_mr60bha2","seeed_mr60bha2.jpg","Breathing & heartbeat detection"
"Seeed Studio MR60FDA2 mmWave","components/seeed_mr60fda2","seeed_mr60fda2.jpg","Presence & Fall detection"
{{< /imgtable >}}

### Light

{{< imgtable >}}
"AM43","components/sensor/am43","am43.jpg","Lux"
"APDS9306","components/sensor/apds9306","apds9306.png","Lux"
"APDS9960","components/sensor/apds9960","apds9960.jpg","Colour & Gesture"
"AS7341","components/sensor/as7341","as7341.jpg","Spectral Color Sensor"
"BH1750","components/sensor/bh1750","bh1750.jpg","Lux"
"LTR301","components/sensor/ltr501","ltr501.jpg","Lux"
"LTR303","components/sensor/ltr_als_ps","ltr303.jpg","Lux"
"LTR329","components/sensor/ltr_als_ps","ltr329.jpg","Lux"
"LTR390","components/sensor/ltr390","ltr390.jpg","Lux & UV"
"LTR501","components/sensor/ltr501","ltr501.jpg","Lux & Proximity"
"LTR553","components/sensor/ltr_als_ps","ltr-ps.jpg","Lux & Proximity"
"LTR556","components/sensor/ltr_als_ps","ltr-ps.jpg","Lux & Proximity"
"LTR558","components/sensor/ltr501","ltr501.jpg","Lux & Proximity"
"LTR559","components/sensor/ltr_als_ps","ltr559.jpg","Lux & Proximity"
"LTR659","components/sensor/ltr_als_ps","ltr-ps.jpg","Proximity"
"MAX44009","components/sensor/max44009","max44009.svg","Lux"
"OPT3001","components/sensor/opt3001","opt3001.jpg","Lux"
"TCS34725","components/sensor/tcs34725","tcs34725.jpg","Lux & RGB colour"
"TSL2561","components/sensor/tsl2561","tsl2561.jpg","Lux"
"TSL2591","components/sensor/tsl2591","tsl2591.jpg","Lux"
"VEML3235","components/sensor/veml3235","veml3235.jpg","Lux"
"VEML6030","components/sensor/veml7700","veml6030.jpg","Lux"
"VEML7700","components/sensor/veml7700","veml7700.jpg","Lux"
{{< /imgtable >}}

### Magnetic

{{< imgtable >}}
"AS5600","components/sensor/as5600","as5600.jpg","12-Bit Magnetic Position Sensor"
"HMC5883L","components/sensor/hmc5883l","hmc5883l.jpg","3-Axis magnetometer"
"MLX90393","components/sensor/mlx90393","mlx90393.jpg","3-Axis magnetometer"
"MMC5603","components/sensor/mmc5603","mmc5603.jpg","3-Axis magnetometer"
"MMC5983","components/sensor/mmc5983","mmc5983.jpg","3-Axis magnetometer"
"QMC5883L","components/sensor/qmc5883l","qmc5883l.jpg","3-Axis magnetometer"
{{< /imgtable >}}

### Miscellaneous

{{< imgtable >}}
"AS3935","components/sensor/as3935","as3935.jpg","Storm lightning"
"b-parasite","components/sensor/b_parasite","b_parasite.jpg","Moisture & Temperature & Humidity & Light"
"Binary Sensor Map","components/sensor/binary_sensor_map","binary_sensor_map.jpg","Map binary to value"
"Combination","components/sensor/combination","function.svg","dark-invert"
"Duty Time","components/sensor/duty_time","timer-play-outline.svg","dark-invert"
"EZO sensor circuits","components/sensor/ezo","ezo-ph-circuit.png","(pH)"
"FS3000","components/sensor/fs3000","fs3000.jpg","Air velocity"
"GDK101","components/sensor/gdk101","gdk101.jpg","Radiation"
"Growatt Solar","components/sensor/growatt_solar","growatt.jpg","Solar rooftop"
"Havells Solar","components/sensor/havells_solar","havellsgti5000d_s.jpg","Solar rooftop"
"Integration","components/sensor/integration","sigma.svg","dark-invert"
"Kuntze pool sensor","components/sensor/kuntze","kuntze.jpg",""
"LC709203F","components/sensor/lc709203f","lc709203f.jpg","Battery level & Thermistor"
"LVGL widget","components/sensor/lvgl","lvgl_c_num.png",""
"M5Stack Unit 8 Angle","components/sensor/m5stack_8angle","m5stack_8angle.png",""
"MAX17043","components/sensor/max17043","max17043.jpg","Battery level"
"MicroNova pellet stove","components/micronova","micronova.svg",""
"Modbus Sensor","components/sensor/modbus_controller","modbus.png",""
"Nextion","components/sensor/nextion","nextion.jpg","Sensors from display"
"Person Sensor (SEN21231)","components/sensor/sen21231","sen21231.png",""
"Resol VBus","components/vbus","resol_deltasol_bs_plus.jpg",""
"Rotary Encoder","components/sensor/rotary_encoder","rotary_encoder.jpg",""
"SMT100","components/sensor/smt100","smt100.jpg","Moisture & Temperature"
"Sound Level","components/sensor/sound_level","waveform.svg","dark-invert"
"Tuya Sensor","components/sensor/tuya","tuya.png",""
"TX20","components/sensor/tx20","tx20.jpg","Wind speed & Wind direction"
"uFire EC sensor","components/sensor/ufire_ec","ufire_ec.png","EC & Temperature"
"uFire ISE sensor","components/sensor/ufire_ise","ufire_ise.png","pH & Temperature"
"WireGuard","components/wireguard","wireguard_custom_logo.svg","dark-invert"
{{< /imgtable >}}

### Motion

{{< imgtable >}}
"APDS9960","components/sensor/apds9960","apds9960.jpg","Colour & Gesture"
"BMI160","components/sensor/bmi160","bmi160.jpg","Accelerometer & Gyroscope"
"LD2410","components/sensor/ld2410","ld2410.jpg","Motion & Presence"
"LD2412","components/sensor/ld2412","ld2412.jpg","Motion & Presence"
"LD2420","components/sensor/ld2420","ld2420.jpg","Motion & Presence"
"LD2450","components/sensor/ld2450","ld2450.png","Motion & Presence"
"MPU6050","components/sensor/mpu6050","mpu6050.jpg","Accelerometer & Gyroscope"
"MPU6886","components/sensor/mpu6886","mpu6886.jpg","Accelerometer & Gyroscope"
"MSA301","components/sensor/msa3xx","msa301.jpg","Accelerometer"
"MSA311","components/sensor/msa3xx","msa311.jpg","Accelerometer"
"RuuviTag","components/sensor/ruuvitag","ruuvitag.jpg","Temperature & Humidity & Accelerometer"
"Seeed Studio MR24HPC1 mmWave","components/seeed_mr24hpc1","seeed-mr24hpc1.jpg","Motion & Presence"
{{< /imgtable >}}

### Thermocouple

{{< imgtable >}}
"KMeterISO","components/sensor/kmeteriso","kmeteriso.jpg","K-Type",""
"MAX31855","components/sensor/max31855","max31855.jpg","K-Type",""
"MAX31856","components/sensor/max31856","max31856.jpg","All types",""
"MAX31865","components/sensor/max31865","max31865.jpg","Platinum RTD",""
"MAX6675","components/sensor/max6675","max6675.jpg","K-Type",""
"MCP9600","components/sensor/mcp9600","mcp9600.jpg","All types",""
{{< /imgtable >}}

### Weight

{{< imgtable >}}
"HX711","components/sensor/hx711","hx711.jpg","Load cell amplifier"
"Xiaomi Miscale","components/sensor/xiaomi_miscale","xiaomi_miscale1&2.jpg",""
{{< /imgtable >}}
Looking for a sensor that outputs its values as an analog voltage? Have a look at the
{{< docref "/components/sensor/adc" "ADC Sensor" >}} together with a formula like in the
[TEMT6000 configuration](https://devices.esphome.io/devices/temt6000).

## Binary Sensor Components

Binary Sensors are organized into categories; if a given sensor fits into more than one category, it will appear
multiple times.

### Core

{{< imgtable >}}
"Binary Sensor Core","components/binary_sensor/index","folder-open.svg","dark-invert"
"Template Binary Sensor","components/binary_sensor/template","description.svg","dark-invert"
"GPIO","components/binary_sensor/gpio","gpio.svg",""
"Home Assistant","components/binary_sensor/homeassistant","home-assistant.svg","dark-invert"
"Status","components/binary_sensor/status","server-network.svg","dark-invert"
"Switch","components/binary_sensor/switch","electric-switch.svg","dark-invert"
"Host SDL2","components/binary_sensor/sdl","sdl.png",""
{{< /imgtable >}}

### Capacitive Touch

{{< imgtable >}}
"CAP1188 Capacitive Touch Sensor","components/binary_sensor/cap1188","cap1188.jpg",""
"ESP32 Touch Pad","components/binary_sensor/esp32_touch","touch.svg","dark-invert"
"MPR121 Capacitive Touch Sensor","components/binary_sensor/mpr121","mpr121.jpg",""
"TTP229","components/binary_sensor/ttp229","ttp229.jpg",""
{{< /imgtable >}}

### Mechanical

{{< imgtable >}}
"Matrix Keypad","components/matrix_keypad","matrix_keypad.jpg"
"TM1637","components/display/tm1637","tm1637.jpg"
"TM1638","components/display/tm1638","tm1638.jpg"
{{< /imgtable >}}

### NFC/RFID

Often known as "tag" or "card" readers within the community.

{{< imgtable >}}
"NFC Tag","components/binary_sensor/nfc","nfc.png","dark-invert"
"PN532","components/binary_sensor/pn532","pn532.jpg",""
"PN7150","components/pn7150","pn7150.jpg",""
"PN716X","components/pn7160","pn716x.jpg",""
"RC522","components/binary_sensor/rc522","rc522.jpg",""
"RDM6300","components/binary_sensor/rdm6300","rdm6300.jpg",""
"Wiegand Reader","components/wiegand","wiegand.jpg",""
{{< /imgtable >}}

### Touchscreen

{{< imgtable >}}
"Touchscreen Core","components/touchscreen/index","touch.svg","dark-invert"
"FT5X06","components/touchscreen/ft5x06","indicator.jpg",""
"GT911","components/touchscreen/gt911","esp32_s3_box_3.png",""
"Nextion Binary Sensor","components/binary_sensor/nextion","nextion.jpg",""
"TT21100","components/touchscreen/tt21100","esp32-s3-korvo-2-lcd.png",""
"LVGL widget","components/binary_sensor/lvgl","lvgl_c_bns.png",""
{{< /imgtable >}}

### Presence Detection

{{< imgtable >}}
"AT581X","components/at581x","at581x.png"
"DFRobot mmWave Radar","components/dfrobot_sen0395","dfrobot_sen0395.jpg"
"LD2410","components/sensor/ld2410","ld2410.jpg"
"LD2412","components/sensor/ld2412","ld2412.jpg"
"LD2420","components/sensor/ld2420","ld2420.jpg"
"LD2450","components/sensor/ld2450","ld2450.png"
"Seeed Studio MR24HPC1 mmWave","components/seeed_mr24hpc1","seeed-mr24hpc1.jpg"
{{< /imgtable >}}

### Miscellaneous

{{< imgtable >}}
"Analog Threshold","components/binary_sensor/analog_threshold","analog_threshold.svg","dark-invert"
"ESP32 BLE Presence","components/binary_sensor/ble_presence","bluetooth.svg","dark-invert"
"Hydreon Rain Sensor Binary Sensor","components/binary_sensor/hydreon_rgxx","hydreon_rg9.jpg",""
"Modbus Binary Sensor","components/binary_sensor/modbus_controller","modbus.png",""
"PipSolar - compatible PV Inverter","components/pipsolar","pipsolar.jpg",""
"Pylontech Batteries","components/pylontech","pylontech.jpg",""
"Qwiic PIR Motion","components/binary_sensor/qwiic_pir","qwiic_pir.jpg",""
"Resol VBus","components/vbus","resol_deltasol_bs_plus.jpg",""
"Tuya Binary Sensor","components/binary_sensor/tuya","tuya.png",""
"WireGuard","components/wireguard","wireguard_custom_logo.svg","dark-invert"
{{< /imgtable >}}

## Alarm Control Panel Components

{{< imgtable >}}
"Alarm Control Panel Core","components/alarm_control_panel/index","alarm-panel.svg","dark-invert"
"Template Alarm Control Panel","components/alarm_control_panel/template","description.svg","dark-invert"
{{< /imgtable >}}

## Audio ADC Components

{{< imgtable >}}
"Audio ADC Core","components/audio_adc/index","audio_adc.svg"
"ES7210","components/audio_adc/es7210","es7210.svg"
"ES7243E","components/audio_adc/es7243e","es7243e.svg"
{{< /imgtable >}}

## Audio DAC Components

{{< imgtable >}}
"Audio DAC Core","components/audio_dac/index","audio_dac.svg"
"AIC3204","components/audio_dac/aic3204","aic3204.svg"
"ES8156","components/audio_dac/es8156","es8156.svg"
"ES8311","components/audio_dac/es8311","es8311.svg"
"ES8388","components/audio_dac/es8388","es8388.svg"
{{< /imgtable >}}

## Button Components

{{< imgtable >}}
"Button Core","components/button/index","folder-open.svg","dark-invert"
"Template Button","components/button/template","description.svg","dark-invert"
"Factory Reset Button","components/button/factory_reset","restart-alert.svg","dark-invert"
"Generic Output Button","components/button/output","upload.svg","dark-invert"
"Restart Button","components/button/restart","restart.svg","dark-invert"
"Safe Mode Button","components/button/safe_mode","restart-alert.svg","dark-invert"
"Shutdown Button","components/button/shutdown","power_settings.svg","dark-invert"
"UART Button","components/button/uart","uart.svg",""
"Wake-on-LAN","components/button/wake_on_lan","power_settings.svg","dark-invert"
{{< /imgtable >}}

## Climate Components

{{< imgtable >}}
"Climate Core","components/climate/index","folder-open.svg","dark-invert"
"Anova Cooker","components/climate/anova","anova.png",""
"Bang Bang Controller","components/climate/bang_bang","air-conditioner.svg","dark-invert"
"BedJet Climate System","components/climate/bedjet","bedjet.png",""
"Haier Climate","components/climate/haier","haier.svg",""
"IR Remote Climate","components/climate/climate_ir","air-conditioner-ir.svg","dark-invert"
"Midea","components/climate/midea","midea.svg",""
"PID Controller","components/climate/pid","function.svg","dark-invert"
"Thermostat Controller","components/climate/thermostat","air-conditioner.svg","dark-invert"
"Tuya Climate","components/climate/tuya","tuya.png",""
"Uponor Smatrix Base Pulse Underfloor Heating","components/uponor_smatrix","uponor.svg",""
{{< /imgtable >}}

## Cover Components

{{< imgtable >}}
"Cover Core","components/cover/index","folder-open.svg","dark-invert"
"Template Cover","components/cover/template","description.svg","dark-invert"
"AM43 Cover","components/cover/am43","am43.jpg",""
"Current-Based Cover","components/cover/current_based","flash.svg","dark-invert"
"Endstop Cover","components/cover/endstop","electric-switch.svg","dark-invert"
"Feedback Cover","components/cover/feedback","feedback_cover.svg","dark-invert"
"HE60R Cover","components/cover/he60r","he60r.jpg",""
"Time-Based Cover","components/cover/time_based","timer.svg","dark-invert"
"Tormatic/Novoferm Cover","components/cover/tormatic","tormatic.png",""
"Tuya Cover","components/cover/tuya","tuya.png",""
{{< /imgtable >}}

## Datetime Components

{{< imgtable >}}
"Datetime Core","components/datetime/index","clock-outline.svg","dark-invert"
"Template Datetime","components/datetime/template","description.svg","dark-invert"
{{< /imgtable >}}

## Display Components

{{< imgtable >}}
"Display Core","components/display/index","folder-open.svg","dark-invert"
"Font Renderer","components/font","format-font.svg","dark-invert"
"Graph","components/graph","chart-line.svg","dark-invert"
"QR Code","components/qr_code","qr-code.svg","dark-invert"
"Image","components/image","image-outline.svg","dark-invert"
"Animation","components/animation","image-multiple-outline.svg","dark-invert"
"Online Image","components/online_image","image-sync-outline.svg","dark-invert"
"Display Menu Core","components/display_menu/index","folder-open.svg","dark-invert"
"Graphical Display Menu","components/display_menu/graphical_display_menu","graphical_display_menu.png",""
"LCD Menu","components/display_menu/lcd_menu","lcd_menu.png",""
"LVGL Graphics","components/lvgl/index","lvgl.png",""
{{< /imgtable >}}
{{< anchor "display-hw" >}}

## Display Hardware Platforms

{{< imgtable >}}
"Addressable Light","components/display/addressable_light","addressable_light.jpg"
"MIPI DSI Displays","components/display/mipi_dsi","tab5.jpg"
"MIPI RGB Displays","components/display/mipi_rgb","indicator.jpg"
"MIPI SPI Displays","components/display/mipi_spi","t4-s3.jpg"
"ePaper SPI Displays","components/display/epaper_spi","epaper.svg"
"ILI9xxx","components/display/ili9xxx","ili9341.jpg"
"ILI9341","components/display/ili9xxx","ili9341.svg"
"ILI9342","components/display/ili9xxx","ili9342.svg"
"ILI9481","components/display/ili9xxx","ili9481.svg"
"ILI9486","components/display/ili9xxx","ili9341.jpg"
"ILI9488","components/display/ili9xxx","ili9488.svg"
"WSPICOLCD","components/display/ili9xxx","ili9488.svg"
"HUB75 LED Matrix","components/display/hub75","hub75.svg"
"Inkplate","components/display/inkplate","inkplate6.jpg"
"LCD Display","components/display/lcd_display","lcd.jpg"
"MAX7219 Dot Matrix","components/display/max7219digit","max7219digit.jpg"
"MAX7219","components/display/max7219","max7219.jpg"
"Nextion","components/display/nextion","nextion.jpg"
"PCD8544 (Nokia 5110/ 3310)","components/display/pcd8544","pcd8544.jpg"
"PVVX MiThermometer","components/display/pvvx_mithermometer","xiaomi_lywsd03mmc.jpg"
"Quad SPI Displays","components/display/qspi_dbi","t4-s3.jpg"
"RPI_DPI_RGB","components/display/rpi_dpi_rgb","waveshare_touch-s3.jpg"
"SSD1306","components/display/ssd1306","ssd1306.jpg"
"SSD1322","components/display/ssd1322","ssd1322.jpg"
"SSD1325","components/display/ssd1325","ssd1325.jpg"
"SSD1327","components/display/ssd1327","ssd1327.jpg"
"SSD1331","components/display/ssd1331","ssd1331.jpg"
"SSD1351","components/display/ssd1351","ssd1351.jpg"
"ST7567","components/display/st7567","st7567.jpg"
"ST7701S","components/display/st7701s","indicator.jpg"
"ST7735","components/display/st7735","st7735.jpg"
"ST7789V","components/display/st7789v","st7789v.jpg"
"ST7796","components/display/ili9xxx","st7796.svg"
"ST7920","components/display/st7920","st7920.jpg"
"TM1621","components/display/tm1621","tm1621.jpg"
"TM1637","components/display/tm1637","tm1637.jpg"
"TM1638","components/display/tm1638","tm1638.jpg"
"TM1651 Battery Display","components/tm1651","tm1651_battery_display.jpg"
"Waveshare E-Paper","components/display/waveshare_epaper","waveshare_epaper.jpg"
"Host SDL2 display","components/display/sdl","sdl.png"
{{< /imgtable >}}

## Electromechanical

{{< imgtable >}}
"Atlas Scientific Peristaltic Pump","components/ezo_pmp","ezo-pmp.jpg",""
"Grove TB6612FNG","components/grove_tb6612fng","motor.png","dark-invert"
"Matrix Keypad","components/matrix_keypad","matrix_keypad.jpg",""
"RTTTL Buzzer","components/rtttl","buzzer.jpg",""
"Servo","components/servo","servo.svg",""
"Stepper","components/stepper/index","stepper.svg",""
{{< /imgtable >}}

## Energy/Solar Management

{{< imgtable >}}
"Growatt Solar","components/sensor/growatt_solar","growatt.jpg",""
"Havells Solar","components/sensor/havells_solar","havellsgti5000d_s.jpg",""
"PipSolar-compatible PV Inverter","components/pipsolar","pipsolar.jpg",""
"Power Supply","components/power_supply","power.svg","dark-invert"
"Resol VBus","components/vbus","resol_deltasol_bs_plus.jpg",""
"SML","components/sml","sml.svg",""
"SUN-GTIL2 inverter","components/sun_gtil2","sun_1000g2.png",""
{{< /imgtable >}}

## Event Components

{{< imgtable >}}
"Event Core","components/event/index","folder-open.svg","dark-invert"
"Template Event","components/event/template","description.svg","dark-invert"
{{< /imgtable >}}

## Fan Components

{{< imgtable >}}
"Fan Core","components/fan/index","folder-open.svg","dark-invert"
"Template Fan","components/fan/template","description.svg","dark-invert"
"Binary Fan","components/fan/binary","fan.svg","dark-invert"
"H-bridge Fan","components/fan/hbridge","fan.svg","dark-invert"
"Speed Fan","components/fan/speed","fan.svg","dark-invert"
"Tuya Fan","components/fan/tuya","tuya.png",""
{{< /imgtable >}}

## Home Assistant Components

Components specifically for interacting with Home Assistant.

{{< imgtable >}}
"Binary Sensor","components/binary_sensor/homeassistant","home-assistant.svg","dark-invert"
"Bluetooth Proxy","components/bluetooth_proxy","bluetooth.svg","dark-invert"
"micro Wake Word","components/micro_wake_word","voice-assistant.svg","dark-invert"
"Number","components/number/homeassistant","home-assistant.svg","dark-invert"
"Sensor","components/sensor/homeassistant","home-assistant.svg","dark-invert"
"Switch","components/switch/homeassistant","home-assistant.svg","dark-invert"
"Text Sensor","components/text_sensor/homeassistant","home-assistant.svg","dark-invert"
"Voice Assistant","components/voice_assistant","voice-assistant.svg","dark-invert"
{{< /imgtable >}}

## Light Components

{{< imgtable >}}
"Light Core","components/light/index","folder-open.svg","dark-invert"
"Beken SPI","components/light/beken_spi_led_strip","color_lens.svg","dark-invert"
"Binary Light","components/light/binary","lightbulb.svg","dark-invert"
"Cold+Warm White Light","components/light/cwww","brightness-medium.svg","dark-invert"
"Color Temperature Light","components/light/color_temperature","brightness-medium.svg","dark-invert"
"ESP32 RMT","components/light/esp32_rmt_led_strip","color_lens.svg","dark-invert"
"FastLED Light","components/light/fastled","color_lens.svg","dark-invert"
"H-bridge Light","components/light/hbridge","brightness-medium.svg","dark-invert"
"Light Partition","components/light/partition","color_lens.svg","dark-invert"
"LightWaveRF","components/lightwaverf","brightness-medium.svg","dark-invert"
"LVGL widget","components/light/lvgl","lvgl_c_lig.png",""
"Monochromatic Light","components/light/monochromatic","brightness-medium.svg","dark-invert"
"NeoPixelBus Light","components/light/neopixelbus","color_lens.svg","dark-invert"
"RGB Light","components/light/rgb","rgb.png",""
"RGBCT Light","components/light/rgbct","rgbw.png",""
"RGBW Light","components/light/rgbw","rgbw.png",""
"RGBWW Light","components/light/rgbww","rgbw.png",""
"RP2040 PIO","components/light/rp2040_pio_led_strip","color_lens.svg","dark-invert"
"Shelly Dimmer","components/light/shelly_dimmer","shellydimmer2.jpg",""
"Sonoff D1 Dimmer","components/light/sonoff_d1","sonoff_d1.jpg",""
"SPI LED Strips","components/light/spi_led_strip","apa102.jpg",""
"Status Led","components/light/status_led","led-on.svg","dark-invert"
"Tuya Dimmer","components/light/tuya","tuya.png",""
{{< /imgtable >}}
**Looking for WS2811 and similar individually addressable lights?** For the ESP32 and its variants, we recommend the
{{< docref "light/esp32_rmt_led_strip" >}} or {{< docref "light/spi_led_strip" >}}; for other processors, have a look
at the {{< docref "light/fastled" "FastLED Light" >}}.

## Lock Components

{{< imgtable >}}
"Lock Core","components/lock/index","folder-open.svg","dark-invert"
"Template Lock","components/lock/template","description.svg","dark-invert"
"Generic Output Lock","components/lock/output","upload.svg","dark-invert"
{{< /imgtable >}}

## Media Player Components

{{< imgtable >}}
"Media Player Core","components/media_player/index","folder-open.svg","dark-invert"
"DFPlayer","components/dfplayer","dfplayer.svg","dark-invert"
"I2S Audio","components/media_player/i2s_audio","i2s_audio.svg",""
"Speaker","components/media_player/speaker","speaker.svg","dark-invert"
{{< /imgtable >}}

## Microphone Components

{{< imgtable >}}
"Microphone Core","components/microphone/index","microphone.svg","dark-invert"
"I2S Microphone","components/microphone/i2s_audio","i2s_audio.svg",""
{{< /imgtable >}}

## Number Components

{{< imgtable >}}
"Number Core","components/number/index","folder-open.svg","dark-invert"
"Template Number","components/number/template","description.svg","dark-invert"
"Home Assistant","components/number/homeassistant","home-assistant.svg","dark-invert"
"LVGL widget Number","components/number/lvgl","lvgl_c_num.png",""
"Modbus Number","components/number/modbus_controller","modbus.png",""
"Tuya Number","components/number/tuya","tuya.png",""
{{< /imgtable >}}

## Output Components

{{< imgtable >}}
"Output Core","components/output/index","folder-open.svg","dark-invert"
"Template Output","components/output/template","description.svg","dark-invert"
"AC Dimmer","components/output/ac_dimmer","ac_dimmer.svg","dark-invert"
"BLE Binary Output","components/output/ble_client","bluetooth.svg","dark-invert"
"BP1658CJ","components/output/bp1658cj","bp1658cj.svg",""
"BP5758D","components/output/bp5758d","bp5758d.svg",""
"DAC7678","components/output/dac7678","dac7678.svg",""
"EMC2101","components/emc2101","emc2101.jpg",""
"ESP32 DAC","components/output/esp32_dac","dac.svg",""
"ESP32 LEDC","components/output/ledc","pwm.png",""
"ESP8266 Software PWM","components/output/esp8266_pwm","pwm.png",""
"GP8403","components/output/gp8403","gp8403.svg",""
"GPIO Output","components/output/gpio","gpio.svg",""
"LibreTiny PWM","components/output/libretiny_pwm","pwm.png",""
"MCP4661","components/output/mcp4461","mcp4461.jpg",""
"MCP4725","components/output/mcp4725","mcp4725.jpg",""
"MCP4728","components/output/mcp4728","mcp4728.jpg",""
"MCP47A1","components/output/mcp47a1","mcp47a1.svg",""
"Modbus Output","components/output/modbus_controller","modbus.png",""
"MY9231/MY9291","components/output/my9231","my9231.svg",""
"PCA9685","components/output/pca9685","pca9685.jpg",""
"Sigma-Delta Output","components/output/sigma_delta_output","sigma-delta.svg","dark-invert"
"Slow PWM","components/output/slow_pwm","pwm.png",""
"SM16716","components/output/sm16716","sm16716.svg",""
"SM2135","components/output/sm2135","sm2135.svg",""
"SM2235","components/output/sm2235","sm2235.svg",""
"SM2335","components/output/sm2335","sm2335.svg",""
"TLC59208F","components/output/tlc59208f","tlc59208f.jpg",""
"TLC5947","components/output/tlc5947","tlc5947.jpg",""
"TLC5971","components/output/tlc5971","tlc5971.jpg",""
"X9C Potentiometer","components/output/x9c","x9c.jpg",""
{{< /imgtable >}}

## Select Components

{{< imgtable >}}
"Select Core","components/select/index","folder-open.svg","dark-invert"
"Template Select","components/select/template","description.svg","dark-invert"
"LVGL widget Select","components/select/lvgl","lvgl_c_sel.png",""
"Modbus Select","components/select/modbus_controller","modbus.png",""
"Tuya Select","components/select/tuya","tuya.png",""
{{< /imgtable >}}

## Speaker Components

{{< imgtable >}}
"Speaker Core","components/speaker/index","speaker.svg","dark-invert"
"I2S Speaker","components/speaker/i2s_audio","i2s_audio.svg",""
"Mixer Speaker","components/speaker/mixer","mixer.svg",""
"Resampler Speaker","components/speaker/resampler","waveform.svg","dark-invert"
{{< /imgtable >}}

## Switch Components

{{< imgtable >}}
"Switch Core","components/switch/index","folder-open.svg","dark-invert"
"Template Switch","components/switch/template","description.svg","dark-invert"
"BLE Client Switch","components/switch/ble_client","bluetooth.svg","dark-invert"
"Factory Reset Switch","components/switch/factory_reset","restart-alert.svg","dark-invert"
"Generic Output Switch","components/switch/output","upload.svg","dark-invert"
"GPIO Switch","components/switch/gpio","gpio.svg",""
"H-bridge Switch","components/switch/hbridge","hbridge-relay.jpg",""
"LVGL Widget","components/switch/lvgl","lvgl_c_swi.png",""
"Modbus Switch","components/switch/modbus_controller","modbus.png",""
"Nextion Switch","components/switch/nextion","nextion.jpg",""
"Restart Switch","components/switch/restart","restart.svg","dark-invert"
"Safe Mode Switch","components/switch/safe_mode","restart-alert.svg","dark-invert"
"Shutdown Switch","components/switch/shutdown","power_settings.svg","dark-invert"
"Tuya Switch","components/switch/tuya","tuya.png",""
"UART Switch","components/switch/uart","uart.svg",""
"Home Assistant","components/switch/homeassistant","home-assistant.svg","dark-invert"
{{< /imgtable >}}

## Text Components

{{< imgtable >}}
"Text Core","components/text/index","folder-open.svg","dark-invert"
"Template Text","components/text/template","description.svg","dark-invert"
"LVGL textarea Text","components/text/lvgl","lvgl_c_txt.png",""
{{< /imgtable >}}

## Text Sensor Components

{{< imgtable >}}
"Text Sensor Core","components/text_sensor/index","folder-open.svg","dark-invert"
"Template Text Sensor","components/text_sensor/template","description.svg","dark-invert"
"BLE Scanner","components/text_sensor/ble_scanner","bluetooth.svg","dark-invert"
"Ethernet Info","components/text_sensor/ethernet_info","ethernet.svg","dark-invert"
"Home Assistant","components/text_sensor/homeassistant","home-assistant.svg","dark-invert"
"LibreTiny","components/text_sensor/libretiny","libretiny.svg",""
"LVGL textarea Text Sensor","components/text_sensor/lvgl","lvgl_c_txt.png",""
"Modbus Text Sensor","components/text_sensor/modbus_controller","modbus.png",""
"MQTT Subscribe Text","components/text_sensor/mqtt_subscribe","mqtt.png",""
"Nextion Text Sensor","components/text_sensor/nextion","nextion.jpg",""
"OpenThread Info","components/text_sensor/openthread_info","openthread.png",""
"Tuya Text Sensor","components/text_sensor/tuya","tuya.png",""
"Version","components/text_sensor/version","new-box.svg","dark-invert"
"WiFi Info","components/text_sensor/wifi_info","network-wifi.svg","dark-invert"
"WireGuard","components/wireguard","wireguard_custom_logo.svg","dark-invert"
"WL-134 Pet Tag Sensor","components/text_sensor/wl_134","fingerprint.svg","dark-invert"
{{< /imgtable >}}

## Time Components

{{< imgtable >}}
"Time Core","components/time/index","clock-outline.svg","dark-invert"
"BM8563 RTC","components/time/bm8563","bm8563.svg",""
"DS1307 RTC","components/time/ds1307","clock-outline.svg","dark-invert"
"RX8130 RTC","components/time/rx8130","clock-outline.svg","dark-invert"
"GPS Time","components/time/gps","crosshairs-gps.svg","dark-invert"
"Home Assistant Time","components/time/homeassistant","home-assistant.svg","dark-invert"
"PCF85063 RTC","components/time/pcf85063","clock-outline.svg","dark-invert"
"PCF8563 RTC","components/time/pcf8563","clock-outline.svg","dark-invert"
"SNTP","components/time/sntp","clock-outline.svg","dark-invert"
{{< /imgtable >}}

## Touchscreen Components

{{< imgtable >}}
"Touchscreen Core","components/touchscreen/index","folder-open.svg","dark-invert"
"AXS15231","components/touchscreen/axs15231","axs15231.svg",""
"CST226","components/touchscreen/cst226","t4-s3.jpg",""
"CST816","components/touchscreen/cst816","cst816.jpg",""
"CHSC6X","components/touchscreen/chsc6x","chsc6x.png",""
"EKTF2232","components/touchscreen/ektf2232","ektf2232.svg","Inkplate 6 Plus"
"FT63X6","components/touchscreen/ft63x6","wt32-sc01.png",""
"GT911","components/touchscreen/gt911","esp32_s3_box_3.png",""
"Lilygo T5 4.7""","components/touchscreen/lilygo_t5_47","lilygo_t5_47_touch.jpg",""
"TT21100","components/touchscreen/tt21100","esp32-s3-korvo-2-lcd.png",""
"XPT2046","components/touchscreen/xpt2046","xpt2046.jpg",""
{{< /imgtable >}}

## Valve Components

{{< imgtable >}}
"Valve Core","components/valve/index","folder-open.svg","dark-invert"
"Template Valve","components/valve/template","description.svg","dark-invert"
{{< /imgtable >}}

## Wireless Communication

Used for creating infrared (IR) or radio frequency (RF) remote control transmitters and/or receivers, or to connect
ESPHome to cellular networks. **Does not encompass Wi-Fi.**

{{< imgtable >}}
"CC1101","components/cc1101","cc1101.webp",""
"IR Remote Climate","components/climate/climate_ir","air-conditioner-ir.svg","dark-invert"
"Remote Receiver","components/remote_receiver","remote.svg","dark-invert"
"Remote Transmitter","components/remote_transmitter","remote.svg","dark-invert"
"RF Bridge","components/rf_bridge","rf_bridge.jpg",""
"SIM800L","components/sim800l","sim800l.jpg",""
"SX126x","components/sx126x","sx126x.png",""
"SX127x","components/sx127x","sx127x.png",""
{{< /imgtable >}}

## Miscellaneous Components

{{< imgtable >}}
"Camera Encoder","components/camera/camera_encoder","camera.svg","dark-invert"
"ESP32 Camera","components/esp32_camera","camera.svg","dark-invert"
"Exposure Notifications","components/exposure_notifications","exposure_notifications.png",""
"GPS","components/gps","crosshairs-gps.svg","dark-invert"
"Grow Fingerprint Reader","components/fingerprint_grow","fingerprint.svg","dark-invert"
"HLK-FM22x Face Recognition Module","components/hlk_fm22x","face.svg","dark-invert"
"Modbus Controller","components/modbus_controller","modbus.png",""
"Sprinkler","components/sprinkler","sprinkler-variant.svg","dark-invert"
"Status LED","components/status_led","led-on.svg","dark-invert"
"Sun","components/sun","weather-sunny.svg","dark-invert"
"Tuya MCU","components/tuya","tuya.png",""
"Z-Wave Proxy","components/zwave_proxy","z-wave.svg",""
{{< /imgtable >}}

## Cookbook

{{< imgtable >}}
"Lambda Magic - Tips and Tricks","cookbook/lambda_magic","head-lightbulb-outline.svg","dark-invert"
"LVGL Recipes","cookbook/lvgl","lvgl.png",""
"Garage Door Template Cover","cookbook/garage-door","garage-variant.svg","dark-invert"
"Time & Temperature on OLED Display","cookbook/display_time_temp_oled","display_time_temp_oled_2.jpg",""
"ESP32 Water Leak Detector","cookbook/leak-detector-m5stickC","leak-detector-m5stickC_main_index.jpg",""
"BME280 Environment extras","cookbook/bme280_environment","bme280.jpg",""
"Non-Invasive Power Meter","cookbook/power_meter","power_meter.jpg",""
"Sonoff Fishpond Pump","cookbook/sonoff-fishpond-pump","cookbook-sonoff-fishpond-pump.jpg",""
"Arduino Port Extender","cookbook/arduino_port_extender","arduino_logo.svg",""
"EHMTX a matrix status/text display","cookbook/ehmtx","ehmtx.jpg",""
"Pulse Catcher","cookbook/pulse-catcher","pulses.png",""
{{< /imgtable >}}

## Contributing

ESPHome depends on and welcomes contributions from our community. If you'd like to contribute, please see our
[developer site](https://developers.esphome.io).
