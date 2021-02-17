import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import spi, sensor
from esphome.const import CONF_ID, UNIT_EMPTY, ICON_EMPTY, DEVICE_CLASS_EMPTY

DEPENDENCIES = ["spi"]

CONF_TOUCHED = "touched"
CONF_COORD_X = "coord_x"
CONF_COORD_Y = "coord_y"
CONF_TIRQ_PIN = "tirq_pin"
CONF_REPORT_INTERVAL = "report_interval"
CONF_TRANSFORM = "transform"
CONF_CAL_X_MIN = "cal_x_min"
CONF_CAL_X_MAX = "cal_x_max"
CONF_CAL_Y_MIN = "cal_y_min"
CONF_CAL_Y_MAX = "cal_y_max"
CONF_DIM_X = "dim_x"
CONF_DIM_Y = "dim_y"

xpt2046_ns = cg.esphome_ns.namespace("xpt2046")

XPT2046Component = xpt2046_ns.class_(
    "XPT2046Component", cg.PollingComponent, spi.SPIDevice
)

XPT2046Transform = xpt2046_ns.enum("XPT2046Transform")

TRANSFORM = {
    "SWAP_X_Y": XPT2046Transform.SWAP_X_Y,
    "INVERT_X": XPT2046Transform.INVERT_X,
    "INVERT_Y": XPT2046Transform.INVERT_Y,
}


def validate_xpt2046(config):
    if (
        cv.int_(config[CONF_CAL_X_MIN]) < 0
        or cv.int_(config[CONF_CAL_X_MIN]) > 4095
        or cv.int_(config[CONF_CAL_X_MAX]) < 0
        or cv.int_(config[CONF_CAL_X_MAX]) > 4095
        or cv.int_(config[CONF_CAL_X_MAX]) - cv.int_(config[CONF_CAL_X_MIN]) < 1000
    ):
        raise cv.Invalid(
            "Calibration X values not in the 0-4095 range or difference < 1000"
        )

    if (
        cv.int_(config[CONF_CAL_Y_MIN]) < 0
        or cv.int_(config[CONF_CAL_Y_MIN]) > 4095
        or cv.int_(config[CONF_CAL_Y_MAX]) < 0
        or cv.int_(config[CONF_CAL_Y_MAX]) > 4095
        or cv.int_(config[CONF_CAL_Y_MAX]) - cv.int_(config[CONF_CAL_Y_MIN]) < 1000
    ):
        raise cv.Invalid(
            "Calibration Y values not in the 0-4095 range or difference < 1000"
        )

    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(XPT2046Component),
            cv.Optional(CONF_TIRQ_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_TOUCHED): sensor.sensor_schema(
                UNIT_EMPTY, ICON_EMPTY, 1, DEVICE_CLASS_EMPTY
            ),
            cv.Optional(CONF_COORD_X): sensor.sensor_schema(
                UNIT_EMPTY, ICON_EMPTY, 1, DEVICE_CLASS_EMPTY
            ),
            cv.Optional(CONF_COORD_Y): sensor.sensor_schema(
                UNIT_EMPTY, ICON_EMPTY, 1, DEVICE_CLASS_EMPTY
            ),
            cv.Optional(CONF_CAL_X_MIN, default=0): cv.positive_int,
            cv.Optional(CONF_CAL_X_MAX, default=4095): cv.positive_not_null_int,
            cv.Optional(CONF_CAL_Y_MIN, default=0): cv.positive_int,
            cv.Optional(CONF_CAL_Y_MAX, default=4095): cv.positive_not_null_int,
            cv.Optional(CONF_DIM_X, default=100): cv.positive_not_null_int,
            cv.Optional(CONF_DIM_Y, default=100): cv.positive_not_null_int,
            cv.Optional(
                CONF_REPORT_INTERVAL, default="1s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_TRANSFORM): cv.All(
                cv.ensure_list(cv.enum(TRANSFORM, upper=True)), cv.Length(max=3)
            ),
        }
    )
    .extend(cv.polling_component_schema("50ms"))
    .extend(spi.spi_device_schema()),
    validate_xpt2046,
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield spi.register_spi_device(var, config)

    if CONF_TOUCHED in config:
        conf = config[CONF_TOUCHED]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_touched_sensor(sens))

    if CONF_COORD_X in config:
        conf = config[CONF_COORD_X]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_x_sensor(sens))

    if CONF_COORD_Y in config:
        conf = config[CONF_COORD_Y]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_y_sensor(sens))

    if CONF_TIRQ_PIN in config:
        tirq = yield cg.gpio_pin_expression(config[CONF_TIRQ_PIN])
        cg.add(var.set_tirq_pin(tirq))

    cg.add(var.set_report_interval(config[CONF_REPORT_INTERVAL]))
    cg.add(var.set_dimensions(config[CONF_DIM_X], config[CONF_DIM_Y]))
    cg.add(
        var.set_calibration(
            config[CONF_CAL_X_MIN],
            config[CONF_CAL_X_MAX],
            config[CONF_CAL_Y_MIN],
            config[CONF_CAL_Y_MAX],
        )
    )

    if CONF_TRANSFORM in config:
        for trans in config[CONF_TRANSFORM]:
            cg.add(var.set_transform(trans))
