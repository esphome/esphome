import logging
from typing import Any

from esphome import automation
import esphome.codegen as cg
from esphome.components.const import CONF_MANUFACTURER
from esphome.components.esp32 import DOMAIN as ESP32_DOMAIN, only_on_variant
from esphome.components.esp32.const import (
    VARIANT_ESP32C5,
    VARIANT_ESP32C6,
    VARIANT_ESP32H2,
    VARIANT_ESP32H4,
    VARIANT_ESP32H21,
    VARIANT_ESP32S31,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_ESPHOME,
    CONF_ID,
    CONF_INTERNAL,
    CONF_MODEL,
    CONF_NAME,
    CONF_ON_START,
    CONF_POLLING_INTERVAL,
    CONF_PROJECT,
    CONF_VERSION,
    __version__ as ESPHOME_VERSION,
)
from esphome.core import CORE, CoroPriority, coroutine_with_priority, entity_helpers
import esphome.final_validate as fv
from esphome.types import ConfigType

# CONF_CLUSTER, CONF_ENDPOINT, CONF_MAX_EP_NUMBER, CONF_REPORT,
# CONF_USE_DEVICE_TYPE and REPORT are re-exported for existing consumers of
# this package's namespace.
from .const import (  # noqa: F401
    CONF_CLUSTER,
    CONF_ENDPOINT,
    CONF_MAX_EP_NUMBER,
    CONF_ON_JOIN,
    CONF_POWER_SOURCE,
    CONF_REPORT,
    CONF_ROUTER,
    CONF_SLEEPY,
    CONF_USE_DEVICE_TYPE,
    CONF_WIPE_ON_BOOT,
    KEY_ZIGBEE,
    KEY_ZIGBEE_AREA_STR,
    KEY_ZIGBEE_VERSION_STR,
    POWER_SOURCE,
    REPORT,
    ZigbeeComponent,
)
from .const_zephyr import (
    CONF_IEEE802154_VENDOR_OUI,
    CONF_MAX_EP_NUMBER_ZEPHYR,
    CONF_ZIGBEE_ID,
    KEY_EP_NUMBER,
)
from .zigbee_esp32 import (
    final_validate_esp32,
    validate_binary_sensor_esp32,
    validate_number_esp32,
    validate_sensor_esp32,
    validate_switch_esp32,
    zigbee_require_vfs_select,
)
from .zigbee_zephyr import zephyr_number, zephyr_sensor, zephyr_switch

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@luar123", "@tomaszduda23"]
DOMAIN = "zigbee"

CONFLICTS_WITH = ["openthread"]


# Defined in esphome.core.entity_helpers so entity base schemas can reference
# them without importing this package; re-exported here for existing consumers.
BASE_SCHEMA = entity_helpers.ZIGBEE_BASE_ENTITY_SCHEMA
BINARY_SENSOR_SCHEMA = entity_helpers.ZIGBEE_BINARY_SENSOR_SCHEMA
SENSOR_SCHEMA = (
    cv.Schema({})
    .extend(entity_helpers.zigbee_base_entity_schema(["generic", "device_class"]))
    .extend(zephyr_sensor)
)
SWITCH_SCHEMA = (
    cv.Schema({})
    .extend(entity_helpers.zigbee_base_entity_schema(["generic", "on_off"]))
    .extend(zephyr_switch)
)
NUMBER_SCHEMA = (
    cv.Schema({})
    .extend(entity_helpers.zigbee_base_entity_schema(["generic"]))
    .extend(zephyr_number)
)


def _validate_router_sleepy(config: ConfigType) -> ConfigType:
    if config.get(CONF_ROUTER):
        if config.get(CONF_SLEEPY):
            raise cv.Invalid("router and sleepy are mutually exclusive")
        if config.get(CONF_POLLING_INTERVAL):
            raise cv.Invalid(f"{CONF_POLLING_INTERVAL} is only valid for end devices.")
    return config


