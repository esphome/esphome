#!/bin/bash

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

# we can't set core_dir, because the settings file is stored in `core_dir/appstate.json`
# setting `core_dir` would therefore prevent pio from accessing
export PLATFORMIO_PLATFORMS_DIR="${pio_cache_base}/platforms"
export PLATFORMIO_PACKAGES_DIR="${pio_cache_base}/packages"
export PLATFORMIO_CACHE_DIR="${pio_cache_base}/cache"

# If /build is mounted, use that as the build path
# otherwise use path in /config (so that builds aren't lost on container restart)
if [[ -d /build ]]; then
    export ESPHOME_BUILD_PATH=/build
fi

exec esphome "$@"
