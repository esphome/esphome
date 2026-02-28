from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
from esphome.components.uart import validate_raw_data
import esphome.config_validation as cv
from esphome.const import CONF_DATA, CONF_ID, CONF_TRIGGER_ID
from esphome.core import ID

virtual_uart_ns = cg.esphome_ns.namespace("virtual_uart")
VirtualUART = virtual_uart_ns.class_(
    "VirtualUARTComponent", uart.UARTComponent, cg.Component
)
VirtualUARTInjectRXAction = virtual_uart_ns.class_(
    "VirtualUARTInjectRXAction", automation.Action
)

VirtualUARTTXTrigger = virtual_uart_ns.class_(
    "VirtualUARTTXTrigger",
    automation.Trigger.template(cg.std_vector.template(cg.uint8)),
)

CONF_ON_TX = "on_tx"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(VirtualUART),
        cv.Optional(CONF_ON_TX): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(VirtualUARTTXTrigger),
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)

CONFIG_INJECT_RX_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(VirtualUART),
        cv.Required("data"): cv.templatable(validate_raw_data),
    },
    key=CONF_DATA,
)


@automation.register_action(
    "virtual_uart.inject_rx", VirtualUARTInjectRXAction, CONFIG_INJECT_RX_SCHEMA
)
async def inject_rx_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    data = config[CONF_DATA]
    if isinstance(data, bytes):
        data = list(data)

    if cg.is_template(data):
        templ = await cg.templatable(data, args, cg.std_vector.template(cg.uint8))
        cg.add(var.set_data_template(templ))
    else:
        # Generate static array in flash to avoid RAM copy
        arr_id = ID(f"{action_id}_data", is_declaration=True, type=cg.uint8)
        arr = cg.static_const_array(arr_id, cg.ArrayInitializer(*data))
        cg.add(var.set_data_static(arr, len(data)))
    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    for conf in config.get(CONF_ON_TX, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(cg.std_vector.template(cg.uint8), "data")], conf
        )
