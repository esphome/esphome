import esphome.config_validation as cv

CONFIG_SCHEMA = cv.invalid(
    "The 'udp.binary_sensor' component has been migrated to the 'packet_transport.binary_sensor' component."
)
