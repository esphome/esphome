import esphome.config_validation as cv

CONFIG_SCHEMA = cv.invalid(
    "The 'udp.sensor' component has been migrated to the 'packet_transport.sensor' component."
)
