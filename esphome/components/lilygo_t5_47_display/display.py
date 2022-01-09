import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display
from esphome.const import (
    CONF_FULL_UPDATE_EVERY,
    CONF_ID,
    CONF_LAMBDA,
    CONF_PAGES,
)

CONF_CLEAR = "clear"
CONF_TEMPERATURE = "temperature"
CONF_LANDSCAPE = "landscape"
CONF_POWER_OFF_DELAY_ENABLED = "power_off_delay_enabled"

Epaper_ns = cg.esphome_ns.namespace("lilygo_t5_47_display")
Epaper = Epaper_ns.class_(
    "LilygoT547Display", cg.PollingComponent, display.DisplayBuffer
)

CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(Epaper),
            cv.Optional(CONF_FULL_UPDATE_EVERY, default=10): cv.uint32_t,
            cv.Optional(CONF_CLEAR, default=True): cv.boolean,
            cv.Optional(CONF_POWER_OFF_DELAY_ENABLED, default=False): cv.boolean,
            cv.Optional(CONF_LANDSCAPE, default=True): cv.boolean,
            cv.Optional(CONF_TEMPERATURE, default=23): cv.uint32_t,
        }
    ).extend(cv.polling_component_schema("5s")),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await display.register_display(var, config)

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayBufferRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
    cg.add(var.set_clear_screen(config[CONF_CLEAR]))
    cg.add(var.set_temperature(config[CONF_TEMPERATURE]))
    cg.add(var.set_landscape(config[CONF_LANDSCAPE]))
    cg.add(var.set_power_off_delay_enabled(config[CONF_POWER_OFF_DELAY_ENABLED]))
    cg.add_library("https://github.com/vroland/epdiy.git", None)
    cg.add_build_flag("-DBOARD_HAS_PSRAM")
    cg.add_build_flag("-DCONFIG_EPD_DISPLAY_TYPE_ED047TC1")
    cg.add_build_flag("-DCONFIG_EPD_BOARD_REVISION_LILYGO_T5_47")
