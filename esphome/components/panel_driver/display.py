import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display
from esphome.const import CONF_ID, CONF_LAMBDA

from . import panel_driver_ns
from . import PanelDriver

PanelDriverBuffer = panel_driver_ns.class_(
    "PanelDriverBuffer", cg.PollingComponent, display.DisplayBuffer
)

CONF_PANEL_DRIVER_ID = "panel_driver_id"

CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(PanelDriverBuffer),
            cv.GenerateID(CONF_PANEL_DRIVER_ID): cv.use_id(PanelDriver),
        }
    )
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, config[CONF_PANEL_DRIVER_ID])
    await display.register_display(var, config)
    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
