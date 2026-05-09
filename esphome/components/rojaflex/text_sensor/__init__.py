import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_TYPE

from .. import ROJAFLEX_DEVICE_SCHEMA, RojaflexDevice, register_rojaflex_device, rojaflex_ns

DEPENDENCIES = ["rojaflex"]

TEXT_SENSOR_TYPES = {
    "status": "STATUS",
    "configured_housecode": "CONFIGURED_HOUSECODE",
    "last_rx_raw": "LAST_RX_RAW",
    "last_rx_info": "LAST_RX_INFO",
    "last_tx_error": "LAST_TX_ERROR",
    "channel_status": "CHANNEL_STATUS",
}

RojaflexTextSensor = rojaflex_ns.class_(
    "RojaflexTextSensor", text_sensor.TextSensor, cg.PollingComponent, RojaflexDevice
)
RojaflexTextSensorType = rojaflex_ns.enum("RojaflexTextSensorType", is_class=True)


def validate_channel(config):
    if config[CONF_TYPE] == "channel_status" and CONF_CHANNEL not in config:
        raise cv.Invalid("channel is required for type=channel_status")
    if config[CONF_TYPE] != "channel_status" and CONF_CHANNEL in config:
        raise cv.Invalid("channel is only valid for type=channel_status")
    return config


CONFIG_SCHEMA = cv.All(
    text_sensor.text_sensor_schema(RojaflexTextSensor)
    .extend(ROJAFLEX_DEVICE_SCHEMA)
    .extend(
        {
            cv.Required(CONF_TYPE): cv.enum(TEXT_SENSOR_TYPES, lower=True),
            cv.Optional(CONF_CHANNEL): cv.int_range(min=0, max=15),
        }
    )
    .extend(cv.polling_component_schema("2s")),
    validate_channel,
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)
    await register_rojaflex_device(var, config)
    cg.add(var.set_sensor_type(getattr(RojaflexTextSensorType, TEXT_SENSOR_TYPES[config[CONF_TYPE]])))
    if CONF_CHANNEL in config:
        cg.add(var.set_channel(config[CONF_CHANNEL]))
