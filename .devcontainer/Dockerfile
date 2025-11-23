ARG BUILD_BASE_VERSION=2025.04.0


FROM ghcr.io/esphome/docker-base:debian-${BUILD_BASE_VERSION} AS base

RUN git config --system --add safe.directory "*"

RUN apt update \
    && apt install -y \
      protobuf-compiler

RUN pip install uv

RUN useradd esphome -m

USER esphome
ENV VIRTUAL_ENV=/home/esphome/.local/esphome-venv
RUN uv venv $VIRTUAL_ENV
ENV PATH="$VIRTUAL_ENV/bin:$PATH"
# Override this set to true in the docker-base image
ENV UV_SYSTEM_PYTHON=false

WORKDIR /tmp

COPY requirements.txt ./
RUN uv pip install -r requirements.txt
COPY requirements_dev.txt requirements_test.txt ./
RUN uv pip install -r requirements_dev.txt -r requirements_test.txt

RUN \
    platformio settings set enable_telemetry No \
    && platformio settings set check_platformio_interval 1000000

COPY script/platformio_install_deps.py platformio.ini ./
RUN ./platformio_install_deps.py platformio.ini --libraries --platforms --tools

WORKDIR /workspaces
