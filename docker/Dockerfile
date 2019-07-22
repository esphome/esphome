ARG BUILD_FROM=esphome/esphome-base-amd64:1.8.3
FROM ${BUILD_FROM}

COPY . .
RUN pip2 install --no-cache-dir -e .

ENV USERNAME=""
ENV PASSWORD=""

WORKDIR /config
ENTRYPOINT ["esphome"]
CMD ["/config", "dashboard"]
