# Build these with the build.py script
# Example:
#   python3 docker/build.py --tag dev --arch amd64 --build-type docker build

# One of "docker", "hassio"
ARG BASEIMGTYPE=docker

FROM ghcr.io/hassio-addons/debian-base/amd64:5.2.3 AS base-hassio-amd64
FROM ghcr.io/hassio-addons/debian-base/aarch64:5.2.3 AS base-hassio-arm64
FROM ghcr.io/hassio-addons/debian-base/armv7:5.2.3 AS base-hassio-armv7
FROM debian:bullseye-20220125-slim AS base-docker-amd64
FROM debian:bullseye-20220125-slim AS base-docker-arm64
FROM debian:bullseye-20220125-slim AS base-docker-armv7

# Use TARGETARCH/TARGETVARIANT defined by docker
# https://docs.docker.com/engine/reference/builder/#automatic-platform-args-in-the-global-scope
FROM base-${BASEIMGTYPE}-${TARGETARCH}${TARGETVARIANT} AS base

RUN \
    apt-get update \
    # Use pinned versions so that we get updates with build caching
    && apt-get install -y --no-install-recommends \
        python3=3.9.2-3 \
        python3-pip=20.3.4-4 \
        python3-setuptools=52.0.0-4 \
        python3-pil=8.1.2+dfsg-0.3+deb11u1 \
        python3-cryptography=3.3.2-1 \
        iputils-ping=3:20210202-1 \
        git=1:2.30.2-1 \
        curl=7.74.0-1.3+deb11u1 \
    && rm -rf \
        /tmp/* \
        /var/{cache,log}/* \
        /var/lib/apt/lists/*

ENV \
  # Fix click python3 lang warning https://click.palletsprojects.com/en/7.x/python3/
  LANG=C.UTF-8 LC_ALL=C.UTF-8 \
  # Store globally installed pio libs in /piolibs
  PLATFORMIO_GLOBALLIB_DIR=/piolibs

RUN \
    # Ubuntu python3-pip is missing wheel
    pip3 install --no-cache-dir \
        wheel==0.37.1 \
        platformio==5.2.5 \
    # Change some platformio settings
    && platformio settings set enable_telemetry No \
    && platformio settings set check_libraries_interval 1000000 \
    && platformio settings set check_platformio_interval 1000000 \
    && platformio settings set check_platforms_interval 1000000 \
    && mkdir -p /piolibs



# ======================= docker-type image =======================
FROM base AS docker

# First install requirements to leverage caching when requirements don't change
COPY requirements.txt requirements_optional.txt docker/platformio_install_deps.py platformio.ini /
RUN \
    pip3 install --no-cache-dir -r /requirements.txt -r /requirements_optional.txt \
    && /platformio_install_deps.py /platformio.ini

# Copy esphome and install
COPY . /esphome
RUN pip3 install --no-cache-dir --no-use-pep517 -e /esphome

# Settings for dashboard
ENV USERNAME="" PASSWORD=""

# Expose the dashboard to Docker
EXPOSE 6052

COPY docker/docker_entrypoint.sh /entrypoint.sh

# The directory the user should mount their configuration files to
VOLUME /config
WORKDIR /config
# Set entrypoint to esphome (via a script) so that the user doesn't have to type 'esphome'
# in every docker command twice
ENTRYPOINT ["/entrypoint.sh"]
# When no arguments given, start the dashboard in the workdir
CMD ["dashboard", "/config"]




# ======================= hassio-type image =======================
FROM base AS hassio

RUN \
    apt-get update \
    # Use pinned versions so that we get updates with build caching
    && apt-get install -y --no-install-recommends \
        nginx=1.18.0-6.1 \
    && rm -rf \
        /tmp/* \
        /var/{cache,log}/* \
        /var/lib/apt/lists/*

ARG BUILD_VERSION=dev

# Copy root filesystem
COPY docker/ha-addon-rootfs/ /

# First install requirements to leverage caching when requirements don't change
COPY requirements.txt requirements_optional.txt docker/platformio_install_deps.py platformio.ini /
RUN \
    pip3 install --no-cache-dir -r /requirements.txt -r /requirements_optional.txt \
    && /platformio_install_deps.py /platformio.ini

# Copy esphome and install
COPY . /esphome
RUN pip3 install --no-cache-dir --no-use-pep517 -e /esphome

# Labels
LABEL \
    io.hass.name="ESPHome" \
    io.hass.description="Manage and program ESP8266/ESP32 microcontrollers through YAML configuration files" \
    io.hass.type="addon" \
    io.hass.version="${BUILD_VERSION}"
    # io.hass.arch is inherited from addon-debian-base




# ======================= lint-type image =======================
FROM base AS lint

ENV \
  PLATFORMIO_CORE_DIR=/esphome/.temp/platformio

RUN \
    apt-get update \
    # Use pinned versions so that we get updates with build caching
    && apt-get install -y --no-install-recommends \
        clang-format-11=1:11.0.1-2 \
        clang-tidy-11=1:11.0.1-2 \
        patch=2.7.6-7 \
        software-properties-common=0.96.20.2-2.1 \
        nano=5.4-2 \
        build-essential=12.9 \
        python3-dev=3.9.2-3 \
    && rm -rf \
        /tmp/* \
        /var/{cache,log}/* \
        /var/lib/apt/lists/*

COPY requirements.txt requirements_optional.txt requirements_test.txt docker/platformio_install_deps.py platformio.ini /
RUN \
    pip3 install --no-cache-dir -r /requirements.txt -r /requirements_optional.txt -r /requirements_test.txt \
    && /platformio_install_deps.py /platformio.ini

VOLUME ["/esphome"]
WORKDIR /esphome
