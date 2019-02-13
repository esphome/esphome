#!/usr/bin/with-contenv bash
# ==============================================================================
# Community Hass.io Add-ons: ESPHome
# This files installs the user ESPHome version if specified
# ==============================================================================
# shellcheck disable=SC1091
source /usr/lib/hassio-addons/base.sh

declare esphome_version

if hass.config.has_value 'esphome_version'; then
    esphome_version=$(hass.config.get 'esphome_version')
    pip2 install --no-cache-dir --no-binary :all: "https://github.com/esphome/esphome/archive/${esphome_version}.zip"
fi
