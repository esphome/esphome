import logging
from typing import Any

from esphome import automation, core
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INTERNAL, CONF_MODEL, CONF_NAME, CONF_ON_START
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.types import ConfigType

from .const import (
    CONF_ENDPOINT,
    CONF_MAX_EP_NUMBER,
    CONF_ON_JOIN,
    CONF_POWER_SOURCE,
    CONF_REPORT,
    CONF_ROUTER,
    CONF_USE_DEVICE_TYPE,
    CONF_WIPE_ON_BOOT,
    KEY_ZIGBEE,
    POWER_SOURCE,
    REPORT,
    ZigbeeComponent,
    zigbee_ns,
)
from .const_zephyr import (
    CONF_IEEE802154_VENDOR_OUI,
    CONF_MAX_EP_NUMBER_ZEPHYR,
    CONF_SLEEPY,
    CONF_ZIGBEE_BINARY_SENSOR,
    CONF_ZIGBEE_ID,
    CONF_ZIGBEE_NUMBER,
    CONF_ZIGBEE_SENSOR,
    CONF_ZIGBEE_SWITCH,
    KEY_EP_NUMBER,
)
from .zigbee_esp32 import (
    final_validate_esp32,
    validate_binary_sensor_esp32,
    validate_sensor_esp32,
    zigbee_esp32_supported,
    zigbee_require_vfs_select,
)
from .zigbee_zephyr import (
    ZigbeeBinarySensor,
    ZigbeeNumber,
    ZigbeeSensor,
    ZigbeeSwitch,
    requires_zigbee_zephyr_supported,
    zephyr_binary_sensor,
    zephyr_number,
    zephyr_sensor,
    zephyr_switch,
    zigbee_zephyr_supported,
)

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@luar123", "@tomaszduda23"]

CONFLICTS_WITH = ["openthread"]


def _check_report_deprecation(value: str) -> str:
    if str(value).lower() in ("coordinator", "enable"):
        _LOGGER.warning(
            "Report options 'coordinator' and 'enable' are deprecated and will be removed in a future release. Use 'default' instead."
        )
    return value


BASE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_REPORT): cv.All(
            cv.requires_component("zigbee"),
            cv.requires_component("esp32"),
            _check_report_deprecation,
            cv.enum(REPORT, lower=True),
        ),
        cv.Optional(CONF_ENDPOINT): cv.All(
            cv.requires_component("zigbee"),
            cv.requires_component("esp32"),
            cv.int_range(1, CONF_MAX_EP_NUMBER),
        ),
        cv.Optional(CONF_USE_DEVICE_TYPE): cv.All(
            cv.requires_component("zigbee"),
            cv.requires_component("esp32"),
            cv.boolean,
        ),
    }
)
BINARY_SENSOR_SCHEMA = cv.Schema({}).extend(BASE_SCHEMA).extend(zephyr_binary_sensor)
SENSOR_SCHEMA = cv.Schema({}).extend(BASE_SCHEMA).extend(zephyr_sensor)
SWITCH_SCHEMA = cv.Schema({}).extend(zephyr_switch)
NUMBER_SCHEMA = cv.Schema({}).extend(zephyr_number)


def _validate_router_sleepy(config: ConfigType) -> ConfigType:
    if config.get(CONF_ROUTER) and config.get(CONF_SLEEPY):
        raise cv.Invalid("router and sleepy are mutually exclusive")
    return config


def _require_zigbee_supported(config: ConfigType) -> ConfigType:
    """The zigbee: component itself just needs *some* zigbee radio stack -- either
    ESP32's native one or ZBOSS (nrf52/zephyr-zigbee). Individual ZBOSS-only options
    (wipe_on_boot, ieee802154_vendor_oui, ...) separately require
    requires_zigbee_zephyr_supported."""
    if not (zigbee_esp32_supported() or zigbee_zephyr_supported()):
        raise cv.Invalid(
            "This option requires a zigbee-capable ESP32 variant, platform: nrf52, "
            "or platform: zephyr with a zigbee-capable variant"
        )
    return config


def _require_zigbee_zephyr_supported_if_set(value):
    """wipe_on_boot is ZBOSS-only, but its default (False) must stay valid on ESP32
    too -- only enforce requires_zigbee_zephyr_supported when a value was actually
    requested, not on the harmless untouched default."""
    if value:
        requires_zigbee_zephyr_supported(value)
    return value


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(ZigbeeComponent),
            cv.Optional(CONF_MODEL, default=CORE.name): cv.All(
                cv.string, cv.Length(max=31)
            ),
            cv.Optional(CONF_ROUTER, default=False): cv.boolean,
            cv.Optional(CONF_ON_JOIN): automation.validate_automation({}),
            cv.Optional(CONF_ON_START): automation.validate_automation({}),
            cv.Optional(CONF_WIPE_ON_BOOT, default=False): cv.All(
                cv.Any(
                    cv.boolean,
                    cv.one_of(*["once"], lower=True),
                ),
                _require_zigbee_zephyr_supported_if_set,
            ),
            cv.Optional(CONF_POWER_SOURCE, default="DC_SOURCE"): cv.enum(
                POWER_SOURCE, upper=True
            ),
            cv.Optional(CONF_IEEE802154_VENDOR_OUI): cv.All(
                cv.Any(
                    cv.int_range(min=0x000000, max=0xFFFFFF),
                    cv.one_of(*["random"], lower=True),
                ),
                requires_zigbee_zephyr_supported,
            ),
            cv.Optional(CONF_SLEEPY, default=False): cv.All(
                cv.boolean,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_router_sleepy,
    zigbee_require_vfs_select,
    _require_zigbee_supported,
)


