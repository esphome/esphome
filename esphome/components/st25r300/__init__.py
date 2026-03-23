from esphome import automation, pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor as binary_sensor_
from esphome.components import sensor as sensor_
from esphome.components import st25r as st25r_
from esphome.const import (
    CONF_ID,
    CONF_ON_TAG,
    CONF_ON_TAG_REMOVED,
    CONF_TRIGGER_ID,
    CONF_IRQ_PIN,
    CONF_RESET_PIN,
    CONF_STATUS,
)

CODEOWNERS = ["@JohnMcLear"]
AUTO_LOAD = ["st25r", "binary_sensor", "sensor", "nfc"]
MULTI_CONF = True

CONF_RF_FIELD_ENABLED = "rf_field_enabled"
CONF_RF_POWER = "rf_power"
CONF_RX_GAIN_BOOST = "rx_gain_boost"
CONF_FIELD_STRENGTH = "field_strength"
CONF_HEALTH_CHECK_ENABLED = "health_check_enabled"
CONF_HEALTH_CHECK_INTERVAL = "health_check_interval"
CONF_MAX_FAILED_CHECKS = "max_failed_checks"
CONF_AUTO_RESET_ON_FAILURE = "auto_reset_on_failure"
CONF_NFCV_ENABLED = "nfcv_enabled"
CONF_NFCB_ENABLED = "nfcb_enabled"


st25r300_ns = cg.esphome_ns.namespace("st25r300")
ST25R300 = st25r300_ns.class_("ST25R300", st25r_.ST25R)

ST25R300TagTrigger = st25r300_ns.class_(
    "ST25R300TagTrigger", automation.Trigger.template(cg.std_string)
)
ST25R300TagRemovedTrigger = st25r300_ns.class_(
    "ST25R300TagRemovedTrigger", automation.Trigger.template(cg.std_string)
)

NDEFWriteAction = st25r300_ns.class_("NDEFWriteAction", automation.Action)
CleanTagAction = st25r300_ns.class_("CleanTagAction", automation.Action)

ST25R300_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ST25R300),
        cv.Optional(CONF_IRQ_PIN): pins.internal_gpio_input_pin_schema,
        cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_RF_FIELD_ENABLED, default=True): cv.boolean,
        cv.Optional(CONF_RF_POWER, default=15): cv.int_range(min=0, max=15),
        cv.Optional(CONF_RX_GAIN_BOOST, default=False): cv.boolean,
        cv.Optional(CONF_HEALTH_CHECK_ENABLED, default=True): cv.boolean,
        cv.Optional(CONF_HEALTH_CHECK_INTERVAL, default="60s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_MAX_FAILED_CHECKS, default=3): cv.int_range(min=1, max=255),
        cv.Optional(CONF_AUTO_RESET_ON_FAILURE, default=True): cv.boolean,
        cv.Optional(CONF_NFCV_ENABLED, default=True): cv.boolean,
        cv.Optional(CONF_NFCB_ENABLED, default=True): cv.boolean,
        cv.Optional(CONF_STATUS): binary_sensor_.binary_sensor_schema(),
        cv.Optional(CONF_FIELD_STRENGTH): sensor_.sensor_schema(),
        cv.Optional(CONF_ON_TAG): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ST25R300TagTrigger),
            }
        ),
        cv.Optional(CONF_ON_TAG_REMOVED): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ST25R300TagRemovedTrigger),
            }
        ),
    }
).extend(cv.polling_component_schema("1s"))


async def setup_st25r300(var, config):
    await cg.register_component(var, config)

    if CONF_IRQ_PIN in config:
        irq = await cg.gpio_pin_expression(config[CONF_IRQ_PIN])
        cg.add(var.set_irq_pin(irq))

    if CONF_RESET_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))

    cg.add(var.set_rf_field_enabled(config[CONF_RF_FIELD_ENABLED]))
    cg.add(var.set_rf_power(config[CONF_RF_POWER]))
    cg.add(var.set_rx_gain_boost(config[CONF_RX_GAIN_BOOST]))
    cg.add(var.set_health_check_enabled(config[CONF_HEALTH_CHECK_ENABLED]))
    cg.add(var.set_health_check_interval(config[CONF_HEALTH_CHECK_INTERVAL]))
    cg.add(var.set_max_failed_checks(config[CONF_MAX_FAILED_CHECKS]))
    cg.add(var.set_auto_reset_on_failure(config[CONF_AUTO_RESET_ON_FAILURE]))
    cg.add(var.set_nfcv_enabled(config[CONF_NFCV_ENABLED]))
    cg.add(var.set_nfcb_enabled(config[CONF_NFCB_ENABLED]))

    if CONF_STATUS in config:
        sens = await binary_sensor_.new_binary_sensor(config[CONF_STATUS])
        cg.add(var.set_status_binary_sensor(sens))

    if CONF_FIELD_STRENGTH in config:
        sens = await sensor_.new_sensor(config[CONF_FIELD_STRENGTH])
        cg.add(var.set_field_strength_sensor(sens))

    for conf in config.get(CONF_ON_TAG, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        cg.add(var.register_on_tag_trigger(trigger))
        await automation.build_automation(
            trigger, [(cg.std_string, "x")], conf
        )

    for conf in config.get(CONF_ON_TAG_REMOVED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        cg.add(var.register_on_tag_removed_trigger(trigger))
        await automation.build_automation(
            trigger, [(cg.std_string, "x")], conf
        )


@automation.register_action(
    "st25r300.ndef_write",
    NDEFWriteAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(ST25R300),
            cv.Required("ndef_message"): cv.lambda_,
            cv.Optional("format", default=False): cv.boolean,
        },
        key="ndef_message",
    ),
)
async def st25r300_ndef_write_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    parent = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_parent(parent))
    template_ = await cg.process_lambda(
        config["ndef_message"], args, return_type=cg.RawExpression("esphome::nfc::NdefMessage *")
    )
    cg.add(var.set_message(template_))
    cg.add(var.set_format(config["format"]))
    return var


@automation.register_action(
    "st25r300.clean_tag",
    CleanTagAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(ST25R300),
        },
    ),
)
async def st25r300_clean_tag_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    parent = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_parent(parent))
    return var
