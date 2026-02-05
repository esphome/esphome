import voluptuous as vol

from esphome.components.cc1101 import text_sensor

config = {
    "platform": "cc1101",
    "cc1101_id": "transceiver",
    "rx_attenuation": {"name": "RX Attenuation Text"},
    "chip_id": {"name": "Chip ID"},
    "tuner": {
        "modulation": {"name": "Modulation Text"},
        "frequency": {"name": "Frequency Text"},
    },
}

try:
    print(text_sensor.CONFIG_SCHEMA(config))
except vol.Invalid as e:
    print(e)
