ARG BUILD_VERSION=dev
ARG BUILD_TYPE=docker

FROM ghcr.io/esphome/docker-base:2024.9.0 AS base

RUN git config --system --add safe.directory "*"

COPY requirements.txt /

FROM base AS base-amd64
FROM base AS base-arm64
FROM base AS base-armv7

RUN \
    export cryptography_version=$(grep 'cryptography==' /requirements.txt | cut -d'=' -f3) \
    && curl -L https://www.piwheels.org/cp311/cryptography-$cryptography_version-cp37-abi3-linux_armv7l.whl -o /tmp/cryptography-$cryptography_version-cp37-abi3-linux_armv7l.whl \
    && uv pip install --no-cache-dir /tmp/cryptography-$cryptography_version-cp37-abi3-linux_armv7l.whl \
    && rm /tmp/cryptography-$cryptography_version-cp37-abi3-linux_armv7l.whl


ARG TARGETARCH
ARG TARGETVARIANT

FROM base-${TARGETARCH}${TARGETVARIANT} AS base-final

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

FROM base-final AS base-docker

EXPOSE 6052

HEALTHCHECK --interval=30s --timeout=30s \
    CMD curl --fail http://localhost:6052/version -A "HealthCheck" || exit 1

COPY docker/docker_entrypoint.sh /entrypoint.sh

VOLUME /config
WORKDIR /config
ENTRYPOINT ["/entrypoint.sh"]
CMD ["dashboard", "/config"]

FROM base-final AS base-hassio

ARG \
    BASHIO_VERSION=0.16.2 \
    TEMPIO_VERSION=2021.09.0 \
    S6_OVERLAY_VERSION=3.2.0.0

WORKDIR /usr/src

ARG TARGETARCH
ARG TARGETVARIANT

RUN \
    set -x \
    && if [ "${TARGETARCH}${TARGETVARIANT}" = "armv7" ]; then \
            export S6_ARCH="arm"; \
            export TEMPIO_ARCH="armv7"; \
        elif [ "${TARGETARCH}${TARGETVARIANT}" = "amd64" ]; then \
            export S6_ARCH="x86_64"; \
            export TEMPIO_ARCH="amd64"; \
        elif [ "${TARGETARCH}${TARGETVARIANT}" = "arm64"]; then \
            export S6_ARCH="aarch64"; \
            export TEMPIO_ARCH="aarch64"; \
        fi \
    \
    && curl -L -f -s "https://github.com/just-containers/s6-overlay/releases/download/v${S6_OVERLAY_VERSION}/s6-overlay-${S6_ARCH}.tar.xz" \
        | tar Jxvf - -C / \
    && curl -L -f -s "https://github.com/just-containers/s6-overlay/releases/download/v${S6_OVERLAY_VERSION}/s6-overlay-noarch.tar.xz" \
        | tar Jxvf - -C / \
    && curl -L -f -s "https://github.com/just-containers/s6-overlay/releases/download/v${S6_OVERLAY_VERSION}/s6-overlay-symlinks-arch.tar.xz" \
        | tar Jxvf - -C / \
    && curl -L -f -s "https://github.com/just-containers/s6-overlay/releases/download/v${S6_OVERLAY_VERSION}/s6-overlay-symlinks-noarch.tar.xz" \
        | tar Jxvf - -C / \
    && mkdir -p /etc/fix-attrs.d \
    && mkdir -p /etc/services.d \
    \
    && curl -L -f -s -o /usr/bin/tempio \
        "https://github.com/home-assistant/tempio/releases/download/${TEMPIO_VERSION}/tempio_${TEMPIO_ARCH}" \
    && chmod a+x /usr/bin/tempio \
    \
    && mkdir -p /usr/src/bashio \
    && curl -L -f -s "https://github.com/hassio-addons/bashio/archive/v${BASHIO_VERSION}.tar.gz" \
        | tar -xzf - --strip 1 -C /usr/src/bashio \
    && mv /usr/src/bashio/lib /usr/lib/bashio \
    && ln -s /usr/lib/bashio/bashio /usr/bin/bashio \
    \
    && rm -rf /usr/src/*

RUN \
    set -x \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        nginx-light=1.22.1-9 \
        jq \
    && rm -rf \
        /tmp/* \
        /var/{cache,log}/* \
        /var/lib/apt/lists/* \
        /usr/src/*


COPY docker/ha-addon-rootfs/ /

WORKDIR /root
ENTRYPOINT ["/init"]

ARG BUILD_VERSION
LABEL \
    io.hass.name="ESPHome" \
    io.hass.description="Manage and program microcontrollers through YAML configuration files" \
    io.hass.type="addon" \
    io.hass.version="${BUILD_VERSION}"


ARG BUILD_TYPE
FROM base-${BUILD_TYPE} AS final

COPY . /esphome

RUN uv pip install --no-cache-dir -e /esphome


FROM base-final AS lint

# TODO:
