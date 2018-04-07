FROM python:2.7
MAINTAINER Otto Winter <contact@otto-winter.com>

ENV ESPHOMEYAML_OTA_HOST_PORT=6123
EXPOSE 6123
VOLUME /config
WORKDIR /usr/src/app

COPY requirements.txt /usr/src/app/
RUN pip install --no-cache-dir -r requirements.txt

COPY docker/platformio.ini /usr/src/app/
RUN platformio settings set enable_telemetry No && \
    platformio lib --global install esphomelib=https://github.com/OttoWinter/esphomelib.git#v1.2.1 && \
    platformio run -e espressif32 -e espressif8266; exit 0

# Fix issue with static IP on ESP32: https://github.com/espressif/arduino-esp32/issues/1081
RUN curl https://github.com/espressif/arduino-esp32/commit/144480637a718844b8f48f4392da8d4f622f2e5e.patch | \
    patch /root/.platformio/packages/framework-arduinoespressif32/libraries/WiFi/src/WiFiGeneric.cpp

COPY . .
RUN pip install -e .

WORKDIR /config
ENTRYPOINT ["esphomeyaml"]
