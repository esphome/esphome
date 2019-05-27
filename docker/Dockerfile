ARG BUILD_FROM=esphome/esphome-base-amd64:1.5.1
FROM ${BUILD_FROM}

COPY . .
RUN pip2 install --no-cache-dir -e .

WORKDIR /config
ENTRYPOINT ["esphome"]
CMD ["/config", "dashboard"]
