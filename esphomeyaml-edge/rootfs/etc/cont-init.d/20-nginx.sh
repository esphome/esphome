#!/usr/bin/with-contenv bash
# ==============================================================================
# Community Hass.io Add-ons: ESPHome
# Configures NGINX for use with ESPHome
# ==============================================================================
# shellcheck disable=SC1091
source /usr/lib/hassio-addons/base.sh

declare certfile
declare keyfile
declare port

mkdir -p /var/log/nginx

# Enable SSL
if hass.config.true 'ssl'; then
    rm /etc/nginx/nginx.conf
    mv /etc/nginx/nginx-ssl.conf /etc/nginx/nginx.conf

    certfile=$(hass.config.get 'certfile')
    keyfile=$(hass.config.get 'keyfile')

    sed -i "s/%%certfile%%/${certfile}/g" /etc/nginx/nginx.conf
    sed -i "s/%%keyfile%%/${keyfile}/g" /etc/nginx/nginx.conf
fi

port=$(hass.config.get 'port')
sed -i "s/%%port%%/${port}/g" /etc/nginx/nginx.conf
