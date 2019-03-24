#!/usr/bin/with-contenv bashio
# ==============================================================================
# Community Hass.io Add-ons: ESPHome
# This files migrates the esphome config directory from the old path
# ==============================================================================

if [[ ! -d /config/esphome && -d /config/esphomeyaml ]]; then
    echo "Moving config directory from /config/esphomeyaml to /config/esphome"
    mv /config/esphomeyaml /config/esphome
    mv /config/esphome/.esphomeyaml /config/esphome/.esphome
fi
