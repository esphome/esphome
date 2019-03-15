ARG BUILD_FROM=esphome/esphome-base-amd64:1.3.0
FROM ${BUILD_FROM}

COPY . .
RUN \
    pip2 install --no-cache-dir --no-binary :all: -e .

WORKDIR /config
ENTRYPOINT ["esphome"]
CMD ["/config", "dashboard"]
