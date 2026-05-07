import logging
from typing import Any

from esphome import automation, core
import esphome.codegen as cg
from esphome.components.esp32 import only_on_variant
from esphome.components.esp32.const import (
    VARIANT_ESP32C5,
    VARIANT_ESP32C6,
    VARIANT_ESP32H2,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INTERNAL, CONF_MODEL, CONF_NAME
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.types import ConfigType

from .const import (
    CONF_ON_JOIN,
    CONF_POWER_SOURCE,
    CONF_REPORT,
    CONF_ROUTER,
    CONF_WIPE_ON_BOOT,
    KEY_ZIGBEE,
    POWER_SOURCE,
    REPORT,
    ZigbeeComponent,
    zigbee_ns,
)
from .const_zephyr import (
    CONF_IEEE802154_VENDOR_OUI,
    CONF_MAX_EP_NUMBER,
    CONF_SLEEPY,
    CONF_ZIGBEE_ID,
    KEY_EP_NUMBER,
)
from .zigbee_esp32 import (
    final_validate_esp32,
    validate_binary_sensor_esp32,
    zigbee_require_vfs_select,
)
from .zigbee_zephyr import (
    zephyr_binary_sensor,
    zephyr_number,
    zephyr_sensor,
    zephyr_switch,
)

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@luar123", "@tomaszduda23"]


BINARY_SENSOR_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_REPORT): cv.All(
            cv.requires_component("zigbee"),
            cv.requires_component("esp32"),
            cv.enum(REPORT, lower=True),
        )
    }
).extend(zephyr_binary_sensor)
SENSOR_SCHEMA = cv.Schema({}).extend(zephyr_sensor)
SWITCH_SCHEMA = cv.Schema({}).extend(zephyr_switch)
NUMBER_SCHEMA = cv.Schema({}).extend(zephyr_number)


def _validate_router_sleepy(config: ConfigType) -> ConfigType:
    if config.get(CONF_ROUTER) and config.get(CONF_SLEEPY):
        raise cv.Invalid("router and sleepy are mutually exclusive")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(ZigbeeComponent),
            cv.Optional(CONF_MODEL, default=CORE.name): cv.All(
                cv.string, cv.Length(max=31)
            ),
            cv.Optional(CONF_ROUTER, default=False): cv.boolean,
            cv.Optional(CONF_ON_JOIN): cv.All(
                cv.requires_component("nrf52"),
                automation.validate_automation(single=True),
            ),
            cv.OnlyWith(CONF_WIPE_ON_BOOT, "nrf52", default=False): cv.All(
                cv.Any(
                    cv.boolean,
                    cv.one_of(*["once"], lower=True),
                ),
                cv.requires_component("nrf52"),
            ),
            cv.OnlyWith(CONF_POWER_SOURCE, "nrf52", default="DC_SOURCE"): cv.All(
                cv.enum(POWER_SOURCE, upper=True),
                cv.requires_component("nrf52"),
            ),
            cv.Optional(CONF_IEEE802154_VENDOR_OUI): cv.All(
                cv.Any(
                    cv.int_range(min=0x000000, max=0xFFFFFF),
                    cv.one_of(*["random"], lower=True),
                ),
                cv.requires_component("nrf52"),
            ),
            cv.OnlyWith(CONF_SLEEPY, "nrf52", default=False): cv.All(
                cv.boolean,
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
                    VARIANT_ESP32H2,
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
    if KEY_ZIGBEE not in CORE.data:
        raise cv.Invalid("At least one zigbee device need to be included")
    count = len(CORE.data[KEY_ZIGBEE][KEY_EP_NUMBER])
    if count == 1:
        _LOGGER.warning(
            "Single endpoint requires ZHA or at leatst Zigbee2MQTT 2.8.0. For older versions of Zigbee2MQTT use multiple endpoints"
        )
    if count > CONF_MAX_EP_NUMBER and not CORE.testing_mode:
        raise cv.Invalid(f"Maximum number of end points is {CONF_MAX_EP_NUMBER}")

    return config


FINAL_VALIDATE_SCHEMA = cv.All(
    validate_number_of_ep,
    final_validate_esp32,
)


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_ZIGBEE")
    if CORE.using_zephyr:
        from .zigbee_zephyr import zephyr_to_code

        await zephyr_to_code(config)
    if CORE.is_esp32:
        from .zigbee_esp32 import esp32_to_code

        await esp32_to_code(config)


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
    return consume_endpoint(config)


def validate_sensor(config: ConfigType) -> ConfigType:
    if "zigbee" not in CORE.loaded_integrations or config.get(CONF_INTERNAL):
        return config
    if CORE.is_esp32:
        return config
    return consume_endpoint(config)


def validate_switch(config: ConfigType) -> ConfigType:
    if "zigbee" not in CORE.loaded_integrations or config.get(CONF_INTERNAL):
        return config
    if CORE.is_esp32:
        return config
    return consume_endpoint(config)


def validate_number(config: ConfigType) -> ConfigType:
    if "zigbee" not in CORE.loaded_integrations or config.get(CONF_INTERNAL):
        return config
    if CORE.is_esp32:
        return config
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
