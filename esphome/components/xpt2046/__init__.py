import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome import pins
from esphome.components import spi
from esphome.const import CONF_ID, CONF_ON_STATE, CONF_THRESHOLD, CONF_TRIGGER_ID

CODEOWNERS = ["@numo68"]
AUTO_LOAD = ["binary_sensor"]
DEPENDENCIES = ["spi"]
MULTI_CONF = True

CONF_REPORT_INTERVAL = "report_interval"
CONF_CALIBRATION_X_MIN = "calibration_x_min"
CONF_CALIBRATION_X_MAX = "calibration_x_max"
CONF_CALIBRATION_Y_MIN = "calibration_y_min"
CONF_CALIBRATION_Y_MAX = "calibration_y_max"
CONF_DIMENSION_X = "dimension_x"
CONF_DIMENSION_Y = "dimension_y"
CONF_SWAP_X_Y = "swap_x_y"
CONF_IRQ_PIN = "irq_pin"

xpt2046_ns = cg.esphome_ns.namespace("xpt2046")
CONF_XPT2046_ID = "xpt2046_id"

XPT2046Component = xpt2046_ns.class_(
    "XPT2046Component", cg.PollingComponent, spi.SPIDevice
)

XPT2046OnStateTrigger = xpt2046_ns.class_(
    "XPT2046OnStateTrigger", automation.Trigger.template(cg.int_, cg.int_, cg.bool_)
)


def validate_xpt2046(config):
    if (
        abs(
            cv.int_(config[CONF_CALIBRATION_X_MAX])
            - cv.int_(config[CONF_CALIBRATION_X_MIN])
        )
        < 1000
    ):
        raise cv.Invalid("Calibration X values difference < 1000")

    if (
        abs(
            cv.int_(config[CONF_CALIBRATION_Y_MAX])
            - cv.int_(config[CONF_CALIBRATION_Y_MIN])
        )
        < 1000
    ):
        raise cv.Invalid("Calibration Y values difference < 1000")

    return config


def report_interval(value):
    if value == "never":
        return 4294967295  # uint32_t max
    return cv.positive_time_period_milliseconds(value)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(XPT2046Component),
            cv.Optional(CONF_IRQ_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_CALIBRATION_X_MIN, default=0): cv.int_range(
                min=0, max=4095
            ),
            cv.Optional(CONF_CALIBRATION_X_MAX, default=4095): cv.int_range(
                min=0, max=4095
            ),
            cv.Optional(CONF_CALIBRATION_Y_MIN, default=0): cv.int_range(
                min=0, max=4095
            ),
            cv.Optional(CONF_CALIBRATION_Y_MAX, default=4095): cv.int_range(
                min=0, max=4095
            ),
            cv.Optional(CONF_DIMENSION_X, default=100): cv.positive_not_null_int,
            cv.Optional(CONF_DIMENSION_Y, default=100): cv.positive_not_null_int,
            cv.Optional(CONF_THRESHOLD, default=400): cv.int_range(min=0, max=4095),
            cv.Optional(CONF_REPORT_INTERVAL, default="never"): report_interval,
            cv.Optional(CONF_SWAP_X_Y, default=False): cv.boolean,
            cv.Optional(CONF_ON_STATE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        XPT2046OnStateTrigger
                    ),
                }
            ),
        }
    )
    .extend(cv.polling_component_schema("50ms"))
    .extend(spi.spi_device_schema()),
    validate_xpt2046,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    cg.add(var.set_threshold(config[CONF_THRESHOLD]))
    cg.add(var.set_report_interval(config[CONF_REPORT_INTERVAL]))
    cg.add(var.set_dimensions(config[CONF_DIMENSION_X], config[CONF_DIMENSION_Y]))
    cg.add(
        var.set_calibration(
            config[CONF_CALIBRATION_X_MIN],
            config[CONF_CALIBRATION_X_MAX],
            config[CONF_CALIBRATION_Y_MIN],
            config[CONF_CALIBRATION_Y_MAX],
        )
    )

    if CONF_SWAP_X_Y in config:
        cg.add(var.set_swap_x_y(config[CONF_SWAP_X_Y]))

    if CONF_IRQ_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_IRQ_PIN])
        cg.add(var.set_irq_pin(pin))

    for conf in config.get(CONF_ON_STATE, []):
        await automation.build_automation(
            var.get_on_state_trigger(),
            [(cg.int_, "x"), (cg.int_, "y"), (cg.bool_, "touched")],
            conf,
        )
