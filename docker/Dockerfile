# Build these with the build.py script
# Example:
#   python3 docker/build.py --tag dev --arch amd64 --build-type docker build

# One of "docker", "hassio"
ARG BASEIMGTYPE=docker


# https://github.com/hassio-addons/addon-debian-base/releases
FROM ghcr.io/hassio-addons/debian-base:7.2.0 AS base-hassio
# https://hub.docker.com/_/debian?tab=tags&page=1&name=bookworm
FROM debian:12.2-slim AS base-docker

FROM base-${BASEIMGTYPE} AS base


ARG TARGETARCH
ARG TARGETVARIANT


# Note that --break-system-packages is used below because
# https://peps.python.org/pep-0668/ added a safety check that prevents
# installing packages with the same name as a system package. This is
# not a problem for us because we are not concerned about overwriting
# system packages because we are running in an isolated container.

RUN \
    apt-get update \
    # Use pinned versions so that we get updates with build caching
    && apt-get install -y --no-install-recommends \
        python3-pip=23.0.1+dfsg-1 \
        python3-setuptools=66.1.1-1 \
        python3-venv=3.11.2-1+b1 \
        python3-wheel=0.38.4-2 \
        iputils-ping=3:20221126-1 \
        git=1:2.39.5-0+deb12u1 \
        curl=7.88.1-10+deb12u7 \
        openssh-client=1:9.2p1-2+deb12u3 \
        python3-cffi=1.15.1-5 \
        libcairo2=1.16.0-7 \
        libmagic1=1:5.44-3 \
        patch=2.7.6-7 \
    && ( \
        ( \
            [ "$TARGETARCH$TARGETVARIANT" = "armv7" ] && \
                apt-get install -y --no-install-recommends \
                build-essential=12.9 \
                python3-dev=3.11.2-1+b1 \
                zlib1g-dev=1:1.2.13.dfsg-1 \
                libjpeg-dev=1:2.1.5-2 \
                libfreetype-dev=2.12.1+dfsg-5+deb12u3 \
                libssl-dev=3.0.14-1~deb12u2 \
                libffi-dev=3.4.4-1 \
                libopenjp2-7=2.5.0-2 \
                libtiff6=4.5.0-6+deb12u1 \
                cargo=0.66.0+ds1-1 \
                pkg-config=1.8.1-1 \
                gcc-arm-linux-gnueabihf=4:12.2.0-3 \
        ) \
        || [ "$TARGETARCH$TARGETVARIANT" != "armv7" ] \
    ) \
    && rm -rf \
        /tmp/* \
        /var/{cache,log}/* \
        /var/lib/apt/lists/*

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
    apt-get update \
    # Use pinned versions so that we get updates with build caching
    && apt-get install -y --no-install-recommends \
        nginx-light=1.22.1-9 \
    && rm -rf \
        /tmp/* \
        /var/{cache,log}/* \
        /var/lib/apt/lists/*

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
    apt-get update \
    # Use pinned versions so that we get updates with build caching
    && apt-get install -y --no-install-recommends \
        clang-format-13=1:13.0.1-11+b2 \
        clang-tidy-14=1:14.0.6-12 \
        patch=2.7.6-7 \
        software-properties-common=0.99.30-4.1~deb12u1 \
        nano=7.2-1+deb12u1 \
        build-essential=12.9 \
        python3-dev=3.11.2-1+b1 \
    && rm -rf \
        /tmp/* \
        /var/{cache,log}/* \
        /var/lib/apt/lists/*

COPY requirements_test.txt /
RUN if [ "$TARGETARCH$TARGETVARIANT" = "armv7" ]; then \
        export PIP_EXTRA_INDEX_URL="https://www.piwheels.org/simple"; \
  fi; \
  pip3 install \
  --break-system-packages --no-cache-dir -r /requirements_test.txt

VOLUME ["/esphome"]
WORKDIR /esphome
