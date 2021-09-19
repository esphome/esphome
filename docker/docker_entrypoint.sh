#!/bin/bash

# If /cache is mounted, use that as PIO's coredir
# otherwise use path in /config (so that PIO packages aren't downloaded on each compile)

if [[ -d /cache ]]; then
    export PLATFORMIO_CORE_DIR=/cache/platformio
else
    export PLATFORMIO_CORE_DIR=/config/.esphome/platformio
fi

if [[ ! -d "${PLATFORMIO_CORE_DIR}" ]]; then
    echo "Creating cache directory ${PLATFORMIO_CORE_DIR}"
    echo "You can change this behavior by mounting a directory to the container's /cache directory."
    mkdir -p "${PLATFORMIO_CORE_DIR}"
fi

exec esphome "$@"
