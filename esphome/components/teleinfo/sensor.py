import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, uart
from esphome.const import CONF_ID, CONF_SENSOR, ICON_FLASH, UNIT_WATT_HOURS

DEPENDENCIES = ['uart']

teleinfo_ns = cg.esphome_ns.namespace('teleinfo')
TeleInfo = teleinfo_ns.class_('TeleInfo', cg.PollingComponent, uart.UARTDevice)

CONF_TAG_NAME = "tag_name"
TELEINFO_TAG_SCHEMA = cv.Schema({
    cv.Required(CONF_TAG_NAME): cv.string,
    cv.Required(CONF_SENSOR): sensor.sensor_schema(UNIT_WATT_HOURS, ICON_FLASH, 0)
})

CONF_TAGS = "tags"
CONF_HISTORICAL_MODE = "historical_mode"
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(TeleInfo),
    cv.Optional(CONF_HISTORICAL_MODE, default=False): cv.boolean,
    cv.Optional(CONF_TAGS): cv.ensure_list(TELEINFO_TAG_SCHEMA),
}).extend(cv.polling_component_schema('60s')).extend(uart.UART_DEVICE_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_HISTORICAL_MODE])
    yield cg.register_component(var, config)
    yield uart.register_uart_device(var, config)

    if CONF_TAGS in config:
        for tag in config[CONF_TAGS]:
            sens = yield sensor.new_sensor(tag[CONF_SENSOR])
            cg.add(var.register_teleinfo_sensor(tag[CONF_TAG_NAME], sens))
