#!/usr/bin/with-contenv bashio
# ==============================================================================
# Community Hass.io Add-ons: ESPHome
# This files installs the user ESPHome version if specified
# ==============================================================================

declare esphome_version

if bashio::config.has_value 'esphome_version'; then
    esphome_version=$(bashio::config 'esphome_version')
    full_url="https://github.com/esphome/esphome/archive/${esphome_version}.zip"
    bashio::log.info "Installing esphome version '${esphome_version}' (${full_url})..."
    pip3 install -U --no-cache-dir "${full_url}" \
      || bashio::exit.nok "Failed installing esphome pinned version."
fi
