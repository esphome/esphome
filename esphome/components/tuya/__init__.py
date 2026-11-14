from esphome import automation, final_validate as fv, pins
import esphome.codegen as cg
from esphome.components import time, uart
from esphome.config import iter_ids
import esphome.config_validation as cv
from esphome.const import CONF_PROTOCOL, CONF_SENSOR_DATAPOINT, CONF_TIME_ID
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

DEPENDENCIES = ["uart"]
DOMAIN = "tuya"

CONF_IGNORE_MCU_UPDATE_ON_DATAPOINTS = "ignore_mcu_update_on_datapoints"

CONF_ON_DATAPOINT_UPDATE = "on_datapoint_update"
CONF_ON_MCU_READY = "on_mcu_ready"
CONF_DATAPOINT_TYPE = "datapoint_type"
CONF_STATUS_PIN = "status_pin"
CONF_FORCE_TIME_SYNC = "force_time_sync"
CONF_AUTO_BAUD = "auto_baud"
CONF_MCU_WAKE_PIN = "mcu_wake_pin"

tuya_ns = cg.esphome_ns.namespace("tuya")
TuyaDatapointType = tuya_ns.enum("TuyaDatapointType", is_class=True)
TuyaProtocol = tuya_ns.enum("TuyaProtocol", is_class=True)
Tuya = tuya_ns.class_("Tuya", cg.Component, uart.UARTDevice)
TuyaRefreshAction = tuya_ns.class_("TuyaRefreshAction", automation.Action)

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
PROTOCOL_LOW_POWER = "low_power"


def _validate_time_options(config: ConfigType) -> ConfigType:
    if config[CONF_FORCE_TIME_SYNC] and CONF_TIME_ID not in config:
        raise cv.Invalid(
            f"{CONF_FORCE_TIME_SYNC} requires {CONF_TIME_ID}",
            path=[CONF_FORCE_TIME_SYNC],
        )
    if config[CONF_PROTOCOL] == PROTOCOL_LOW_POWER and config[CONF_FORCE_TIME_SYNC]:
        raise cv.Invalid(
            "force_time_sync is unsupported for low_power; CMD06 is MCU initiated",
            path=[CONF_FORCE_TIME_SYNC],
        )
    if config[CONF_PROTOCOL] == PROTOCOL_LOW_POWER and CONF_STATUS_PIN in config:
        raise cv.Invalid(
            "status_pin is unsupported for low_power; wake is configured separately",
            path=[CONF_STATUS_PIN],
        )
    if CONF_MCU_WAKE_PIN in config and config[CONF_PROTOCOL] != PROTOCOL_LOW_POWER:
        raise cv.Invalid(
            f"{CONF_MCU_WAKE_PIN} requires protocol: low_power",
            path=[CONF_MCU_WAKE_PIN],
        )
    if config[CONF_AUTO_BAUD] and config[CONF_PROTOCOL] != PROTOCOL_LOW_POWER:
        raise cv.Invalid(
            "auto_baud is only supported with protocol: low_power",
            path=[CONF_AUTO_BAUD],
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Tuya),
            cv.Optional(CONF_PROTOCOL, default="standard"): cv.one_of(
                "standard", PROTOCOL_LOW_POWER, lower=True
            ),
            cv.Optional(CONF_AUTO_BAUD, default=False): cv.boolean,
            cv.Optional(CONF_MCU_WAKE_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
            cv.Optional(CONF_FORCE_TIME_SYNC, default=False): cv.boolean,
            cv.Optional(CONF_IGNORE_MCU_UPDATE_ON_DATAPOINTS): cv.ensure_list(
                cv.uint8_t
            ),
            cv.Optional(CONF_STATUS_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_ON_MCU_READY): automation.validate_automation({}),
            cv.Optional(CONF_ON_DATAPOINT_UPDATE): automation.validate_automation(
                {
                    cv.Required(CONF_SENSOR_DATAPOINT): cv.uint8_t,
                    cv.Optional(CONF_DATAPOINT_TYPE, default=DPTYPE_ANY): cv.one_of(
                        *DATAPOINT_TYPES, lower=True
                    ),
                }
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA),
    _validate_time_options,
)


async def to_code(config):
    var = await uart.new_uart_component(config)
    if config[CONF_PROTOCOL] == PROTOCOL_LOW_POWER:
        # Standard Tuya builds must not carry the low-power driver code or state.
        cg.add_define("USE_TUYA_LOW_POWER")
        cg.add(var.set_protocol(TuyaProtocol.TUYA_PROTOCOL_LOW_POWER))
        cg.add(var.set_auto_baud(config[CONF_AUTO_BAUD]))
        await cg.add_pin(var.set_mcu_wake_pin, config.get(CONF_MCU_WAKE_PIN))
    if (time_id := config.get(CONF_TIME_ID)) is not None:
        await cg.add_variable(var.set_time_id, time_id)
        if config[CONF_FORCE_TIME_SYNC]:
            cg.add(var.set_force_time_sync(True))
    await cg.add_pin(var.set_status_pin, config.get(CONF_STATUS_PIN))
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
    for conf in config.get(CONF_ON_MCU_READY, []):
        await automation.build_callback_automation(
            var, "add_on_mcu_ready_callback", [], conf
        )


def _validate_refresh_actions(config: ConfigType) -> None:
    """Reject tuya.refresh unless the referenced Tuya hub uses low_power."""
    full_config = fv.full_config.get()
    for tuya_id, path in iter_ids(full_config):
        if len(path) < 2 or path[-2:] != ["tuya.refresh", CONF_TUYA_ID]:
            continue
        hub = fv.get_declaring_config(tuya_id, full_config)
        if hub.get(CONF_PROTOCOL) != PROTOCOL_LOW_POWER:
            with cv.prepend_path([cv.ROOT_CONFIG_PATH, *path]):
                raise cv.Invalid("tuya.refresh requires protocol: low_power")


FINAL_VALIDATE_SCHEMA = _validate_refresh_actions


@automation.register_action(
    "tuya.refresh",
    TuyaRefreshAction,
    cv.Schema({cv.Required(CONF_TUYA_ID): cv.use_id(Tuya)}),
    synchronous=True,
)
async def tuya_refresh_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_TUYA_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)
