from esphome import automation
import esphome.codegen as cg
from esphome.components import uart, web_server_base
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TRIGGER_ID

AUTO_LOAD = ["json", "api"]
CODEOWNERS = ["@FredM67", "@TrystanLea", "@glynhudson"]

emontx_ns = cg.esphome_ns.namespace("emontx")
EmonTx = emontx_ns.class_("EmonTx", cg.PollingComponent, uart.UARTDevice)

# Add trigger class for on_json
EmonTxJsonTrigger = emontx_ns.class_(
    "EmonTxJsonTrigger", automation.Trigger.template(cg.JsonObject)
)

# Action to send command to emonTx
EmonTxSendCommandAction = emontx_ns.class_("EmonTxSendCommandAction", automation.Action)

CONF_EMONTX_ID = "emontx_id"
CONF_TAG_NAME = "tag_name"
CONF_ON_JSON = "on_json"
CONF_WEB_CONFIG = "web_config"

EMONTX_LISTENER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_EMONTX_ID): cv.use_id(EmonTx),
        cv.Required(CONF_TAG_NAME): cv.string,
    }
)


# Main configuration schema
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(EmonTx),
            # Add on_json trigger for handling JSON data
            cv.Optional(CONF_ON_JSON): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(EmonTxJsonTrigger),
                }
            ),
            # Optional web config interface (proxy to OEM serial config)
            cv.Optional(CONF_WEB_CONFIG, default=False): cv.boolean,
            # Web server base ID (auto-resolved when only one web server exists)
            cv.GenerateID(web_server_base.CONF_WEB_SERVER_BASE_ID): cv.use_id(
                web_server_base.WebServerBase
            ),
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


def final_validate(config):
    # TX is required when web_config is enabled (for sending commands)
    require_tx = config.get(CONF_WEB_CONFIG, False)
    schema = uart.final_validate_device_schema(
        "emontx",
        baud_rate=115200,
        require_tx=require_tx,
        require_rx=True,
        data_bits=8,
        parity=None,
        stop_bits=1,
    )
    return schema(config)


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    # Process on_json triggers
    if CONF_ON_JSON in config:
        for conf in config[CONF_ON_JSON]:
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
            await automation.build_automation(
                trigger,
                [
                    (cg.JsonObject, "json"),
                    (cg.std_string, "raw_json"),
                ],
                conf,
            )

    # Enable web config interface if configured
    if config[CONF_WEB_CONFIG]:
        cg.add_define("USE_EMONTX_WEB_CONFIG")
        # Get web server base (auto-resolved via cv.GenerateID)
        web_server = await cg.get_variable(
            config[web_server_base.CONF_WEB_SERVER_BASE_ID]
        )
        cg.add(var.set_web_server(web_server))


# Action: emontx.send_command
CONF_COMMAND = "command"

EMONTX_SEND_COMMAND_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(EmonTx),
        cv.Required(CONF_COMMAND): cv.templatable(cv.string),
    }
)


@automation.register_action(
    "emontx.send_command", EmonTxSendCommandAction, EMONTX_SEND_COMMAND_ACTION_SCHEMA
)
async def emontx_send_command_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_COMMAND], args, cg.std_string)
    cg.add(var.set_command(template_))
    return var
