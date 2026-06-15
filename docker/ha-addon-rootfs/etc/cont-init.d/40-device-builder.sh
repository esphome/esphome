#!/usr/bin/with-contenv bashio
# ==============================================================================
# Installs the latest prerelease of esphome-device-builder when the
# `use_new_device_builder` config option is enabled.
# This is a temporary install-on-boot step until esphome-device-builder
# becomes a direct dependency of esphome.
# ==============================================================================

if ! bashio::config.true 'use_new_device_builder'; then
  exit 0
fi

bashio::log.info "Installing latest prerelease of esphome-device-builder..."
if command -v uv > /dev/null; then
  uv pip install --system --no-cache-dir --prerelease=allow --upgrade \
    esphome-device-builder ||
    bashio::exit.nok "Failed installing esphome-device-builder."
else
  pip install --no-cache-dir --pre --upgrade esphome-device-builder ||
    bashio::exit.nok "Failed installing esphome-device-builder."
fi
bashio::log.info "Installed esphome-device-builder."