def _validate_ascii(value):
    """Validate that a string contains only ascii characters."""
    if not isinstance(value, str):
        raise cv.Invalid("expected string")
    if not value.isascii():
        raise cv.Invalid("Only ASCII characters are allowed")
    return value


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(ZigbeeComponent),
            cv.Optional(CONF_MODEL, default=CORE.name): cv.All(
                cv.string,
                cv.Length(max=31),
                _validate_ascii,
            ),
            cv.Optional(
                CONF_MANUFACTURER, default="esphome", visibility=cv.Visibility.YAML_ONLY
            ): cv.All(
                cv.string,
                cv.Length(max=31),
                _validate_ascii,
            ),
            cv.Optional(CONF_VERSION, default=0): cv.uint8_t,
            cv.Optional(CONF_ROUTER, default=False): cv.boolean,
            cv.Optional(CONF_ON_JOIN): automation.validate_automation({}),
            cv.Optional(CONF_ON_START): automation.validate_automation({}),
            cv.OnlyWith(CONF_WIPE_ON_BOOT, "nrf52", default=False): cv.All(
                cv.Any(
                    cv.boolean,
                    cv.one_of(*["once"], lower=True),
                ),
                cv.requires_component("nrf52"),
            ),
            cv.Optional(CONF_POWER_SOURCE, default="DC_SOURCE"): cv.enum(
                POWER_SOURCE, upper=True
            ),
            cv.Optional(CONF_IEEE802154_VENDOR_OUI): cv.All(
                cv.Any(
                    cv.int_range(min=0x000000, max=0xFFFFFF),
                    cv.one_of(*["random"], lower=True),
                ),
                cv.requires_component("nrf52"),
            ),
            cv.Optional(CONF_SLEEPY, default=False): cv.All(
                cv.boolean,
            ),
            cv.Optional(CONF_POLLING_INTERVAL): cv.All(
                cv.requires_component(ESP32_DOMAIN),
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=cv.TimePeriodMilliseconds(milliseconds=200),
                    max=cv.TimePeriodMinutes(minutes=60),  # 1 hour
                ),
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_router_sleepy,
    zigbee_require_vfs_select,
    cv.Any(
        cv.All(
            cv.only_on_esp32,
            only_on_variant(
                supported=[
                    VARIANT_ESP32S31,
                    VARIANT_ESP32H2,
                    VARIANT_ESP32H21,
                    VARIANT_ESP32H4,
                    VARIANT_ESP32C5,
                    VARIANT_ESP32C6,
                ]
            ),
        ),
        cv.only_with_framework("zephyr"),
    ),
)


def validate_number_of_ep(config: ConfigType) -> ConfigType:
    if not CORE.is_nrf52:
        return config
    if KEY_EP_NUMBER not in CORE.data.get(KEY_ZIGBEE, {}):
        raise cv.Invalid("At least one zigbee device need to be included")
    count = len(CORE.data[KEY_ZIGBEE][KEY_EP_NUMBER])
    if count == 1:
        _LOGGER.warning(
            "Single endpoint requires ZHA or at leatst Zigbee2MQTT 2.8.0. For older versions of Zigbee2MQTT use multiple endpoints"
        )
    if count > CONF_MAX_EP_NUMBER_ZEPHYR and not CORE.testing_mode:
        raise cv.Invalid(f"Maximum number of end points is {CONF_MAX_EP_NUMBER_ZEPHYR}")

    return config


def final_validate_strings(config: ConfigType) -> ConfigType:
    version_str = ESPHOME_VERSION
    if (
        proj_version := fv.full_config.get()
        .get(CONF_ESPHOME, {})
        .get(CONF_PROJECT, {})
        .get(CONF_VERSION)
    ):
        version_str = proj_version.encode("ascii", "ignore")[:15].decode()
        if version_str != proj_version:
            _LOGGER.warning(
                "esphome.project.version %s was changed to %s for use in Zigbee attributes. Use only a maximum of 15 ASCII characters",
                proj_version,
                version_str,
            )
    area_str = ""
    if CORE.area:
        area_str = CORE.area.encode("ascii", "ignore")[:15].decode()
        if area_str != CORE.area:
            _LOGGER.warning(
                "esphome.area %s was changed to %s for use in Zigbee attributes. Use only a maximum of 15 ASCII characters",
                CORE.area,
                area_str,
            )
    data: dict[str, Any] = CORE.data.setdefault(KEY_ZIGBEE, {})
    data[KEY_ZIGBEE_VERSION_STR] = version_str
    data[KEY_ZIGBEE_AREA_STR] = area_str
    return config


FINAL_VALIDATE_SCHEMA = cv.All(
    final_validate_strings,
    validate_number_of_ep,
    final_validate_esp32,
)


