from esphome import pins
from esphome.components import display
from esphome.components.display import ssd1306_spi
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ADDRESS, CONF_EXTERNAL_VCC, CONF_ID, CONF_LAMBDA, CONF_MODEL, \
    CONF_PAGES, CONF_RESET_PIN


DEPENDENCIES = ['i2c']

I2CSSD1306 = display.display_ns.class_('I2CSSD1306', ssd1306_spi.SSD1306)

PLATFORM_SCHEMA = cv.All(display.FULL_DISPLAY_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(I2CSSD1306),
    cv.Required(CONF_MODEL): ssd1306_spi.SSD1306_MODEL,
    cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_EXTERNAL_VCC): cv.boolean,
    cv.Optional(CONF_ADDRESS): cv.i2c_address,
}).extend(cv.COMPONENT_SCHEMA), cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA))


def to_code(config):
    ssd = Pvariable(config[CONF_ID], App.make_i2c_ssd1306())
    cg.add(ssd.set_model(ssd1306_spi.MODELS[config[CONF_MODEL]]))

    if CONF_RESET_PIN in config:
        reset = yield gpio_output_pin_expression(config[CONF_RESET_PIN])
        cg.add(ssd.set_reset_pin(reset))
    if CONF_EXTERNAL_VCC in config:
        cg.add(ssd.set_external_vcc(config[CONF_EXTERNAL_VCC]))
    if CONF_ADDRESS in config:
        cg.add(ssd.set_address(config[CONF_ADDRESS]))
    if CONF_LAMBDA in config:
        lambda_ = yield process_lambda(config[CONF_LAMBDA],
                                       [(display.DisplayBufferRef, 'it')], return_type=void)
        cg.add(ssd.set_writer(lambda_))

    display.setup_display(ssd, config)
    register_component(ssd, config)


BUILD_FLAGS = '-DUSE_SSD1306'
