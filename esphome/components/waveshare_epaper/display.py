import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display, spi
from esphome.const import CONF_BUSY_PIN, CONF_DC_PIN, CONF_FULL_UPDATE_EVERY, \
    CONF_ID, CONF_LAMBDA, CONF_MODEL, CONF_PAGES, CONF_RESET_PIN

DEPENDENCIES = ['spi']

waveshare_epaper_ns = cg.esphome_ns.namespace('waveshare_epaper')
WaveshareEPaper = waveshare_epaper_ns.class_('WaveshareEPaper', cg.PollingComponent, spi.SPIDevice,
                                             display.DisplayBuffer)
WaveshareEPaperTypeA = waveshare_epaper_ns.class_('WaveshareEPaperTypeA', WaveshareEPaper)
WaveshareEPaper2P7In = waveshare_epaper_ns.class_('WaveshareEPaper2P7In', WaveshareEPaper)
WaveshareEPaper4P2In = waveshare_epaper_ns.class_('WaveshareEPaper4P2In', WaveshareEPaper)
WaveshareEPaper7P5In = waveshare_epaper_ns.class_('WaveshareEPaper7P5In', WaveshareEPaper)

WaveshareEPaperTypeAModel = waveshare_epaper_ns.enum('WaveshareEPaperTypeAModel')
WaveshareEPaperTypeBModel = waveshare_epaper_ns.enum('WaveshareEPaperTypeBModel')

MODELS = {
    '1.54in': ('a', WaveshareEPaperTypeAModel.WAVESHARE_EPAPER_1_54_IN),
    '2.13in': ('a', WaveshareEPaperTypeAModel.WAVESHARE_EPAPER_2_13_IN),
    '2.13in-ttgo': ('a', WaveshareEPaperTypeAModel.TTGO_EPAPER_2_13_IN),
    '2.90in': ('a', WaveshareEPaperTypeAModel.WAVESHARE_EPAPER_2_9_IN),
    '2.70in': ('b', WaveshareEPaper2P7In),
    '4.20in': ('b', WaveshareEPaper4P2In),
    '7.50in': ('b', WaveshareEPaper7P5In),
}


def validate_full_update_every_only_type_a(value):
    if CONF_FULL_UPDATE_EVERY not in value:
        return value
    if MODELS[value[CONF_MODEL]][0] != 'a':
        raise cv.Invalid("The 'full_update_every' option is only available for models "
                         "'1.54in', '2.13in' and '2.90in'.")
    return value


CONFIG_SCHEMA = cv.All(display.FULL_DISPLAY_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(WaveshareEPaper),
    cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_MODEL): cv.one_of(*MODELS, lower=True),
    cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_BUSY_PIN): pins.gpio_input_pin_schema,
    cv.Optional(CONF_FULL_UPDATE_EVERY): cv.uint32_t,
}).extend(cv.polling_component_schema('1s')).extend(spi.SPI_DEVICE_SCHEMA),
                       validate_full_update_every_only_type_a,
                       cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA))


def to_code(config):
    model_type, model = MODELS[config[CONF_MODEL]]
    if model_type == 'a':
        rhs = WaveshareEPaperTypeA.new(model)
        var = cg.Pvariable(config[CONF_ID], rhs, type=WaveshareEPaperTypeA)
    elif model_type == 'b':
        rhs = model.new()
        var = cg.Pvariable(config[CONF_ID], rhs, type=model)
    else:
        raise NotImplementedError()

    yield cg.register_component(var, config)
    yield display.register_display(var, config)
    yield spi.register_spi_device(var, config)

    dc = yield cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))

    if CONF_LAMBDA in config:
        lambda_ = yield cg.process_lambda(config[CONF_LAMBDA], [(display.DisplayBufferRef, 'it')],
                                          return_type=cg.void)
        cg.add(var.set_writer(lambda_))
    if CONF_RESET_PIN in config:
        reset = yield cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))
    if CONF_BUSY_PIN in config:
        reset = yield cg.gpio_pin_expression(config[CONF_BUSY_PIN])
        cg.add(var.set_busy_pin(reset))
    if CONF_FULL_UPDATE_EVERY in config:
        cg.add(var.set_full_update_every(config[CONF_FULL_UPDATE_EVERY]))
