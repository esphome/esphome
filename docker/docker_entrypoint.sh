#!/usr/bin/env bash

# If /cache is mounted, use that as PIO's coredir
# otherwise use path in /config (so that PIO packages aren't downloaded on each compile)

if [[ -d /cache ]]; then
    pio_cache_base=/cache/platformio
else
    pio_cache_base=/config/.esphome/platformio
fi

if [[ ! -d "${pio_cache_base}" ]]; then
    echo "Creating cache directory ${pio_cache_base}"
    echo "You can change this behavior by mounting a directory to the container's /cache directory."
    mkdir -p "${pio_cache_base}"
fi

# We can't set PLATFORMIO_CORE_DIR because the settings file is stored in
# `core_dir/appstate.json` and setting it would prevent pio from reading
# cached settings. Instead we override individual subdirectories.
export PLATFORMIO_PLATFORMS_DIR="${pio_cache_base}/platforms"
export PLATFORMIO_PACKAGES_DIR="${pio_cache_base}/packages"
export PLATFORMIO_CACHE_DIR="${pio_cache_base}/cache"

# Symlink penv to persistent storage so ESP-IDF venvs can be cleaned with
# "Clean All". There's no env var for penv location.
mkdir -p "${pio_cache_base}/penv"
mkdir -p /root/.platformio
ln -sfn "${pio_cache_base}/penv" /root/.platformio/penv

# If /build is mounted, use that as the build path
# otherwise use path in /config (so that builds aren't lost on container restart)
if [[ -d /build ]]; then
    export ESPHOME_BUILD_PATH=/build
fi

exec esphome "$@"
