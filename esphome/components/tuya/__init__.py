from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import time, uart
import esphome.config_validation as cv
from esphome.const import CONF_SENSOR_DATAPOINT, CONF_TIME_ID
from esphome.core import CORE, AutoLoad
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

DOMAIN = "tuya"

CONF_IGNORE_MCU_UPDATE_ON_DATAPOINTS = "ignore_mcu_update_on_datapoints"

CONF_ON_DATAPOINT_UPDATE = "on_datapoint_update"
CONF_DATAPOINT_TYPE = "datapoint_type"
CONF_STATUS_PIN = "status_pin"
CONF_FORCE_TIME_SYNC = "force_time_sync"

tuya_ns = cg.esphome_ns.namespace("tuya")
TuyaDatapointType = tuya_ns.enum("TuyaDatapointType", is_class=True)
Tuya = tuya_ns.class_("Tuya", cg.Component)
TuyaUART = tuya_ns.class_("TuyaUART", Tuya, uart.UARTDevice)

TuyaDatapoint = tuya_ns.struct("TuyaDatapoint")

DPTYPE_ANY = "any"
DPTYPE_RAW = "raw"
DPTYPE_BOOL = "bool"
DPTYPE_INT = "int"
DPTYPE_UINT = "uint"
DPTYPE_STRING = "string"
DPTYPE_ENUM = "enum"
DPTYPE_BITMASK = "bitmask"

# Automation argument type, plus the expected TuyaDatapointType and the field forwarded
DATAPOINT_TYPES = {
    DPTYPE_ANY: (TuyaDatapoint, None, None),
    DPTYPE_RAW: (cg.std_vector.template(cg.uint8), TuyaDatapointType.RAW, "value_raw"),
    DPTYPE_BOOL: (cg.bool_, TuyaDatapointType.BOOLEAN, "value_bool"),
    DPTYPE_INT: (cg.int_, TuyaDatapointType.INTEGER, "value_int"),
    DPTYPE_UINT: (cg.uint32, TuyaDatapointType.INTEGER, "value_uint"),
    DPTYPE_STRING: (cg.std_string, TuyaDatapointType.STRING, "value_string"),
    DPTYPE_ENUM: (cg.uint8, TuyaDatapointType.ENUM, "value_enum"),
    DPTYPE_BITMASK: (cg.uint32, TuyaDatapointType.BITMASK, "value_bitmask"),
}


CONF_TUYA_ID = "tuya_id"


def _validate_time_options(config: ConfigType) -> ConfigType:
    if config[CONF_FORCE_TIME_SYNC] and CONF_TIME_ID not in config:
        raise cv.Invalid(
            f"{CONF_FORCE_TIME_SYNC} requires {CONF_TIME_ID}",
            path=[CONF_FORCE_TIME_SYNC],
        )
    return config


# Options handled by the transport-agnostic Tuya base class, shared by all
# transport implementations. Transport components extend this schema and call
# register_tuya() from their to_code().
BASE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_IGNORE_MCU_UPDATE_ON_DATAPOINTS): cv.ensure_list(cv.uint8_t),
        cv.Optional(CONF_ON_DATAPOINT_UPDATE): automation.validate_automation(
            {
                cv.Required(CONF_SENSOR_DATAPOINT): cv.uint8_t,
                cv.Optional(CONF_DATAPOINT_TYPE, default=DPTYPE_ANY): cv.one_of(
                    *DATAPOINT_TYPES, lower=True
                ),
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)

UART_SCHEMA = cv.All(
    BASE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(TuyaUART),
            cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
            cv.Optional(CONF_FORCE_TIME_SYNC, default=False): cv.boolean,
            cv.Optional(CONF_STATUS_PIN): pins.gpio_output_pin_schema,
        }
    ).extend(uart.UART_DEVICE_SCHEMA),
    _validate_time_options,
)


def _validate_config(value):
    # When auto-loaded as a base class by another transport component, there
    # is no `tuya:` config block and no UART device to validate.
    if isinstance(value, AutoLoad):
        return value
    return UART_SCHEMA(value)


CONFIG_SCHEMA = _validate_config


async def register_tuya(var, config):
    """Register config options handled by the Tuya base class.

    Called by transport components (UART here, others such as TCP elsewhere)
    after creating their component instance.
    """
    if CONF_IGNORE_MCU_UPDATE_ON_DATAPOINTS in config:
        for dp in config[CONF_IGNORE_MCU_UPDATE_ON_DATAPOINTS]:
            cg.add(var.add_ignore_mcu_update_on_datapoints(dp))
    for conf in config.get(CONF_ON_DATAPOINT_UPDATE, []):
        type_, expected, field = DATAPOINT_TYPES[conf[CONF_DATAPOINT_TYPE]]
        forward = None
        if expected is not None:
            forward = [getattr(MockObj("x", ".").expect_type(expected), field)]
        callback = await automation.build_trigger_callback(
            [(type_, "x")], conf, params=[(TuyaDatapoint, "x")], forward=forward
        )
        cg.add(var.register_listener(conf[CONF_SENSOR_DATAPOINT], callback))


async def to_code(config):
    # Auto-loaded as a base class only; no component to instantiate.
    if isinstance(config, AutoLoad):
        return

    var = await uart.new_uart_component(config)
    if (time_id := config.get(CONF_TIME_ID)) is not None:
        await cg.add_variable(var.set_time_id, time_id)
        if config[CONF_FORCE_TIME_SYNC]:
            cg.add(var.set_force_time_sync(True))
    await cg.add_pin(var.set_status_pin, config.get(CONF_STATUS_PIN))
    await register_tuya(var, config)


def FILTER_SOURCE_FILES() -> list[str]:
    # When only auto-loaded as a base class, the UART implementation is unused
    if isinstance(CORE.config.get(DOMAIN), AutoLoad):
        return ["tuya_uart.cpp", "tuya_uart.h"]
    return []
