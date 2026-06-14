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

# The image does not hard-code a CMD; the proper default is supplied here based
# on which version is selected. Any command passed to the container is forwarded
# verbatim and must be consistent with the selected version.
case "${USE_NEW_DEVICE_BUILDER,,}" in
    1 | true | yes | on)
        # Install the latest prerelease of esphome-device-builder on boot.
        # This is a temporary install-on-boot step until esphome-device-builder
        # becomes a direct dependency of esphome.
        echo "Installing latest prerelease of esphome-device-builder..."
        uv pip install --system --no-cache-dir --prerelease=allow --upgrade \
            esphome-device-builder

        # Default to serving the config directory when no command is given.
        if [[ $# -eq 0 ]]; then
            set -- /config
        fi
        exec esphome-device-builder "$@"
        ;;
esac

# Default to the dashboard when no command is given.
if [[ $# -eq 0 ]]; then
    set -- dashboard /config
fi
exec esphome "$@"
