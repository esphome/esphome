# Dockerfile for HassIO add-on
ARG BUILD_FROM=ubuntu:bionic
FROM ${BUILD_FROM}

# Re-declare BUILD_FROM to fix weird docker issue
ARG BUILD_FROM

# On amd64 and alike, using ubuntu as the base is better as building
# for the ESP32 only works with glibc (and ubuntu). However, on armhf
# the build toolchain frequently procudes segfaults under ubuntu.
#  -> Use ubuntu for most architectures, except alpine for armhf
#
# * python and related required because this is a python project
# * git required for platformio library dependencies downloads
# * libc6-compat and openssh required on alpine for weird reasons
# * disable platformio telemetry on install
RUN /bin/bash -c "if [[ '$BUILD_FROM' = *\"ubuntu\"* ]]; then \
      apt-get update && apt-get install -y --no-install-recommends \
        python python-pip python-setuptools python-pil git && \
      rm -rf /var/lib/apt/lists/* /tmp/*; \
    else \
       apk add --no-cache \
          python2 \
          py2-pip \
          git \
          openssh \
          libc6-compat \
          jpeg-dev \
          zlib-dev \
          freetype-dev \
          lcms2-dev \
          openjpeg-dev \
          tiff-dev; \
    fi" && \
    pip install --no-cache-dir platformio && \
    platformio settings set enable_telemetry No


# Create fake project to make platformio install all depdencies.
# * Ignore build errors from platformio - empty project
# * On alpine, only install ESP8266 toolchain
COPY platformio.ini /pio/platformio.ini
RUN /bin/bash -c "if [[ '$BUILD_FROM' = *\"ubuntu\"* ]]; then \
      platformio run -e espressif32 -e espressif8266 -d /pio; exit 0; \
    else \
      echo \"\$(head -8 /pio/platformio.ini)\" >/pio/platformio.ini; \
      platformio run -e espressif8266 -d /pio; exit 0; \
    fi"

# Install latest esphomeyaml from git
RUN pip install --no-cache-dir \
      git+git://github.com/OttoWinter/esphomeyaml.git && \
    pip install pillow tzlocal

CMD ["esphomeyaml", "/config/esphomeyaml", "dashboard"]
