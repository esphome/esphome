from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c, text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_IRQ_PIN,
    CONF_ON_CONNECT,
    CONF_ON_DISCONNECT,
    CONF_ON_ERROR,
    CONF_TRIGGER_ID,
)
from esphome.pins import internal_gpio_input_pin_number

CODEOWNERS = ["@remcom"]
DEPENDENCIES = ["i2c"]

pd_ns = cg.esphome_ns.namespace("fusb302b")
PowerDelivery = pd_ns.class_("PowerDelivery", cg.Component)
FUSB302B = pd_ns.class_("FUSB302B", PowerDelivery, cg.Component, i2c.I2CDevice)

RequestVoltageAction = pd_ns.class_(
    "PowerDeliveryRequestVoltage",
    automation.Action,
    cg.Parented.template(PowerDelivery),
)

ConnectedTrigger = pd_ns.class_("ConnectedTrigger", automation.Trigger.template())
DisconnectedTrigger = pd_ns.class_("DisconnectedTrigger", automation.Trigger.template())
ErrorTrigger = pd_ns.class_("ErrorTrigger", automation.Trigger.template())
PowerReadyTrigger = pd_ns.class_("PowerReadyTrigger", automation.Trigger.template())

IsConnectedCondition = pd_ns.class_("IsConnectedCondition", automation.Condition)

CONF_REQUEST_VOLTAGE = "request_voltage"
CONF_ON_POWER_READY = "on_power_ready"
CONF_CONTRACT_SENSOR = "contract_sensor"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(FUSB302B),
            cv.Required(CONF_IRQ_PIN): internal_gpio_input_pin_number,
            cv.Required(CONF_REQUEST_VOLTAGE): cv.int_range(min=5, max=20),
            cv.Optional(CONF_CONTRACT_SENSOR): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_ON_CONNECT): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ConnectedTrigger)}
            ),
            cv.Optional(CONF_ON_DISCONNECT): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DisconnectedTrigger)}
            ),
            cv.Optional(CONF_ON_ERROR): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ErrorTrigger)}
            ),
            cv.Optional(CONF_ON_POWER_READY): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(PowerReadyTrigger)}
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x22)),
    cv.only_on_esp32,
)

PD_ACTION_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(): cv.use_id(PowerDelivery)}
)


@automation.register_action(
    "power_delivery.request_voltage",
    RequestVoltageAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(PowerDelivery),
            cv.Required(CONF_REQUEST_VOLTAGE): cv.templatable(
                cv.int_range(min=5, max=20)
            ),
        },
        key=CONF_REQUEST_VOLTAGE,
    ),
    synchronous=True,
)
async def power_delivery_request_voltage_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    voltage = await cg.templatable(config[CONF_REQUEST_VOLTAGE], args, int)
    cg.add(var.set_voltage(voltage))
    return var


@automation.register_condition(
    "power_delivery.is_connected", IsConnectedCondition, PD_ACTION_SCHEMA
)
async def power_delivery_is_connected_condition(
    config, condition_id, template_arg, args
):
    var = cg.new_Pvariable(condition_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_request_voltage(config[CONF_REQUEST_VOLTAGE]))
    cg.add(var.set_irq_pin(config[CONF_IRQ_PIN]))
    if contract_sensor_config := config.get(CONF_CONTRACT_SENSOR):
        sens = await text_sensor.new_text_sensor(contract_sensor_config)
        cg.add(var.set_contract_sensor(sens))
    for conf in config.get(CONF_ON_CONNECT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_DISCONNECT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_ERROR, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_POWER_READY, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
