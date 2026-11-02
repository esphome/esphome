"""Airton serial climate component for ESPHome."""

import esphome.codegen as cg
from esphome.components import climate, uart

CODEOWNERS = ["@guiguid"]
DOMAIN = "airton_serial"

CONF_AIRTON_SERIAL_ID = "airton_serial_id"

airton_serial_ns = cg.esphome_ns.namespace(DOMAIN)
AirtonSerialClimate = airton_serial_ns.class_(
    "AirtonSerialClimate", uart.UARTDevice, climate.Climate, cg.Component
)
