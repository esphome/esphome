FROM python:3.13-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
        curl \
        git \
        jq \
        make \
        openssh-client \
        hugo \
        && apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/*

ENV PAGEFIND_VERSION="1.3.0"
ARG TARGETARCH
SHELL ["/bin/bash", "-c"]
RUN <<EOF
    export TARGETARCH=${TARGETARCH/arm64/aarch64}
    export TARGETARCH=${TARGETARCH/amd64/x86_64}
    curl -o pagefind.tar.gz https://github.com/CloudCannon/pagefind/releases/download/v$PAGEFIND_VERSION/pagefind-v$PAGEFIND_VERSION-$TARGETARCH-unknown-linux-musl.tar.gz -L
    tar xzf pagefind.tar.gz
    rm pagefind.tar.gz
    mv pagefind /usr/bin
    chmod +x /usr/bin/pagefind
EOF

RUN useradd -ms /bin/bash esphome


USER esphome

WORKDIR /workspaces/esphome-docs
ENV PATH="${PATH}:/home/esphome/.local/bin"

EXPOSE 8000

CMD ["make", "live-html"]

LABEL \
        org.opencontainers.image.title="esphome-docs" \
        org.opencontainers.image.description="An image to help with ESPHomes documentation development" \
        org.opencontainers.image.vendor="ESPHome" \
        org.opencontainers.image.licenses="CC BY-NC-SA 4.0" \
        org.opencontainers.image.url="https://esphome.io" \
        org.opencontainers.image.source="https://github.com/esphome/esphome-docs" \
        org.opencontainers.image.documentation="https://github.com/esphome/esphome-docs/blob/current/README.md"
