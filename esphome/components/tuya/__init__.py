from esphome import automation, pins
from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import time, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACTION,
    CONF_HEARTBEAT,
    CONF_ID,
    CONF_INVERTED,
    CONF_RX_PIN,
    CONF_SENSOR_DATAPOINT,
    CONF_TIME_ID,
    CONF_TX_PIN,
    CONF_UART_ID,
)
from esphome.core import CORE, AutoLoad, CoroPriority, coroutine_with_priority
from esphome.cpp_generator import MockObj
import esphome.final_validate as fv
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
from esphome.types import ConfigType

CODEOWNERS = ["@dwmw2", "@ssieb"]
DOMAIN = "tuya"


# AutoLoad: another transport pulled in only the base class. None: CI dependency probe.
def AUTO_LOAD(config: ConfigType | AutoLoad | None) -> list[str]:
    if isinstance(config, AutoLoad):
        return []
    return ["uart"]


CONF_IGNORE_MCU_UPDATE_ON_DATAPOINTS = "ignore_mcu_update_on_datapoints"

CONF_ON_DATAPOINT_UPDATE = "on_datapoint_update"
CONF_DATAPOINT_TYPE = "datapoint_type"
CONF_STATUS_PIN = "status_pin"
CONF_FORCE_TIME_SYNC = "force_time_sync"
CONF_MCU_WATCHDOG = "mcu_watchdog"
CONF_MISSED_HEARTBEATS = "missed_heartbeats"
CONF_RESET_TIME = "reset_time"
CONF_DATAPOINT_ACK = "datapoint_ack"

ACTION_REBOOT = "reboot"
ACTION_RESET = "reset"
ACTION_DISABLE = "disable"

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

# The hub's dump_config prints each child's dump
DUMP_PUBLISHER = cg.HubFunction("dump", cg.void, [], define="USE_TUYA_DUMP_PUBLISHER")


@coroutine_with_priority(CoroPriority.FINAL)
async def _register_listeners(var: MockObj) -> None:
    cg.add(var.register_listeners())


def register_tuya_child(hub: MockObj, var: MockObj) -> None:
    """Register the datapoint listeners of ``var`` after all other code, so the hub's own listeners come first."""
    CORE.add_job(_register_listeners, var)
    DUMP_PUBLISHER.add(hub, parent_ref(var).dump_child_config())


def _validate_time_options(config: ConfigType) -> ConfigType:
    if config[CONF_FORCE_TIME_SYNC] and CONF_TIME_ID not in config:
        raise cv.Invalid(
            f"{CONF_FORCE_TIME_SYNC} requires {CONF_TIME_ID}",
            path=[CONF_FORCE_TIME_SYNC],
        )
    return config


def _validate_heartbeat(config: ConfigType) -> ConfigType:
    # runs before the defaults: only a watchdog set in the config is rejected
    if not isinstance(config, dict):
        return config  # the schema reports the wrong type
    try:
        if cv.boolean(config.get(CONF_HEARTBEAT, True)):
            return config
    except cv.Invalid:
        return config  # the schema reports it at the key
    watchdog = config.get(CONF_MCU_WATCHDOG) or {}
    if not isinstance(watchdog, dict):
        return config
    action = str(watchdog.get(CONF_ACTION, "")).lower()
    if CONF_MCU_WATCHDOG in config and action != ACTION_DISABLE:
        raise cv.Invalid(
            f"{CONF_MCU_WATCHDOG} requires {CONF_HEARTBEAT}",
            path=[CONF_MCU_WATCHDOG],
        )
    return config


_MISSED_HEARTBEATS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_MISSED_HEARTBEATS, default=6): cv.int_range(min=1, max=20),
    }
)

# the options of each action: one without effect is rejected
MCU_WATCHDOG_SCHEMA = cv.typed_schema(
    {
        ACTION_REBOOT: _MISSED_HEARTBEATS_SCHEMA,
        # holds TX low for reset_time, which restarts the MCU's UART
        ACTION_RESET: _MISSED_HEARTBEATS_SCHEMA.extend(
            {
                cv.Optional(CONF_RESET_TIME, default="500ms"): cv.All(
                    cv.positive_time_period_milliseconds,
                    cv.Range(
                        min=cv.TimePeriod(milliseconds=50),
                        max=cv.TimePeriod(seconds=5),
                    ),
                ),
            }
        ),
        ACTION_DISABLE: cv.Schema({}),
    },
    key=CONF_ACTION,
    default_type=ACTION_REBOOT,
    lower=True,
)