_CALLBACK_AUTOMATIONS = [
    automation.CallbackAutomation(CONF_ON_JOIN, "add_on_join_callback", [(bool, "x")]),
    automation.CallbackAutomation(CONF_ON_START, "add_on_start_callback", []),
]


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_ZIGBEE")
    var = None
    if CORE.using_zephyr:
        from .zigbee_zephyr import zephyr_to_code

        var = await zephyr_to_code(config)
    if CORE.is_esp32:
        from .zigbee_esp32 import esp32_to_code

        var = await esp32_to_code(config)
    if var is not None:
        await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


async def setup_binary_sensor(entity: cg.MockObj, config: ConfigType) -> None:
    if DOMAIN not in CORE.loaded_integrations or config.get(CONF_INTERNAL):
        return
    if CORE.using_zephyr:
        if not config.get(CONF_ZIGBEE_ID):
            return
        from .zigbee_zephyr import add_binary_sensor
    else:
        from .zigbee_esp32 import add_component as add_binary_sensor
    CORE.add_job(add_binary_sensor, entity, config)


async def setup_sensor(entity: cg.MockObj, config: ConfigType) -> None:
    if DOMAIN not in CORE.loaded_integrations or config.get(CONF_INTERNAL):
        return
    if CORE.using_zephyr:
        if not config.get(CONF_ZIGBEE_ID):
            return
        from .zigbee_zephyr import add_sensor
    else:
        from .zigbee_esp32 import add_component as add_sensor
    CORE.add_job(add_sensor, entity, config)


async def setup_switch(entity: cg.MockObj, config: ConfigType) -> None:
    if DOMAIN not in CORE.loaded_integrations or config.get(CONF_INTERNAL):
        return
    if CORE.using_zephyr:
        if not config.get(CONF_ZIGBEE_ID):
            return
        from .zigbee_zephyr import add_switch
    else:
        from .zigbee_esp32 import add_component as add_switch
    CORE.add_job(add_switch, entity, config)


async def setup_number(
    entity: cg.MockObj,
    config: ConfigType,
    min_value: float,
    max_value: float,
    step: float,
) -> None:
    if DOMAIN not in CORE.loaded_integrations or config.get(CONF_INTERNAL):
        return
    if CORE.using_zephyr:
        if not config.get(CONF_ZIGBEE_ID):
            return
        from .zigbee_zephyr import zephyr_setup_number

        await zephyr_setup_number(entity, config, min_value, max_value, step)
    else:
        from .zigbee_esp32 import add_component as add_number

        CORE.add_job(add_number, entity, config)


def consume_endpoint(config: ConfigType) -> ConfigType:
    if not config.get(CONF_ZIGBEE_ID):
        return config
    if CONF_NAME in config and " " in config[CONF_NAME]:
        _LOGGER.warning(
            "Spaces in '%s' requires ZHA or at least Zigbee2MQTT 2.8.0. For older version of Zigbee2MQTT use '%s'",
            config[CONF_NAME],
            config[CONF_NAME].replace(" ", "_"),
        )
    data: dict[str, Any] = CORE.data.setdefault(KEY_ZIGBEE, {})
    slots: list[str] = data.setdefault(KEY_EP_NUMBER, [])
    slots.extend([""])
    return config


def validate_binary_sensor(config: ConfigType) -> ConfigType:
    if DOMAIN not in CORE.loaded_integrations or config.get(CONF_INTERNAL):
        return config
    if CORE.is_esp32:
        return validate_binary_sensor_esp32(config)
    return consume_endpoint(config)


def validate_sensor(config: ConfigType) -> ConfigType:
    if DOMAIN not in CORE.loaded_integrations or config.get(CONF_INTERNAL):
        return config
    if CORE.is_esp32:
        return validate_sensor_esp32(config)
    return consume_endpoint(config)


def validate_switch(config: ConfigType) -> ConfigType:
    if DOMAIN not in CORE.loaded_integrations or config.get(CONF_INTERNAL):
        return config
    if CORE.is_esp32:
        return validate_switch_esp32(config)
    return consume_endpoint(config)


def validate_number(config: ConfigType) -> ConfigType:
    if DOMAIN not in CORE.loaded_integrations or config.get(CONF_INTERNAL):
        return config
    if CORE.is_esp32:
        return validate_number_esp32(config)
    return consume_endpoint(config)


ZIGBEE_ACTION_SCHEMA = automation.maybe_simple_id(
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(ZigbeeComponent),
        }
    )
)

automation.register_apply_action(
    "zigbee.factory_reset",
    ZIGBEE_ACTION_SCHEMA,
    automation.ApplyCall("factory_reset()"),
)
