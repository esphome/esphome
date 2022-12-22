#!/usr/bin/with-contenv bashio
# ==============================================================================
# Community Hass.io Add-ons: ESPHome
# This files creates all directories used by esphome
# ==============================================================================

pio_cache_base=/data/cache/platformio

mkdir -p "${pio_cache_base}"
