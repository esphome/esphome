# Build these with the build.py script
# Example:
#   python3 docker/build.py --tag dev --arch amd64 --build-type docker build

# One of "docker", "hassio"
ARG BASEIMGTYPE=docker


# https://github.com/hassio-addons/addon-debian-base/releases
FROM ghcr.io/hassio-addons/base:16.1.2 AS base-hassio
# FROM debian:12.2-slim AS base-docker
FROM ghcr.io/hassio-addons/base:16.1.2 AS base-docker

FROM base-${BASEIMGTYPE} AS base


ARG TARGETARCH
ARG TARGETVARIANT


# Note that --break-system-packages is used below because
# https://peps.python.org/pep-0668/ added a safety check that prevents
# installing packages with the same name as a system package. This is
# not a problem for us because we are not concerned about overwriting
# system packages because we are running in an isolated container.

RUN \
    apk update \
    # Use pinned versions so that we get updates with build caching
    && apk add --no-cache \
        python3=3.12.6-r0 \
        py3-pip=24.0-r2 \
        py3-setuptools=70.3.0-r0 \
        libmagic=5.45-r1 \
        git=2.45.2-r0 \
        patch=2.7.6-r10

RUN \
    # Install build tools for wheels - these will be removed after use
    apk add --no-cache --virtual .build-deps \
    build-base=0.5-r3 \
    python3-dev=3.12.6-r0 \
    zlib-dev=1.3.1-r1 \
    jpeg-dev=9e-r1 \
    libffi-dev=3.4.6-r0 \
    openjpeg=2.5.2-r0 \
    tiff-dev=4.6.0t-r0

ENV \
  # Fix click python3 lang warning https://click.palletsprojects.com/en/7.x/python3/
  LANG=C.UTF-8 LC_ALL=C.UTF-8 \
  # Store globally installed pio libs in /piolibs
  PLATFORMIO_GLOBALLIB_DIR=/piolibs

# Support legacy binaries on Debian multiarch system. There is no "correct" way
# to do this, other than using properly built toolchains...
# See: https://unix.stackexchange.com/questions/553743/correct-way-to-add-lib-ld-linux-so-3-in-debian
RUN \
    if [ "$TARGETARCH$TARGETVARIANT" = "armv7" ]; then \
        ln -s /lib/arm-linux-gnueabihf/ld-linux-armhf.so.3 /lib/ld-linux.so.3; \
    fi

RUN \
    # Ubuntu python3-pip is missing wheel
    if [ "$TARGETARCH$TARGETVARIANT" = "armv7" ]; then \
        export PIP_EXTRA_INDEX_URL="https://www.piwheels.org/simple"; \
    fi; \
    pip3 install \
    --break-system-packages --no-cache-dir \
    # Keep platformio version in sync with requirements.txt
    platformio==6.1.15 \
    # Change some platformio settings
    && platformio settings set enable_telemetry No \
    && platformio settings set check_platformio_interval 1000000 \
    && mkdir -p /piolibs


# First install requirements to leverage caching when requirements don't change
# tmpfs is for https://github.com/rust-lang/cargo/issues/8719

COPY requirements.txt requirements_optional.txt /
RUN --mount=type=tmpfs,target=/root/.cargo if [ "$TARGETARCH$TARGETVARIANT" = "armv7" ]; then \
        curl -L https://www.piwheels.org/cp311/cryptography-43.0.0-cp37-abi3-linux_armv7l.whl -o /tmp/cryptography-43.0.0-cp37-abi3-linux_armv7l.whl \
        && pip3 install --break-system-packages --no-cache-dir /tmp/cryptography-43.0.0-cp37-abi3-linux_armv7l.whl \
        && rm /tmp/cryptography-43.0.0-cp37-abi3-linux_armv7l.whl \
        && export PIP_EXTRA_INDEX_URL="https://www.piwheels.org/simple"; \
    fi; \
    CARGO_REGISTRIES_CRATES_IO_PROTOCOL=sparse CARGO_HOME=/root/.cargo \
    pip3 install \
    --break-system-packages --no-cache-dir -r /requirements.txt -r /requirements_optional.txt

COPY script/platformio_install_deps.py platformio.ini /
RUN /platformio_install_deps.py /platformio.ini --libraries

# Clean up build tools
RUN apk del .build-deps

# Avoid unsafe git error when container user and file config volume permissions don't match
RUN git config --system --add safe.directory '*'


# ======================= docker-type image =======================
FROM base AS docker

# Copy esphome and install
COPY . /esphome
RUN if [ "$TARGETARCH$TARGETVARIANT" = "armv7" ]; then \
        export PIP_EXTRA_INDEX_URL="https://www.piwheels.org/simple"; \
  fi; \
  pip3 install \
  --break-system-packages --no-cache-dir -e /esphome


# Settings for dashboard
ENV USERNAME="" PASSWORD=""

# Expose the dashboard to Docker
EXPOSE 6052

# Run healthcheck (heartbeat)
HEALTHCHECK --interval=30s --timeout=30s \
  CMD curl --fail http://localhost:6052/version -A "HealthCheck" || exit 1

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
    apk update \
    # Use pinned versions so that we get updates with build caching
    && apk add --no-cache \
        nginx=1.26.2-r0

ARG BUILD_VERSION=dev

# Copy root filesystem
COPY docker/ha-addon-rootfs/ /

# Copy esphome and install
COPY . /esphome
RUN if [ "$TARGETARCH$TARGETVARIANT" = "armv7" ]; then \
        export PIP_EXTRA_INDEX_URL="https://www.piwheels.org/simple"; \
  fi; \
  pip3 install \
  --break-system-packages --no-cache-dir -e /esphome

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
    apk update \
    # Use pinned versions so that we get updates with build caching
    && apk add --no-cache \
        clang15-extra-tools=15.0.7-r19 \
        nano=8.0-r0 \
        build-base=0.5-r3 \
        python3-dev=3.12.6-r0

COPY requirements_test.txt /
RUN if [ "$TARGETARCH$TARGETVARIANT" = "armv7" ]; then \
        export PIP_EXTRA_INDEX_URL="https://www.piwheels.org/simple"; \
  fi; \
  pip3 install \
  --break-system-packages --no-cache-dir -r /requirements_test.txt

VOLUME ["/esphome"]
WORKDIR /esphome
