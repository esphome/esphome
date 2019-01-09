ARG BUILD_FROM=python:2.7
FROM ${BUILD_FROM}
MAINTAINER Otto Winter <contact@otto-winter.com>

RUN apt-get update && apt-get install -y \
        python-pil \
        git \
    && apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* && \
    pip install --no-cache-dir --no-binary :all: platformio && \
    platformio settings set enable_telemetry No && \
    platformio settings set check_libraries_interval 1000000 && \
    platformio settings set check_platformio_interval 1000000 && \
    platformio settings set check_platforms_interval 1000000

ENV ESPHOMEYAML_OTA_HOST_PORT=6123
EXPOSE 6123
VOLUME /config
WORKDIR /usr/src/app

COPY docker/platformio.ini /pio/platformio.ini
RUN platformio run -d /pio; rm -rf /pio

COPY . .
RUN pip install --no-cache-dir --no-binary :all: -e .

WORKDIR /config
ENTRYPOINT ["esphomeyaml"]
CMD ["/config", "dashboard"]
