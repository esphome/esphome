import esphome.codegen as cg
from esphome.components import display, esp32
import esphome.config_validation as cv
from esphome.const import (
    CONF_FULL_UPDATE_EVERY,
    CONF_ID,
    CONF_LAMBDA,
    CONF_MODEL,
    CONF_PAGES,
)
from esphome.cpp_generator import MockObj

CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["esp32", "psram"]

CONF_POWER_OFF_DELAY_ENABLED = "power_off_delay_enabled"

epdiy_ns = cg.esphome_ns.namespace("epdiy")
EPDiyDisplay = epdiy_ns.class_("EPDiyDisplay", display.Display)


class EpdBoardDefinition(MockObj):
    def __str__(self):
        return f"&{self.base}"


class EpdDisplay_t(MockObj):
    def __str__(self):
        return f"&{self.base}"


EpdInitOptions = cg.global_ns.enum("EpdInitOptions")


class Model:
    def __init__(
        self,
        *,
        board_definition: MockObj,
        display_t: MockObj,
        init_options: MockObj,
        width: int,
        height: int,
        vcom_mv: int = 0,
    ):
        self.board_definition = board_definition
        self.display_t = display_t
        self.init_options = init_options
        self.width = width
        self.height = height
        self.vcom_mv = vcom_mv


MODELS: dict[str, Model] = {
    "lilygo_t5_4.7": Model(
        board_definition=EpdBoardDefinition("epd_board_lilygo_t5_47"),
        display_t=EpdDisplay_t("ED047TC2"),
        init_options=(EpdInitOptions.EPD_LUT_64K, EpdInitOptions.EPD_FEED_QUEUE_8),
        width=960,
        height=540,
    ),
}

CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(EPDiyDisplay),
            cv.Required(CONF_MODEL): cv.one_of(*MODELS.keys()),
            cv.Optional(CONF_FULL_UPDATE_EVERY, default=10): cv.uint32_t,
            cv.Optional(CONF_POWER_OFF_DELAY_ENABLED, default=False): cv.boolean,
        }
    ).extend(cv.polling_component_schema("60s")),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
    cv.only_with_esp_idf,  # When trying to add library via platformio it breaks, using as an idf component works fine
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await display.register_display(var, config)

    model = MODELS[config[CONF_MODEL]]
    cg.add(
        var.set_model_details(
            model.board_definition,
            model.display_t,
            cg.RawExpression(
                f"static_cast<EpdInitOptions>({'|'.join(str(o) for o in model.init_options)})"
            ),
            model.vcom_mv,
        )
    )

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))

    cg.add(var.set_power_off_delay_enabled(config[CONF_POWER_OFF_DELAY_ENABLED]))

    esp32.add_idf_component(
        name="vroland/epdiy",
        repo="https://github.com/vroland/epdiy",
        ref="c61e9e923ce2418150d54f88cea5d196cdc40c54",
    )
