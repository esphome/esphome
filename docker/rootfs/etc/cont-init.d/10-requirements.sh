#!/usr/bin/with-contenv bashio
# ==============================================================================
# Community Hass.io Add-ons: ESPHome
# This files check if all user configuration requirements are met
# ==============================================================================

# Check SSL requirements, if enabled
if bashio::config.true 'ssl'; then
    if ! bashio::config.has_value 'certfile'; then
        bashio::fatal 'SSL is enabled, but no certfile was specified.'
        bashio::exit.nok
    fi

    if ! bashio::config.has_value 'keyfile'; then
        bashio::fatal 'SSL is enabled, but no keyfile was specified'
        bashio::exit.nok
    fi


    certfile="/ssl/$(bashio::config 'certfile')"
    keyfile="/ssl/$(bashio::config 'keyfile')"

    if ! bashio::fs.file_exists "${certfile}"; then
        if ! bashio::fs.file_exists "${keyfile}"; then
            # Both files are missing, let's print a friendlier error message
            bashio::log.fatal 'You enabled encrypted connections using the "ssl": true option.'
            bashio::log.fatal "However, the SSL files '${certfile}' and '${keyfile}'"
            bashio::log.fatal "were not found. If you're using Hass.io on your local network and don't want"
            bashio::log.fatal 'to encrypt connections to the ESPHome dashboard, you can manually disable'
            bashio::log.fatal 'SSL by setting "ssl" to false."'
            bashio::exit.nok
        fi
        bashio::log.fatal "The configured certfile '${certfile}' was not found."
        bashio::exit.nok
    fi

    if ! bashio::fs.file_exists "/ssl/$(bashio::config 'keyfile')"; then
        bashio::log.fatal "The configured keyfile '${keyfile}' was not found."
        bashio::exit.nok
    fi
fi
