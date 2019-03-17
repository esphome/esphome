#!/usr/bin/with-contenv bashio
# ==============================================================================
# Community Hass.io Add-ons: ESPHome
# Configures NGINX for use with ESPHome
# ==============================================================================

declare certfile
declare keyfile
declare port

mkdir -p /var/log/nginx

# Enable SSL
if bashio::config.true 'ssl'; then
    rm /etc/nginx/nginx.conf
    mv /etc/nginx/nginx-ssl.conf /etc/nginx/nginx.conf

    certfile=$(bashio::config 'certfile')
    keyfile=$(bashio::config 'keyfile')

    sed -i "s/%%certfile%%/${certfile}/g" /etc/nginx/nginx.conf
    sed -i "s/%%keyfile%%/${keyfile}/g" /etc/nginx/nginx.conf
fi

port=$(bashio::config 'port')
sed -i "s/%%port%%/${port}/g" /etc/nginx/nginx.conf