def validate_number_of_ep(config: ConfigType) -> ConfigType:
    if not zigbee_zephyr_supported():
        return config
    if KEY_ZIGBEE not in CORE.data:
        raise cv.Invalid("At least one zigbee device need to be included")
    count = len(CORE.data[KEY_ZIGBEE][KEY_EP_NUMBER])
    if count == 1:
        _LOGGER.warning(
            "Single endpoint requires ZHA or at leatst Zigbee2MQTT 2.8.0. For older versions of Zigbee2MQTT use multiple endpoints"
        )
    if count > CONF_MAX_EP_NUMBER_ZEPHYR and not CORE.testing_mode:
        raise cv.Invalid(f"Maximum number of end points is {CONF_MAX_EP_NUMBER_ZEPHYR}")

    return config


FINAL_VALIDATE_SCHEMA = cv.All(
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
    if not config.get(CONF_ZIGBEE_ID) or config.get(CONF_INTERNAL):
        return
    if CORE.using_zephyr:
        from .zigbee_zephyr import zephyr_setup_binary_sensor

        await zephyr_setup_binary_sensor(entity, config)


async def setup_sensor(entity: cg.MockObj, config: ConfigType) -> None:
    if not config.get(CONF_ZIGBEE_ID) or config.get(CONF_INTERNAL):
        return
    if CORE.using_zephyr:
        from .zigbee_zephyr import zephyr_setup_sensor

        await zephyr_setup_sensor(entity, config)


async def setup_switch(entity: cg.MockObj, config: ConfigType) -> None:
    if not config.get(CONF_ZIGBEE_ID) or config.get(CONF_INTERNAL):
        return
    if CORE.using_zephyr:
        from .zigbee_zephyr import zephyr_setup_switch

        await zephyr_setup_switch(entity, config)


async def setup_number(
    entity: cg.MockObj,
    config: ConfigType,
    min_value: float,
    max_value: float,
    step: float,
) -> None:
    if not config.get(CONF_ZIGBEE_ID) or config.get(CONF_INTERNAL):
        return
    if CORE.using_zephyr:
        from .zigbee_zephyr import zephyr_setup_number

        await zephyr_setup_number(entity, config, min_value, max_value, step)


def default_zigbee_ids(
    config: ConfigType, declare_key: str | None = None, declare_validator=None
) -> ConfigType:
    """Fill CONF_ZIGBEE_ID and (per-platform) declare_key with the auto-resolve/
    auto-generate ID -- same value cv.use_id/cv.declare_id themselves produce for None.
    Caller must already know zigbee_zephyr_supported() is true."""
    config.setdefault(CONF_ZIGBEE_ID, cv.use_id(ZigbeeComponent)(None))
    if declare_key is not None:
        config.setdefault(declare_key, declare_validator(None))
    return config


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
    if "zigbee" not in CORE.loaded_integrations or config.get(CONF_INTERNAL):
        return config
    if CORE.is_esp32:
        return validate_binary_sensor_esp32(config)
    if zigbee_zephyr_supported():
        config = default_zigbee_ids(
            config, CONF_ZIGBEE_BINARY_SENSOR, cv.declare_id(ZigbeeBinarySensor)
        )
    return consume_endpoint(config)


def validate_sensor(config: ConfigType) -> ConfigType:
    if "zigbee" not in CORE.loaded_integrations or config.get(CONF_INTERNAL):
        return config
    if CORE.is_esp32:
        return validate_sensor_esp32(config)
    if zigbee_zephyr_supported():
        config = default_zigbee_ids(
            config, CONF_ZIGBEE_SENSOR, cv.declare_id(ZigbeeSensor)
        )
    return consume_endpoint(config)


def validate_switch(config: ConfigType) -> ConfigType:
    if "zigbee" not in CORE.loaded_integrations or config.get(CONF_INTERNAL):
        return config
    if CORE.is_esp32:
        return config
    if zigbee_zephyr_supported():
        config = default_zigbee_ids(
            config, CONF_ZIGBEE_SWITCH, cv.declare_id(ZigbeeSwitch)
        )
    return consume_endpoint(config)


def validate_number(config: ConfigType) -> ConfigType:
    if "zigbee" not in CORE.loaded_integrations or config.get(CONF_INTERNAL):
        return config
    if CORE.is_esp32:
        return config
    if zigbee_zephyr_supported():
        config = default_zigbee_ids(
            config, CONF_ZIGBEE_NUMBER, cv.declare_id(ZigbeeNumber)
        )
    return consume_endpoint(config)


ZIGBEE_ACTION_SCHEMA = automation.maybe_simple_id(
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(ZigbeeComponent),
        }
    )
)

FactoryResetAction = zigbee_ns.class_(
    "FactoryResetAction", automation.Action, cg.Parented.template(ZigbeeComponent)
)


@automation.register_action(
    "zigbee.factory_reset",
    FactoryResetAction,
    ZIGBEE_ACTION_SCHEMA,
    synchronous=True,
)
async def reset_zigbee_to_code(
    config: ConfigType,
    action_id: core.ID,
    template_arg: cg.TemplateArguments,
    args: list[tuple],
) -> cg.Pvariable:
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
