#!/usr/bin/with-contenv bash
# ==============================================================================
# Community Hass.io Add-ons: ESPHome
# This files check if all user configuration requirements are met
# ==============================================================================
# shellcheck disable=SC1091
source /usr/lib/hassio-addons/base.sh

# Check SSL requirements, if enabled
if hass.config.true 'ssl'; then
    if ! hass.config.has_value 'certfile'; then
        hass.die 'SSL is enabled, but no certfile was specified.'
    fi

    if ! hass.config.has_value 'keyfile'; then
        hass.die 'SSL is enabled, but no keyfile was specified'
    fi

    if ! hass.file_exists "/ssl/$(hass.config.get 'certfile')"; then
        if ! hass.file_exists "/ssl/$(hass.config.get 'keyfile')"; then
            # Both files are missing, let's print a friendlier error message
            text="You enabled encrypted connections using the \"ssl\": true option.
            However, the SSL files \"$(hass.config.get 'certfile')\" and \"$(hass.config.get 'keyfile')\"
            were not found. If you're using Hass.io on your local network and don't want
            to encrypt connections to the ESPHome dashboard, you can manually disable
            SSL by setting \"ssl\" to false."
            hass.die "${text}"
        fi
        hass.die 'The configured certfile is not found'
    fi

    if ! hass.file_exists "/ssl/$(hass.config.get 'keyfile')"; then
        hass.die 'The configured keyfile is not found'
    fi
fi