TUYA_SCHEMA = cv.Schema(
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
    _validate_heartbeat,
    TUYA_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(TuyaUART),
            cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
            cv.Optional(CONF_FORCE_TIME_SYNC, default=False): cv.boolean,
            cv.Optional(CONF_STATUS_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_HEARTBEAT, default=True): cv.boolean,
            cv.Optional(CONF_DATAPOINT_ACK, default=False): cv.boolean,
            cv.Optional(CONF_MCU_WATCHDOG, default={}): MCU_WATCHDOG_SCHEMA,
        }
    ).extend(uart.UART_DEVICE_SCHEMA),
    _validate_time_options,
)


@schema_extractor("schema")
def _validate_config(value: ConfigType | AutoLoad) -> ConfigType | AutoLoad | cv.Schema:
    if value is SCHEMA_EXTRACT:
        # Expose the UART schema to the language-schema dumper
        return UART_SCHEMA
    if isinstance(value, AutoLoad):
        return value
    return UART_SCHEMA(value)


CONFIG_SCHEMA = _validate_config


def _final_validate(config: ConfigType | AutoLoad) -> ConfigType | AutoLoad:
    if isinstance(config, AutoLoad):
        return config
    if config[CONF_MCU_WATCHDOG][CONF_ACTION] != ACTION_RESET:
        return config
    # the reset holds the TX pin of the bus low
    path = [CONF_MCU_WATCHDOG, CONF_ACTION]
    bus = fv.get_declaring_config(config[CONF_UART_ID])
    if str(bus[CONF_ID].type) not in uart.NATIVE_UART_CLASSES:
        raise cv.Invalid(
            f"{CONF_ACTION}: {ACTION_RESET} requires a UART bus of the microcontroller itself",
            path=path,
        )
    # without the RX pin no heartbeat reply would end the resets
    for pin in (CONF_TX_PIN, CONF_RX_PIN):
        if pin not in bus:
            raise cv.Invalid(
                f"{CONF_ACTION}: {ACTION_RESET} requires a UART bus with a {pin}",
                path=path,
            )
    if bus[CONF_TX_PIN].get(CONF_INVERTED, False):
        raise cv.Invalid(
            f"{CONF_ACTION}: {ACTION_RESET} requires a UART bus with a {CONF_TX_PIN} that is not inverted",
            path=path,
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def setup_tuya(var: MockObj, config: ConfigType) -> None:
    """Apply the base-class options; called from a transport's to_code()."""
    for dp in config.get(CONF_IGNORE_MCU_UPDATE_ON_DATAPOINTS, []):
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


async def to_code(config: ConfigType | AutoLoad) -> None:
    if isinstance(config, AutoLoad):
        return

    var = await uart.new_uart_component(config)
    if (time_id := config.get(CONF_TIME_ID)) is not None:
        await cg.add_variable(var.set_time_id, time_id)
        if config[CONF_FORCE_TIME_SYNC]:
            cg.add(var.set_force_time_sync(True))
    await cg.add_pin(var.set_status_pin, config.get(CONF_STATUS_PIN))
    if not config[CONF_HEARTBEAT]:
        cg.add(var.set_heartbeat(False))
    if config[CONF_DATAPOINT_ACK]:
        cg.add(var.set_datapoint_ack(True))
    watchdog = config[CONF_MCU_WATCHDOG]
    if config[CONF_HEARTBEAT] and watchdog[CONF_ACTION] != ACTION_DISABLE:
        cg.add_define("USE_TUYA_MCU_WATCHDOG")
        cg.add(var.set_mcu_watchdog(watchdog[CONF_MISSED_HEARTBEATS]))
        if watchdog[CONF_ACTION] == ACTION_RESET:
            cg.add_define("USE_TUYA_MCU_WATCHDOG_RESET")
            uart.request_tx_break()
            cg.add(
                var.set_mcu_watchdog_reset(watchdog[CONF_RESET_TIME].total_milliseconds)
            )
    await setup_tuya(var, config)


def FILTER_SOURCE_FILES() -> list[str]:
    # When only auto-loaded as a base class, the UART implementation is unused
    if isinstance(CORE.config.get(DOMAIN), AutoLoad):
        return ["tuya_uart.cpp", "tuya_uart.h"]
    return []
