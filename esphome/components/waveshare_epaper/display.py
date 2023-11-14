import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import core, pins
from esphome.components import display, spi
from esphome.const import (
    CONF_BUSY_PIN,
    CONF_DC_PIN,
    CONF_FULL_UPDATE_EVERY,
    CONF_ID,
    CONF_LAMBDA,
    CONF_MODEL,
    CONF_PAGES,
    CONF_RESET_DURATION,
    CONF_RESET_PIN,
)

DEPENDENCIES = ["spi"]

waveshare_epaper_ns = cg.esphome_ns.namespace("waveshare_epaper")
WaveshareEPaper = waveshare_epaper_ns.class_(
    "WaveshareEPaper", cg.PollingComponent, spi.SPIDevice, display.DisplayBuffer
)
WaveshareEPaperTypeA = waveshare_epaper_ns.class_(
    "WaveshareEPaperTypeA", WaveshareEPaper
)
WaveshareEPaper2P7In = waveshare_epaper_ns.class_(
    "WaveshareEPaper2P7In", WaveshareEPaper
)
WaveshareEPaper2P9InB = waveshare_epaper_ns.class_(
    "WaveshareEPaper2P9InB", WaveshareEPaper
)
GDEY029T94 = waveshare_epaper_ns.class_("GDEY029T94", WaveshareEPaper)
WaveshareEPaper4P2In = waveshare_epaper_ns.class_(
    "WaveshareEPaper4P2In", WaveshareEPaper
)
WaveshareEPaper4P2InBV2 = waveshare_epaper_ns.class_(
    "WaveshareEPaper4P2InBV2", WaveshareEPaper
)
WaveshareEPaper5P8In = waveshare_epaper_ns.class_(
    "WaveshareEPaper5P8In", WaveshareEPaper
)
WaveshareEPaper5P8InV2 = waveshare_epaper_ns.class_(
    "WaveshareEPaper5P8InV2", WaveshareEPaper
)
WaveshareEPaper7P5In = waveshare_epaper_ns.class_(
    "WaveshareEPaper7P5In", WaveshareEPaper
)
WaveshareEPaper7P5InBC = waveshare_epaper_ns.class_(
    "WaveshareEPaper7P5InBC", WaveshareEPaper
)
WaveshareEPaper7P5InBV2 = waveshare_epaper_ns.class_(
    "WaveshareEPaper7P5InBV2", WaveshareEPaper
)
WaveshareEPaper7P5InBV3 = waveshare_epaper_ns.class_(
    "WaveshareEPaper7P5InBV3", WaveshareEPaper
)
WaveshareEPaper7P5InV2 = waveshare_epaper_ns.class_(
    "WaveshareEPaper7P5InV2", WaveshareEPaper
)
WaveshareEPaper7P5InV2alt = waveshare_epaper_ns.class_(
    "WaveshareEPaper7P5InV2alt", WaveshareEPaper
)
WaveshareEPaper7P5InHDB = waveshare_epaper_ns.class_(
    "WaveshareEPaper7P5InHDB", WaveshareEPaper
)
WaveshareEPaper2P13InDKE = waveshare_epaper_ns.class_(
    "WaveshareEPaper2P13InDKE", WaveshareEPaper
)
GDEW0154M09 = waveshare_epaper_ns.class_("GDEW0154M09", WaveshareEPaper)

WaveshareEPaperTypeAModel = waveshare_epaper_ns.enum("WaveshareEPaperTypeAModel")
WaveshareEPaperTypeBModel = waveshare_epaper_ns.enum("WaveshareEPaperTypeBModel")

