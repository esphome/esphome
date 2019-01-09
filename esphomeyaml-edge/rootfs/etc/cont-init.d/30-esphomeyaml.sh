#!/usr/bin/with-contenv bash
# ==============================================================================
# Community Hass.io Add-ons: esphomeyaml
# This files installs the user esphomeyaml version if specified
# ==============================================================================
# shellcheck disable=SC1091
source /usr/lib/hassio-addons/base.sh

declare esphomeyaml_version

if hass.config.has_value 'esphomeyaml_version'; then
    esphomeyaml_version=$(hass.config.get 'esphomeyaml_version')
    pip2 install --no-cache-dir --no-binary :all: "https://github.com/OttoWinter/esphomeyaml/archive/${esphomeyaml_version}.zip"
fi
