ARG BUILD_VERSION=dev
ARG BUILD_BASE_VERSION=2026.06.1
ARG BUILD_TYPE=docker

# Runtime images ship Python and the tools ESPHome needs without the compiler toolchain
FROM ghcr.io/esphome/docker-base:debian-runtime-${BUILD_BASE_VERSION} AS runtime-source-docker
FROM ghcr.io/esphome/docker-base:debian-ha-addon-runtime-${BUILD_BASE_VERSION} AS runtime-source-ha-addon


# ======================= builder =======================
# The full image carries build-essential for native Python extensions. Everything, uv included, is installed
# into one virtualenv so the runtime stage can copy a single directory that carries no Python version in its path.
FROM ghcr.io/esphome/docker-base:debian-${BUILD_BASE_VERSION} AS builder

# docker-base sets UV_SYSTEM_PYTHON, which would make uv skip the venv and install into the system Python
ENV VIRTUAL_ENV=/opt/venv \
    UV_SYSTEM_PYTHON=false \
    PATH="/opt/venv/bin:$PATH"

RUN python3 -m venv /opt/venv \
    && pip install --no-cache-dir --disable-pip-version-check uv==0.10.1

COPY requirements.txt /

RUN \
    uv pip install --no-cache-dir \
    -r /requirements.txt

# Install the ESPHome Device Builder dashboard.
RUN uv pip install --no-cache-dir esphome-device-builder==1.23.0


# ======================= runtime =======================
FROM runtime-source-${BUILD_TYPE} AS base

RUN git config --system --add safe.directory "*" \
    && git config --system advice.detachedHead false

# The venv goes first on PATH so pip, uv and the console scripts all resolve inside it
ENV PIP_DISABLE_PIP_VERSION_CHECK=1 \
    VIRTUAL_ENV=/opt/venv \
    UV_SYSTEM_PYTHON=false \
    PATH="/opt/venv/bin:$PATH"

COPY --from=builder /opt/venv /opt/venv

RUN \
    platformio settings set enable_telemetry No \
    && platformio settings set check_platformio_interval 1000000 \
    && mkdir -p /piolibs

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
FROM base AS base-docker

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
FROM base AS base-ha-addon

# Copy root filesystem
COPY docker/ha-addon-rootfs/ /

ARG BUILD_VERSION
LABEL \
    io.hass.name="ESPHome" \
    io.hass.description="ESPHome is a system to configure your microcontrollers by simple yet powerful configuration files and control them remotely through Home Automation systems" \
    io.hass.type="addon" \
    io.hass.version="${BUILD_VERSION}"
    # io.hass.arch is inherited from addon-debian-base

FROM base-${BUILD_TYPE} AS final

# ESPHome itself is pure Python; installing it last keeps the dependency layer cached across source changes
COPY . /esphome
RUN uv pip install --no-cache-dir -e /esphome
