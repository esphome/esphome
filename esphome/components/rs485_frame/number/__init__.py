import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_LAMBDA, CONF_MAX_VALUE, CONF_MIN_VALUE, CONF_STEP

from .. import CONF_RS485_FRAME_ID, RS485FrameHub, rs485_frame_ns

AUTO_LOAD = ["rs485_frame"]

RS485FrameNumber = rs485_frame_ns.class_("RS485FrameNumber", number.Number)

CONFIG_SCHEMA = number.number_schema(RS485FrameNumber).extend(
    {
        cv.GenerateID(CONF_RS485_FRAME_ID): cv.use_id(RS485FrameHub),
        cv.Required(CONF_MIN_VALUE): cv.float_,
        cv.Required(CONF_MAX_VALUE): cv.float_,
        cv.Required(CONF_STEP): cv.positive_float,
        cv.Required(CONF_LAMBDA): cv.returning_lambda,
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_RS485_FRAME_ID])
    var = await number.new_number(
        config,
        hub,
        min_value=config[CONF_MIN_VALUE],
        max_value=config[CONF_MAX_VALUE],
        step=config[CONF_STEP],
    )

    template_ = await cg.process_lambda(
        config[CONF_LAMBDA],
        [(cg.float_, "x")],
        return_type=cg.optional.template(cg.std_vector.template(cg.uint8)),
    )
    cg.add(var.set_template(template_))
