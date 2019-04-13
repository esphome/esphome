from esphome import pins
from esphome.components import display, spi
from esphome.components.spi import SPIComponent
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_BUSY_PIN, CONF_CS_PIN, CONF_DC_PIN, CONF_FULL_UPDATE_EVERY, \
    CONF_ID, CONF_LAMBDA, CONF_MODEL, CONF_PAGES, CONF_RESET_PIN, CONF_SPI_ID
register_component


DEPENDENCIES = ['spi']

WaveshareEPaperTypeA = display.display_ns.WaveshareEPaperTypeA
WaveshareEPaper = display.display_ns.class_('WaveshareEPaper',
                                            PollingComponent, spi.SPIDevice, display.DisplayBuffer)

WaveshareEPaperTypeAModel = display.display_ns.enum('WaveshareEPaperTypeAModel')
WaveshareEPaperTypeBModel = display.display_ns.enum('WaveshareEPaperTypeBModel')

MODELS = {
    '1.54in': ('a', WaveshareEPaperTypeAModel.WAVESHARE_EPAPER_1_54_IN),
    '2.13in': ('a', WaveshareEPaperTypeAModel.WAVESHARE_EPAPER_2_13_IN),
    '2.90in': ('a', WaveshareEPaperTypeAModel.WAVESHARE_EPAPER_2_9_IN),
    '2.70in': ('b', WaveshareEPaperTypeBModel.WAVESHARE_EPAPER_2_7_IN),
    '4.20in': ('b', WaveshareEPaperTypeBModel.WAVESHARE_EPAPER_4_2_IN),
    '7.50in': ('b', WaveshareEPaperTypeBModel.WAVESHARE_EPAPER_7_5_IN),
}


def validate_full_update_every_only_type_a(value):
    if CONF_FULL_UPDATE_EVERY not in value:
        return value
    if MODELS[value[CONF_MODEL]][0] != 'a':
        raise cv.Invalid("The 'full_update_every' option is only available for models "
                          "'1.54in', '2.13in' and '2.90in'.")
    return value


PLATFORM_SCHEMA = cv.All(display.FULL_DISPLAY_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(WaveshareEPaper),
    cv.GenerateID(CONF_SPI_ID): cv.use_variable_id(SPIComponent),
    cv.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_MODEL): cv.one_of(*MODELS, lower=True),
    cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_BUSY_PIN): pins.gpio_input_pin_schema,
    cv.Optional(CONF_FULL_UPDATE_EVERY): cv.uint32_t,
}).extend(cv.COMPONENT_SCHEMA), validate_full_update_every_only_type_a,
                          cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA))


def to_code(config):
    spi_ = yield get_variable(config[CONF_SPI_ID])
    cs = yield gpio_output_pin_expression(config[CONF_CS_PIN])
    dc = yield gpio_output_pin_expression(config[CONF_DC_PIN])

    model_type, model = MODELS[config[CONF_MODEL]]
    if model_type == 'a':
        rhs = App.make_waveshare_epaper_type_a(spi_, cs, dc, model)
        epaper = Pvariable(config[CONF_ID], rhs, type=WaveshareEPaperTypeA)
    elif model_type == 'b':
        rhs = App.make_waveshare_epaper_type_b(spi_, cs, dc, model)
        epaper = Pvariable(config[CONF_ID], rhs, type=WaveshareEPaper)
    else:
        raise NotImplementedError()

    if CONF_LAMBDA in config:
        lambda_ = yield process_lambda(config[CONF_LAMBDA], [(display.DisplayBufferRef, 'it')],
                                       return_type=void)
        cg.add(epaper.set_writer(lambda_))
    if CONF_RESET_PIN in config:
        reset = yield gpio_output_pin_expression(config[CONF_RESET_PIN])
        cg.add(epaper.set_reset_pin(reset))
    if CONF_BUSY_PIN in config:
        reset = yield gpio_input_pin_expression(config[CONF_BUSY_PIN])
        cg.add(epaper.set_busy_pin(reset))
    if CONF_FULL_UPDATE_EVERY in config:
        cg.add(epaper.set_full_update_every(config[CONF_FULL_UPDATE_EVERY]))

    display.setup_display(epaper, config)
    register_component(epaper, config)


BUILD_FLAGS = '-DUSE_WAVESHARE_EPAPER'
