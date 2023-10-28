import logging
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID

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
CONF_NUM_BATTERIES = "num_batteries"

pylontech_ns = cg.esphome_ns.namespace("pylontech")
PylontechComponent = pylontech_ns.class_(
    "PylontechComponent", cg.PollingComponent, uart.UARTDevice
)

CV_NUM_BATTERIES = cv.int_range(1, 6)

PYLONTECH_COMPONENT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_PYLONTECH_ID): cv.use_id(PylontechComponent),
        cv.Required(CONF_BATTERY): CV_NUM_BATTERIES,
    }
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PylontechComponent),
            cv.Required(CONF_NUM_BATTERIES): CV_NUM_BATTERIES,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def check_battery_index(index: int):
    if not (1 <= index <= num_batteries):
        raise cv.Invalid(f"battery expected between 1 and {num_batteries}")


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    global num_batteries
    num_batteries = config[CONF_NUM_BATTERIES]
    cg.add_define("PYLONTECH_NUM_BATTERIES", config[CONF_NUM_BATTERIES])
