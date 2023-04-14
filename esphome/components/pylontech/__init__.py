import logging
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID


from esphome.core import CORE, coroutine_with_priority

CODEOWNERS = ["@functionpointer"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor"]
_LOGGER = logging.getLogger(__name__)
MULTI_CONF = True

CONF_PYLONTECH_ID = "pylontech_id"
CONF_BATTERY = "battery"
CONF_COULOMB = "coulomb"
CONF_TEMPERATURE_LOW = "temperature_low"
CONF_TEMPERATURE_HIGH = "temperature_high"
CONF_VOLTAGE_LOW = "voltage_low"
CONF_VOLTAGE_HIGH = "voltage_high"
CONF_MOS_TEMPERATURE = "mos_temperature"

CONF_BASE_STATE = "base_state"
CONF_VOLTAGE_STATE = "voltage_state"
CONF_CURRENT_STATE = "current_state"
CONF_TEMPERATURE_STATE = "temperature_state"

pylontech_ns = cg.esphome_ns.namespace("pylontech")
PylontechComponent = pylontech_ns.class_(
    "PylontechComponent", cg.PollingComponent, uart.UARTDevice
)

PYLONTECH_COMPONENT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_PYLONTECH_ID): cv.use_id(PylontechComponent),
        cv.Required(CONF_BATTERY): cv.int_range(1, 6),
    }
)

CONFIG_SCHEMA = cv.All(
    cv.Schema({cv.GenerateID(): cv.declare_id(PylontechComponent)})
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)
# Each instance of PylontechComponent may address multiple batteries
# With their own sensors for voltage, current, etc
# To make this possible, each PylontechComponent has an array for each type of sensor
# Problem: we need to know how long those arrays should be
# Solution: keep track of the maximum index that is used in the entire esphome config
# This is done using the global _MAX_BATTERY_INDEX
# Whenever a battery index is used, mark_battery_index_in_use is called to keep track
# add_num_batteries() will be called after all configs are processed to tell the c code the final count
_MAX_BATTERY_INDEX = -1


async def mark_battery_index_in_use(index: int, parent):
    global _MAX_BATTERY_INDEX
    _MAX_BATTERY_INDEX = max(_MAX_BATTERY_INDEX, index)
    cg.add(parent.mark_battery_index_in_use(index))


@coroutine_with_priority(-5.0)
async def add_num_batteries():
    if not getattr(add_num_batteries, "already_ran", False):
        if _MAX_BATTERY_INDEX < 0:
            _LOGGER.error("no idea how many batteries!")
        cg.add_define("PYLONTECH_NUM_BATTERIES", _MAX_BATTERY_INDEX)
        add_num_batteries.already_ran = True


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    CORE.add_job(add_num_batteries)