MODELS = {
    "1.54in": ("a", WaveshareEPaperTypeAModel.WAVESHARE_EPAPER_1_54_IN),
    "1.54inv2": ("a", WaveshareEPaperTypeAModel.WAVESHARE_EPAPER_1_54_IN_V2),
    "2.13in": ("a", WaveshareEPaperTypeAModel.WAVESHARE_EPAPER_2_13_IN),
    "2.13in-ttgo": ("a", WaveshareEPaperTypeAModel.TTGO_EPAPER_2_13_IN),
    "2.13in-ttgo-b1": ("a", WaveshareEPaperTypeAModel.TTGO_EPAPER_2_13_IN_B1),
    "2.13in-ttgo-b73": ("a", WaveshareEPaperTypeAModel.TTGO_EPAPER_2_13_IN_B73),
    "2.13in-ttgo-b74": ("a", WaveshareEPaperTypeAModel.TTGO_EPAPER_2_13_IN_B74),
    "2.90in": ("a", WaveshareEPaperTypeAModel.WAVESHARE_EPAPER_2_9_IN),
    "2.90inv2": ("a", WaveshareEPaperTypeAModel.WAVESHARE_EPAPER_2_9_IN_V2),
    "gdey029t94": ("c", GDEY029T94),
    "2.70in": ("b", WaveshareEPaper2P7In),
    "2.90in-b": ("b", WaveshareEPaper2P9InB),
    "4.20in": ("b", WaveshareEPaper4P2In),
    "4.20in-bv2": ("b", WaveshareEPaper4P2InBV2),
    "5.83in": ("b", WaveshareEPaper5P8In),
    "5.83inv2": ("b", WaveshareEPaper5P8InV2),
    "7.50in": ("b", WaveshareEPaper7P5In),
    "7.50in-bv2": ("b", WaveshareEPaper7P5InBV2),
    "7.50in-bv3": ("b", WaveshareEPaper7P5InBV3),
    "7.50in-bc": ("b", WaveshareEPaper7P5InBC),
    "7.50inv2": ("b", WaveshareEPaper7P5InV2),
    "7.50inv2alt": ("b", WaveshareEPaper7P5InV2alt),
    "7.50in-hd-b": ("b", WaveshareEPaper7P5InHDB),
    "2.13in-ttgo-dke": ("c", WaveshareEPaper2P13InDKE),
    "1.54in-m5coreink-m09": ("c", GDEW0154M09),
}


def validate_full_update_every_only_types_ac(value):
    if CONF_FULL_UPDATE_EVERY not in value:
        return value
    if MODELS[value[CONF_MODEL]][0] == "b":
        full_models = []
        for key, val in sorted(MODELS.items()):
            if val[0] != "b":
                full_models.append(key)
        raise cv.Invalid(
            "The 'full_update_every' option is only available for models "
            + ", ".join(full_models)
        )
    return value


CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(WaveshareEPaper),
            cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_MODEL): cv.one_of(*MODELS, lower=True),
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_BUSY_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_FULL_UPDATE_EVERY): cv.uint32_t,
            cv.Optional(CONF_RESET_DURATION): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(max=core.TimePeriod(milliseconds=500)),
            ),
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(spi.spi_device_schema()),
    validate_full_update_every_only_types_ac,
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
)


async def to_code(config):
    model_type, model = MODELS[config[CONF_MODEL]]
    if model_type == "a":
        rhs = WaveshareEPaperTypeA.new(model)
        var = cg.Pvariable(config[CONF_ID], rhs, WaveshareEPaperTypeA)
    elif model_type in ("b", "c"):
        rhs = model.new()
        var = cg.Pvariable(config[CONF_ID], rhs, model)
    else:
        raise NotImplementedError()

    await cg.register_component(var, config)
    await display.register_display(var, config)
    await spi.register_spi_device(var, config)

    dc = await cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
    if CONF_RESET_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))
    if CONF_BUSY_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_BUSY_PIN])
        cg.add(var.set_busy_pin(reset))
    if CONF_FULL_UPDATE_EVERY in config:
        cg.add(var.set_full_update_every(config[CONF_FULL_UPDATE_EVERY]))
    if CONF_RESET_DURATION in config:
        cg.add(var.set_reset_duration(config[CONF_RESET_DURATION]))
