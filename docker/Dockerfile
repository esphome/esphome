ARG BUILD_FROM=esphome/esphome-base-amd64:2.0.0
FROM ${BUILD_FROM}

COPY . .
RUN pip3 install --no-cache-dir -e .

ENV USERNAME=""
ENV PASSWORD=""

WORKDIR /config
ENTRYPOINT ["esphome"]
CMD ["/config", "dashboard"]
