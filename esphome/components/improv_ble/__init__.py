from esphome import automation
import esphome.codegen as cg
from esphome.components import binary_sensor, improv_base, output
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_ON_START,
    CONF_ON_STATE,
    CONF_TRIGGER_ID,
    PLATFORM_ESP32,
)
from esphome.core import CORE
from esphome.types import ConfigType

# The BLE GATT server component that hosts the Improv service, per target
# platform. improv_ble itself is platform neutral; supporting another chip
# means adding its BLE server component here and the matching backend in
# improv_ble_component.cpp. Doubles as the platform gate below, so an
# unsupported chip is rejected in validation rather than at link time.
BLE_SERVER_BACKENDS: dict[str, str] = {
    PLATFORM_ESP32: "esp32_ble_server",
}


def AUTO_LOAD() -> list[str]:
    auto_load = ["improv_base"]
    if backend := BLE_SERVER_BACKENDS.get(CORE.target_platform):
        auto_load.append(backend)
    return auto_load


CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["wifi"]

# Legacy top-level YAML key that routes here; esphome/loader.py and
# esphome/config.py handle the warning and the key rename.
ALIASES = ["esp32_improv"]
ALIAS_REMOVAL_VERSION = "2027.4.0"

CONF_AUTHORIZED_DURATION = "authorized_duration"
CONF_AUTHORIZER = "authorizer"
CONF_BLE_SERVER_ID = "ble_server_id"
CONF_IDENTIFY_DURATION = "identify_duration"
CONF_ON_PROVISIONED = "on_provisioned"
CONF_ON_PROVISIONING = "on_provisioning"
CONF_ON_STOP = "on_stop"
CONF_STATUS_INDICATOR = "status_indicator"
CONF_WIFI_TIMEOUT = "wifi_timeout"

# Default WiFi timeout - aligned with WiFi component ap_timeout
# Allows sufficient time to try all BSSIDs before starting provisioning mode
DEFAULT_WIFI_TIMEOUT = "90s"


improv_ns = cg.esphome_ns.namespace("improv")
Error = improv_ns.enum("Error")
State = improv_ns.enum("State")

improv_ble_ns = cg.esphome_ns.namespace("improv_ble")
ImprovBLEComponent = improv_ble_ns.class_("ImprovBLEComponent", cg.Component)
ImprovBLEProvisionedTrigger = improv_ble_ns.class_(
    "ImprovBLEProvisionedTrigger", automation.Trigger.template()
)
ImprovBLEProvisioningTrigger = improv_ble_ns.class_(
    "ImprovBLEProvisioningTrigger", automation.Trigger.template()
)
ImprovBLEStartTrigger = improv_ble_ns.class_(
    "ImprovBLEStartTrigger", automation.Trigger.template()
)
ImprovBLEStateTrigger = improv_ble_ns.class_(
    "ImprovBLEStateTrigger", automation.Trigger.template()
)
ImprovBLEStoppedTrigger = improv_ble_ns.class_(
    "ImprovBLEStoppedTrigger", automation.Trigger.template()
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ImprovBLEComponent),
            cv.Required(CONF_AUTHORIZER): cv.Any(
                cv.none, cv.use_id(binary_sensor.BinarySensor)
            ),
            cv.Optional(CONF_STATUS_INDICATOR): cv.use_id(output.BinaryOutput),
            cv.Optional(
                CONF_IDENTIFY_DURATION, default="10s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_AUTHORIZED_DURATION, default="1min"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_WIFI_TIMEOUT, default=DEFAULT_WIFI_TIMEOUT
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_ON_PROVISIONED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        ImprovBLEProvisionedTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_PROVISIONING): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        ImprovBLEProvisioningTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_START): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        ImprovBLEStartTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_STATE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        ImprovBLEStateTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_STOP): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        ImprovBLEStoppedTrigger
                    ),
                }
            ),
        }
    )
    .extend(improv_base.IMPROV_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    cv.only_on(list(BLE_SERVER_BACKENDS)),
)


async def to_code(config: ConfigType) -> None:
    # ESP32 backend setup: the platform gate above means this is the only backend
    # that can reach to_code. Make it conditional when a second one is added.
    from esphome.components import esp32_ble

    # Register the loggers this component needs
    esp32_ble.register_bt_logger(esp32_ble.BTLoggers.GATT, esp32_ble.BTLoggers.SMP)

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add_define("USE_IMPROV_BLE")

    await improv_base.setup_improv_core(var, config)

    cg.add(var.set_identify_duration(config[CONF_IDENTIFY_DURATION]))
    cg.add(var.set_authorized_duration(config[CONF_AUTHORIZED_DURATION]))

    cg.add(var.set_wifi_timeout(config[CONF_WIFI_TIMEOUT]))

    if CONF_AUTHORIZER in config and config[CONF_AUTHORIZER] is not None:
        activator = await cg.get_variable(config[CONF_AUTHORIZER])
        cg.add(var.set_authorizer(activator))

    if CONF_STATUS_INDICATOR in config:
        status_indicator = await cg.get_variable(config[CONF_STATUS_INDICATOR])
        cg.add(var.set_status_indicator(status_indicator))

    use_state_callback = False
    for conf in config.get(CONF_ON_PROVISIONED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
        use_state_callback = True
    for conf in config.get(CONF_ON_PROVISIONING, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
        use_state_callback = True
    for conf in config.get(CONF_ON_START, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
        use_state_callback = True
    for conf in config.get(CONF_ON_STATE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(State, "state"), (Error, "error")], conf
        )
        use_state_callback = True
    for conf in config.get(CONF_ON_STOP, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
        use_state_callback = True
    if use_state_callback:
        cg.add_define("USE_IMPROV_BLE_STATE_CALLBACK")
