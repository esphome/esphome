from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import i2c, sensor, text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CURRENT,
    CONF_ID,
    CONF_INTERRUPT_PIN,
    CONF_ON_CONNECT,
    CONF_ON_DISCONNECT,
    CONF_ON_ERROR,
    CONF_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_FLASH,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_VOLT,
)
from esphome.types import ConfigType

CODEOWNERS = ["@remcom"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor", "text_sensor"]

CONF_CONTRACT = "contract"
CONF_ON_POWER_READY = "on_power_ready"
CONF_REQUEST_VOLTAGE = "request_voltage"

fusb302b_ns = cg.esphome_ns.namespace("fusb302b")
FUSB302B = fusb302b_ns.class_("FUSB302B", cg.Component, i2c.I2CDevice)

ConnectForwarder = fusb302b_ns.class_("ConnectForwarder")
DisconnectForwarder = fusb302b_ns.class_("DisconnectForwarder")
PowerReadyForwarder = fusb302b_ns.class_("PowerReadyForwarder")
ErrorForwarder = fusb302b_ns.class_("ErrorForwarder")

# USB PD fixed supply voltages are 5, 9, 12, 15 and 20 V; the highest one that does not exceed the
# requested voltage is chosen.
validate_voltage = cv.int_range(min=5, max=20)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(FUSB302B),
            cv.Required(CONF_INTERRUPT_PIN): pins.internal_gpio_input_pullup_pin_schema,
            cv.Required(CONF_REQUEST_VOLTAGE): validate_voltage,
            cv.Optional(CONF_CONTRACT): text_sensor.text_sensor_schema(
                icon=ICON_FLASH,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_CURRENT): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_ON_CONNECT): automation.validate_automation({}),
            cv.Optional(CONF_ON_DISCONNECT): automation.validate_automation({}),
            cv.Optional(CONF_ON_POWER_READY): automation.validate_automation({}),
            cv.Optional(CONF_ON_ERROR): automation.validate_automation({}),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x22)),
    cv.only_on_esp32,
)

_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_CONNECT, "add_on_state_callback", forwarder=ConnectForwarder
    ),
    automation.CallbackAutomation(
        CONF_ON_DISCONNECT, "add_on_state_callback", forwarder=DisconnectForwarder
    ),
    automation.CallbackAutomation(
        CONF_ON_POWER_READY, "add_on_state_callback", forwarder=PowerReadyForwarder
    ),
    automation.CallbackAutomation(
        CONF_ON_ERROR, "add_on_state_callback", forwarder=ErrorForwarder
    ),
)


async def to_code(config: ConfigType) -> None:
    interrupt_pin = await cg.gpio_pin_expression(config[CONF_INTERRUPT_PIN])
    var = cg.new_Pvariable(config[CONF_ID], interrupt_pin)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_request_voltage(config[CONF_REQUEST_VOLTAGE]))

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_VOLTAGE, var.set_voltage_sensor)
    await sensors(CONF_CURRENT, var.set_current_sensor)
    text_sensors = text_sensor.sub_text_sensors(config)
    await text_sensors(CONF_CONTRACT, var.set_contract_text_sensor)

    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


automation.register_apply_action(
    "fusb302b.request_voltage",
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(FUSB302B),
            cv.Required(CONF_VOLTAGE): cv.templatable(validate_voltage),
        },
        key=CONF_VOLTAGE,
    ),
    automation.ApplyField(CONF_VOLTAGE, "request_voltage", cg.uint8),
)

automation.register_apply_condition(
    "fusb302b.is_connected",
    automation.maybe_simple_id({cv.GenerateID(): cv.use_id(FUSB302B)}),
    "is_connected()",
)
