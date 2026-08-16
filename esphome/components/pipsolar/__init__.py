import esphome.codegen as cg
from esphome.components import time, uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TIME_ID

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@andreashergert1984"]
AUTO_LOAD = ["binary_sensor", "text_sensor", "sensor", "switch", "output"]
MULTI_CONF = True

CONF_PIPSOLAR_ID = "pipsolar_id"
CONF_CLOCK_CORRECTION_THRESHOLD = "clock_correction_threshold"

pipsolar_ns = cg.esphome_ns.namespace("pipsolar")
PipsolarComponent = pipsolar_ns.class_("Pipsolar", cg.Component)

PIPSOLAR_COMPONENT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_PIPSOLAR_ID): cv.use_id(PipsolarComponent),
    }
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PipsolarComponent),
            cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
            cv.Optional(
                CONF_CLOCK_CORRECTION_THRESHOLD, default="60s"
            ): cv.positive_time_period_seconds,
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if (time_id := config.get(CONF_TIME_ID)) is not None:
        cg.add(var.set_time(await cg.get_variable(time_id)))
        cg.add(
            var.set_clock_correction_threshold(config[CONF_CLOCK_CORRECTION_THRESHOLD])
        )
