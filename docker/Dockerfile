ARG BUILD_TYPE=docker

FROM ghcr.io/esphome/docker-base:2024.9.0 AS base

RUN git config --system --add safe.directory "*"

COPY requirements.txt /

RUN \
    uv pip install --no-cache-dir \
    -r /requirements.txt

RUN \
    # Change some platformio settings
    platformio settings set enable_telemetry No \
    && platformio settings set check_platformio_interval 1000000 \
    && mkdir -p /piolibs

COPY script/platformio_install_deps.py platformio.ini /

RUN /platformio_install_deps.py /platformio.ini --libraries

FROM base AS base-docker

ENV USERNAME="" PASSWORD=""
EXPOSE 6052

HEALTHCHECK --interval=30s --timeout=30s \
    CMD curl --fail http://localhost:6052/version -A "HealthCheck" || exit 1

COPY docker/docker_entrypoint.sh /entrypoint.sh

VOLUME /config
WORKDIR /config
ENTRYPOINT ["/entrypoint.sh"]
CMD ["dashboard", "/config"]

FROM base AS base-hassio

# TODO: Install s6-overlay

RUN \
    set -x \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        nginx-light=1.22.1-9 \
    && rm -rf \
        /tmp/* \
        /var/{cache,log}/* \
        /var/lib/apt/lists/* \
        /usr/src/*

ARG BUILD_VERSION=dev

COPY docker/ha-addon-rootfs/ /

LABEL \
    io.hass.name="ESPHome" \
    io.hass.description="Manage and program microcontrollers through YAML configuration files" \
    io.hass.type="addon" \
    io.hass.version="${BUILD_VERSION}"


FROM base-${BUILD_TYPE} AS final

COPY . /esphome

RUN uv pip install --no-cache-dir -e /esphome


FROM base AS lint

# TODO:
