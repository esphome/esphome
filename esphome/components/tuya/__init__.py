import re

from esphome import automation, pins
from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import time, uart
from esphome.components.sensor import Sensor
from esphome.components.text_sensor import TextSensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_SENSOR_DATAPOINT,
    CONF_TIME_ID,
    CONF_UPDATE_INTERVAL,
)
from esphome.core import CORE, ID, AutoLoad, CoroPriority, coroutine_with_priority
from esphome.cpp_generator import MockObj
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
from esphome.types import ConfigType, TemplateArgsType

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
CONF_WEATHER = "weather"
CONF_ON_WEATHER_REQUEST = "on_weather_request"

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

TuyaSendWeatherAction = tuya_ns.class_("TuyaSendWeatherAction", automation.Action)

# Tuya weather keys by the type of their value on the wire
# fmt: off
WEATHER_INT_KEYS = {
    "w.temp", "w.humidity", "w.pressure", "w.realFeel", "w.uvi", "w.windLevel",
    "w.aqi", "w.qualityLevel", "w.pm10", "w.pm25", "w.o3", "w.no2", "w.co", "w.so2",
    "w.thigh", "w.tlow",
}
WEATHER_STRING_KEYS = {
    "w.conditionNum", "w.condition", "w.tips", "w.windSpeed", "w.windDir", "w.sunrise",
    "w.sunset", "w.sunRise", "w.sunSet", "w.currdate", "w.rank", "w.quality", "c.area",
    "c.city", "c.province",
}
# fmt: on
WEATHER_KEYS = WEATHER_INT_KEYS | WEATHER_STRING_KEYS
WEATHER_FORECAST_RE = re.compile(r"^(w\.[A-Za-z0-9]+)\.([0-6])$")
# with the forecast keys w.<key>.<day>, day 0 = today
WEATHER_CONFIG_KEYS = sorted(WEATHER_KEYS) + [
    f"{key}.{day}"
    for key in sorted(WEATHER_KEYS)
    if key.startswith("w.")
    for day in range(7)
]
WEATHER_INT_RANGE = cv.int_range(min=-(2**31), max=2**31 - 1)


def weather_base_key(key: str) -> str | None:
    """The Tuya key without a forecast day suffix, or None for an unknown key."""
    m = WEATHER_FORECAST_RE.match(key)
    base = m.group(1) if m else key
    return base if base in WEATHER_KEYS else None


def weather_key_is_string(key: str) -> bool:
    return weather_base_key(key) in WEATHER_STRING_KEYS


def weather_key_takes_sensor(key: str) -> bool:
    base = weather_base_key(key)
    return base == "w.windSpeed" or base in WEATHER_INT_KEYS


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


def weather_key(value: str) -> str:
    value = cv.string_strict(value)
    if weather_base_key(value) is None:
        raise cv.Invalid(
            f"'{value}' is not a Tuya weather key; known keys: "
            f"{', '.join(sorted(WEATHER_KEYS))}, with an optional forecast day 0-6 (w.temp.1 = tomorrow)"
        )
    return value


def _weather_sources(config: ConfigType, options: tuple[str, ...]) -> list[str]:
    """The Tuya keys of a config; the schema has validated the value of every known key."""
    keys = [key for key in config if key not in options]
    for key in keys:
        try:
            weather_key(key)
        except cv.Invalid as err:
            err.prepend([key])
            raise
    return keys


WEATHER_OPTIONS = (CONF_UPDATE_INTERVAL, CONF_ON_WEATHER_REQUEST)


def _runs_script(node: object) -> bool:
    """Whether a validated automation holds a script.execute action."""
    if isinstance(node, dict):
        return "script.execute" in node or any(_runs_script(v) for v in node.values())
    return isinstance(node, list) and any(_runs_script(v) for v in node)


def validate_weather_sources(config: ConfigType) -> ConfigType:
    if (
        not _weather_sources(config, WEATHER_OPTIONS)
        and CONF_ON_WEATHER_REQUEST not in config
    ):
        raise cv.Invalid(
            "weather needs at least one Tuya key with an entity id, or on_weather_request"
        )
    # a script can delay or wait in its queue: its values would come after the answer
    if _runs_script(config.get(CONF_ON_WEATHER_REQUEST)):
        raise cv.Invalid(
            "script.execute is not allowed here: the values must be sent before the automation returns",
            [CONF_ON_WEATHER_REQUEST],
        )
    interval = config[CONF_UPDATE_INTERVAL].total_milliseconds
    if interval != cv.SCHEDULER_DONT_RUN and interval < 60000:
        raise cv.Invalid(
            "update_interval must be at least 1min, or never", [CONF_UPDATE_INTERVAL]
        )
    return config


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
    TUYA_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(TuyaUART),
            cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
            cv.Optional(CONF_FORCE_TIME_SYNC, default=False): cv.boolean,
            cv.Optional(CONF_STATUS_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_WEATHER): cv.All(
                cv.Schema(
                    {
                        cv.Optional(
                            CONF_UPDATE_INTERVAL, default="30min"
                        ): cv.update_interval,
                        cv.Optional(
                            CONF_ON_WEATHER_REQUEST
                        ): automation.validate_synchronous_automation(
                            CONF_ON_WEATHER_REQUEST, "the answer to the MCU"
                        ),
                        **{
                            cv.Optional(key): cv.use_id(
                                Sensor if weather_key_takes_sensor(key) else TextSensor
                            )
                            for key in WEATHER_CONFIG_KEYS
                        },
                    },
                    extra=cv.ALLOW_EXTRA,
                ),
                validate_weather_sources,
            ),
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
    await setup_tuya(var, config)
    if (weather := config.get(CONF_WEATHER)) is not None:
        cg.add_define("USE_TUYA_WEATHER")
        interval = weather[CONF_UPDATE_INTERVAL].total_milliseconds
        if interval == cv.SCHEDULER_DONT_RUN:
            interval = 0
        cg.add(var.set_weather_update_interval(interval))
        sources = [key for key in weather if key not in WEATHER_OPTIONS]
        sensors = sum(1 for key in sources if weather_key_takes_sensor(key))
        cg.add(var.init_weather_sources(sensors, len(sources) - sensors))
        for key in sources:
            source = await cg.get_variable(weather[key])
            if weather_key_takes_sensor(key):
                cg.add(var.add_weather_sensor(key, source))
            else:
                cg.add(var.add_weather_text_sensor(key, source))
        for conf in weather.get(CONF_ON_WEATHER_REQUEST, []):
            await automation.build_callback_automation(
                var,
                "add_on_weather_request_callback",
                [
                    (
                        cg.std_vector.template(cg.std_string)
                        .operator("const")
                        .operator("ref"),
                        "x",
                    )
                ],
                conf,
            )


def FILTER_SOURCE_FILES() -> list[str]:
    # When only auto-loaded as a base class, the UART implementation is unused
    if isinstance(CORE.config.get(DOMAIN), AutoLoad):
        return ["tuya_uart.cpp", "tuya_uart.h"]
    return []


def validate_send_weather(config: ConfigType) -> ConfigType:
    if not _weather_sources(config, (CONF_ID,)):
        raise cv.Invalid("tuya.send_weather needs at least one Tuya key")
    return config


SEND_WEATHER_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(TuyaUART),
            # the key decides the type: a string or an int32
            **{
                cv.Optional(key): cv.templatable(
                    cv.string if weather_key_is_string(key) else WEATHER_INT_RANGE
                )
                for key in WEATHER_CONFIG_KEYS
            },
        },
        extra=cv.ALLOW_EXTRA,
    ),
    validate_send_weather,
)


@automation.register_action(
    "tuya.send_weather", TuyaSendWeatherAction, SEND_WEATHER_SCHEMA, synchronous=True
)
async def tuya_send_weather_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    cg.add_define("USE_TUYA_WEATHER")
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent, len(config) - 1)
    for key, value in config.items():
        if key == CONF_ID:
            continue
        string = weather_key_is_string(key)
        if not string or cg.is_template(value):
            type_ = cg.std_string if string else cg.int32
            value = await cg.templatable(value, args, type_)
        cg.add((var.add_string if string else var.add_int)(key, value))
    return var
