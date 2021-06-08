ARG BUILD_FROM=esphome/esphome-base-amd64:3.3.0
FROM ${BUILD_FROM}

# First install requirements to leverage caching when requirements don't change
COPY requirements.txt requirements_optional.txt docker/platformio_install_deps.py platformio.ini /
RUN \
    pip3 install --no-cache-dir -r /requirements.txt -r /requirements_optional.txt \
    && /platformio_install_deps.py /platformio.ini

# Then copy esphome and install
COPY . .
RUN pip3 install --no-cache-dir -e .

# Settings for dashboard
ENV USERNAME="" PASSWORD=""

# Expose the dashboard to Docker
EXPOSE 6052

# Run healthcheck (heartbeat)
HEALTHCHECK --interval=30s --timeout=30s \
  CMD curl --fail http://localhost:6052 || exit 1

# The directory the user should mount their configuration files to
WORKDIR /config
# Set entrypoint to esphome so that the user doesn't have to type 'esphome'
# in every docker command twice
ENTRYPOINT ["esphome"]
# When no arguments given, start the dashboard in the workdir
CMD ["dashboard", "/config"]
