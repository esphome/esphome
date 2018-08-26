FROM python:2.7
MAINTAINER Otto Winter <contact@otto-winter.com>

RUN apt-get update && apt-get install -y \
        python-pil \
    && rm -rf /var/lib/apt/lists/*

ENV ESPHOMEYAML_OTA_HOST_PORT=6123
EXPOSE 6123
VOLUME /config
WORKDIR /usr/src/app

COPY requirements.txt /usr/src/app/
RUN pip install --no-cache-dir -r requirements.txt && \
    pip install --no-cache-dir tornado esptool

COPY docker/platformio.ini /usr/src/app/
RUN platformio settings set enable_telemetry No && \
    platformio run -e espressif32 -e espressif8266; exit 0

COPY . .
RUN pip install -e . && \
    pip install tzlocal

WORKDIR /config
ENTRYPOINT ["esphomeyaml"]
