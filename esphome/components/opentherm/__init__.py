import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID

from . import schema

CODEOWNERS = ["@kvdheijden"]

MULTI_CONF = True

opentherm_ns = cg.esphome_ns.namespace("opentherm")
OpenthermHub = opentherm_ns.class_("OpenthermHub", cg.Component)

CONF_OPENTHERM_ID = 'opentherm_id'
CONF_IN_PIN = 'in_pin'
CONF_OUT_PIN = 'out_pin'
CONF_IS_THERMOSTAT = 'is_thermostat'

CONFIG_SCHEMA = (
    cv.Schema({
        cv.GenerateID(): cv.declare_id(OpenthermHub),
        cv.Required(CONF_IN_PIN): cv.int_,
        cv.Required(CONF_OUT_PIN): cv.int_,
        cv.Optional(CONF_IS_THERMOSTAT, default=False): cv.boolean,
    })
    .extend({
        cv.Optional(key): cv.use_id(sensor.Sensor)
        for key, entity in schema.INPUTS.items()
    })
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config) -> None:
    id_ = config[CONF_ID]
    handle_interrupt_cb = f'{id_}_handle_interrupt'
    process_response_cb = f'{id_}_process_response'

    var = cg.new_Pvariable(
        id_,
        config[CONF_IN_PIN],
        config[CONF_OUT_PIN],
        config[CONF_IS_THERMOSTAT],
        cg.RawExpression(handle_interrupt_cb),
        cg.RawExpression(process_response_cb)
    )

    cg.add_global(cg.RawStatement(
        f"""void IRAM_ATTR {handle_interrupt_cb}() {{
    {id_}->handle_interrupt();
}}"""))

    cg.add_global(cg.RawStatement(
        f"""void {process_response_cb}(unsigned long response, OpenThermResponseStatus status) {{
    {id_}->process_response(response, status);
}}"""))

    await cg.register_component(var, config)

    # https://github.com/ihormelnyk/opentherm_library/blob/master/library.properties
    cg.add_library("ihormelnyk/OpenTherm Library", "1.1.5")
