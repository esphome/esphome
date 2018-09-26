FROM python:2.7
MAINTAINER Otto Winter <contact@otto-winter.com>

RUN apt-get update && apt-get install -y \
        python-pil \
    && rm -rf /var/lib/apt/lists/*

ENV ESPHOMEYAML_OTA_HOST_PORT=6123
EXPOSE 6123
VOLUME /config
WORKDIR /usr/src/app

RUN pip install --no-cache-dir --no-binary :all: platformio && \
    platformio settings set enable_telemetry No

COPY docker/platformio.ini /usr/src/app/
RUN platformio settings set enable_telemetry No && \
    platformio run -e espressif32 -e espressif8266; exit 0

COPY . .
RUN pip install --no-cache-dir -e . && \
    pip install --no-cache-dir tzlocal pillow

WORKDIR /config
ENTRYPOINT ["esphomeyaml"]
