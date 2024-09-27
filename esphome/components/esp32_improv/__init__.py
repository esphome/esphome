from esphome import automation
import esphome.codegen as cg
from esphome.components import binary_sensor, esp32_ble_server, output
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_ON_STATE, CONF_TRIGGER_ID

AUTO_LOAD = ["esp32_ble_server"]
CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["wifi", "esp32"]

CONF_AUTHORIZED_DURATION = "authorized_duration"
CONF_AUTHORIZER = "authorizer"
CONF_BLE_SERVER_ID = "ble_server_id"
CONF_IDENTIFY_DURATION = "identify_duration"
CONF_ON_PROVISIONED = "on_provisioned"
CONF_ON_PROVISIONING = "on_provisioning"
CONF_ON_START = "on_start"
CONF_ON_STOP = "on_stop"
CONF_STATUS_INDICATOR = "status_indicator"
CONF_WIFI_TIMEOUT = "wifi_timeout"

improv_ns = cg.esphome_ns.namespace("improv")
Error = improv_ns.enum("Error")
State = improv_ns.enum("State")

esp32_improv_ns = cg.esphome_ns.namespace("esp32_improv")
ESP32ImprovComponent = esp32_improv_ns.class_(
    "ESP32ImprovComponent", cg.Component, esp32_ble_server.BLEServiceComponent
)
ESP32ImprovProvisionedTrigger = esp32_improv_ns.class_(
    "ESP32ImprovProvisionedTrigger", automation.Trigger.template()
)
ESP32ImprovProvisioningTrigger = esp32_improv_ns.class_(
    "ESP32ImprovProvisioningTrigger", automation.Trigger.template()
)
ESP32ImprovStartTrigger = esp32_improv_ns.class_(
    "ESP32ImprovStartTrigger", automation.Trigger.template()
)
ESP32ImprovStateTrigger = esp32_improv_ns.class_(
    "ESP32ImprovStateTrigger", automation.Trigger.template()
)
ESP32ImprovStoppedTrigger = esp32_improv_ns.class_(
    "ESP32ImprovStoppedTrigger", automation.Trigger.template()
)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ESP32ImprovComponent),
        cv.GenerateID(CONF_BLE_SERVER_ID): cv.use_id(esp32_ble_server.BLEServer),
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
            CONF_WIFI_TIMEOUT, default="1min"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_ON_PROVISIONED): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    ESP32ImprovProvisionedTrigger
                ),
            }
        ),
        cv.Optional(CONF_ON_PROVISIONING): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    ESP32ImprovProvisioningTrigger
                ),
            }
        ),
        cv.Optional(CONF_ON_START): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ESP32ImprovStartTrigger),
            }
        ),
        cv.Optional(CONF_ON_STATE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ESP32ImprovStateTrigger),
            }
        ),
        cv.Optional(CONF_ON_STOP): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    ESP32ImprovStoppedTrigger
                ),
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    ble_server = await cg.get_variable(config[CONF_BLE_SERVER_ID])
    cg.add(ble_server.register_service_component(var))

    cg.add_define("USE_IMPROV")
    cg.add_library("improv/Improv", "1.2.4")

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
        cg.add_define("USE_ESP32_IMPROV_STATE_CALLBACK")
