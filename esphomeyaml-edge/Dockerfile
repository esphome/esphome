# Dockerfile for HassIO edge add-on
ARG BUILD_FROM=homeassistant/amd64-base-ubuntu:latest
FROM ${BUILD_FROM}

RUN apt-get update && apt-get install -y --no-install-recommends \
        python \
        python-pip \
        python-setuptools \
        python-pil \
        git \
    && apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* && \
    pip install --no-cache-dir --no-binary :all: platformio && \
    platformio settings set enable_telemetry No && \
    platformio settings set check_libraries_interval 1000000 && \
    platformio settings set check_platformio_interval 1000000 && \
    platformio settings set check_platforms_interval 1000000

COPY platformio.ini /pio/platformio.ini
RUN platformio run -d /pio; rm -rf /pio

RUN pip install --no-cache-dir git+https://github.com/OttoWinter/esphomeyaml.git@dev#egg=esphomeyaml && \
    pip install --no-cache-dir pillow tzlocal

CMD ["esphomeyaml", "/config/esphomeyaml", "dashboard"]
