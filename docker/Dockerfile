ARG BUILD_FROM=esphome/esphome-base-amd64:1.0.0
FROM ${BUILD_FROM}

COPY . .
RUN \
    pip install --no-cache-dir --no-binary :all: -e . && \
    pip install --no-cache-dir pillow

WORKDIR /config
ENTRYPOINT ["esphome"]
CMD ["/config", "dashboard"]
