ARG BUILD_VERSION=dev
ARG BUILD_BASE_VERSION=2026.06.1
ARG BUILD_TYPE=docker

# Builder stages (with build-essential for compiling native Python extensions)
FROM ghcr.io/esphome/docker-base:debian-${BUILD_BASE_VERSION} AS builder-source-docker
FROM ghcr.io/esphome/docker-base:debian-ha-addon-${BUILD_BASE_VERSION} AS builder-source-ha-addon

# Runtime stages (no build-essential)
FROM ghcr.io/esphome/docker-base:debian-runtime-${BUILD_BASE_VERSION} AS runtime-source-docker
FROM ghcr.io/esphome/docker-base:debian-ha-addon-runtime-${BUILD_BASE_VERSION} AS runtime-source-ha-addon

# ======================= builder =======================
ARG BUILD_TYPE
FROM builder-source-${BUILD_TYPE} AS builder

RUN git config --system --add safe.directory "*" \
    && git config --system advice.detachedHead false

ENV PIP_DISABLE_PIP_VERSION_CHECK=1

RUN pip install --no-cache-dir -U pip uv==0.10.1

COPY requirements.txt /

RUN \
    uv pip install --no-cache-dir \
    -r /requirements.txt

# Install the ESPHome Device Builder dashboard.
RUN uv pip install --no-cache-dir esphome-device-builder==1.13.1

RUN \
    platformio settings set enable_telemetry No \
    && platformio settings set check_platformio_interval 1000000 \
    && mkdir -p /piolibs

COPY script/platformio_install_deps.py platformio.ini /
RUN /platformio_install_deps.py /platformio.ini --libraries \
    && rm -rf /root/.platformio/.cache

# Install esphome itself
COPY . /esphome
RUN uv pip install --no-cache-dir -e /esphome


# ======================= runtime base =======================
ARG BUILD_TYPE
FROM runtime-source-${BUILD_TYPE} AS runtime-base

RUN git config --system --add safe.directory "*" \
    && git config --system advice.detachedHead false

# Copy Python packages from builder
COPY --from=builder /usr/local/lib/python3.14/site-packages /usr/local/lib/python3.14/site-packages
COPY --from=builder /usr/local/bin /usr/local/bin

# Copy PlatformIO libraries (pre-installed, no cache)
COPY --from=builder /root/.platformio /root/.platformio

# Copy PlatformIO config
COPY --from=builder /platformio.ini /platformio.ini
COPY --from=builder /platformio_install_deps.py /platformio_install_deps.py
COPY --from=builder /piolibs /piolibs

# Copy esphome source (installed in editable mode)
COPY --from=builder /esphome /esphome

ARG BUILD_VERSION

LABEL \
    org.opencontainers.image.authors="The ESPHome Authors" \
    org.opencontainers.image.title="ESPHome" \
    org.opencontainers.image.description="ESPHome is a system to configure your microcontrollers by simple yet powerful configuration files and control them remotely through Home Automation systems" \
    org.opencontainers.image.url="https://esphome.io/" \
    org.opencontainers.image.documentation="https://esphome.io/" \
    org.opencontainers.image.source="https://github.com/esphome/esphome" \
    org.opencontainers.image.licenses="ESPHome" \
    org.opencontainers.image.version=${BUILD_VERSION}


# ======================= docker-type image =======================
FROM runtime-base AS base-docker

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


# ======================= ha-addon-type image =======================
FROM runtime-base AS base-ha-addon

# Copy root filesystem
COPY docker/ha-addon-rootfs/ /

ARG BUILD_VERSION
LABEL \
    io.hass.name="ESPHome" \
    io.hass.description="ESPHome is a system to configure your microcontrollers by simple yet powerful configuration files and control them remotely through Home Automation systems" \
    io.hass.type="addon" \
    io.hass.version="${BUILD_VERSION}"
    # io.hass.arch is inherited from addon-debian-base

ARG BUILD_TYPE
FROM base-${BUILD_TYPE} AS final
