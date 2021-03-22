import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID, CONF_TRIGGER_ID, CONF_ON_MESSAGE, CONF_URL, CONF_TYPE

DEPENDENCIES = ["network"]
AUTO_LOAD = ["json", "http_request"]

telegram_bot_ns = cg.esphome_ns.namespace("telegram_bot")
TelegramBotComponent = telegram_bot_ns.class_("TelegramBotComponent", cg.Component)
TelegramBotSendAction = telegram_bot_ns.class_(
    "TelegramBotSendAction", automation.Action
)
TelegramBotAnswerCallbackAction = telegram_bot_ns.class_(
    "TelegramBotAnswerCallbackAction", automation.Action
)
TelegramBotMessageUpdater = telegram_bot_ns.class_(
    "TelegramBotMessageUpdater", cg.PollingComponent
)
TelegramBotMessageTrigger = telegram_bot_ns.class_(
    "TelegramBotMessageTrigger",
    automation.Trigger.template(telegram_bot_ns.struct("Message")),
)

CONF_TOKEN = "token"
CONF_SCAN_INTERVAL = "scan_interval"  # Don't use CONF_UPDATE_INTERVAL here
CONF_MESSAGE = "message"
CONF_CHAT_ID = "chat_id"
CONF_CALLBACK_QUERY_ID = "callback_query_id"
CONF_ALLOWED_CHAT_IDS = "allowed_chat_ids"
CONF_UPDATER_ID = "updater_id"
CONF_INLINE_KEYBOARD = "inline_keyboard"
CONF_TEXT = "text"
CONF_CALLBACK_DATA = "callback_data"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TelegramBotComponent),
        cv.Required(CONF_TOKEN): cv.string_strict,
        cv.GenerateID(CONF_UPDATER_ID): cv.declare_id(TelegramBotMessageUpdater),
        cv.Optional(CONF_ON_MESSAGE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    TelegramBotMessageTrigger
                ),
                cv.Optional(CONF_MESSAGE): cv.string_strict,
                cv.Optional(CONF_TYPE): cv.one_of(
                    "message", "channel_post", "callback_query", lower=True
                ),
            }
        ),
        cv.Optional(CONF_ALLOWED_CHAT_IDS): cv.ensure_list(cv.string),
        cv.Optional(CONF_SCAN_INTERVAL): cv.update_interval,
    }
).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_token(config[CONF_TOKEN]))
    for chat_id in config.get(CONF_ALLOWED_CHAT_IDS, []):
        cg.add(var.add_allowed_chat_id(chat_id))
    yield cg.register_component(var, config)

    updater = cg.new_Pvariable(config[CONF_UPDATER_ID], var)
    scan_interval = 10000
    if CONF_ON_MESSAGE not in config:
        scan_interval = cv.update_interval("never")
    elif CONF_SCAN_INTERVAL in config:
        scan_interval = config[CONF_SCAN_INTERVAL]
    cg.add(updater.set_update_interval(scan_interval))
    yield cg.register_component(updater, config)

    if CONF_ON_MESSAGE in config:
        for conf in config[CONF_ON_MESSAGE]:
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], updater)
            if CONF_MESSAGE in conf:
                cg.add(trigger.set_message(conf[CONF_MESSAGE]))
            if CONF_TYPE in conf:
                cg.add(trigger.set_type(conf[CONF_TYPE]))
            yield automation.build_automation(
                trigger, [(telegram_bot_ns.struct("Message"), "x")], conf
            )


TELEGRAM_BOT_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(TelegramBotComponent),
        cv.Required(CONF_MESSAGE): cv.templatable(cv.string),
    }
)

TELEGRAM_BOT_SEND_ACTION_SCHEMA = TELEGRAM_BOT_ACTION_SCHEMA.extend(
    {
        cv.Required(CONF_CHAT_ID): cv.ensure_list(cv.templatable(cv.string)),
        cv.Optional(CONF_INLINE_KEYBOARD): cv.ensure_list(
            cv.All(
                cv.Schema(
                    {
                        cv.Required(CONF_TEXT): cv.string,
                        cv.Optional(CONF_URL): cv.string,
                        cv.Optional(CONF_CALLBACK_DATA): cv.string,
                    }
                ),
                cv.has_exactly_one_key(CONF_URL, CONF_CALLBACK_DATA),
            )
        ),
    }
)

TELEGRAM_BOT_CALLBACK_ACTION_SCHEMA = TELEGRAM_BOT_ACTION_SCHEMA.extend(
    {
        cv.Required(CONF_CALLBACK_QUERY_ID): cv.templatable(cv.string),
    }
)


@automation.register_action(
    "telegram_bot.send_message", TelegramBotSendAction, TELEGRAM_BOT_SEND_ACTION_SCHEMA
)
def telegram_bot_send_action_to_code(config, action_id, template_arg, args):
    parent = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)

    message_ = yield cg.templatable(config[CONF_MESSAGE], args, cg.std_string)
    cg.add(var.set_message(message_))

    for chat_id_template_ in config[CONF_CHAT_ID]:
        chat_id_ = yield cg.templatable(chat_id_template_, args, cg.std_string)
        cg.add(var.add_chat_id(chat_id_))

    for btn in config.get(CONF_INLINE_KEYBOARD, []):
        cg.add(
            var.add_keyboard_button(
                btn[CONF_TEXT], btn.get(CONF_URL, ""), btn.get(CONF_CALLBACK_DATA, "")
            )
        )

    yield var


@automation.register_action(
    "telegram_bot.answer_callback_query",
    TelegramBotAnswerCallbackAction,
    TELEGRAM_BOT_CALLBACK_ACTION_SCHEMA,
)
def telegram_bot_callback_action_to_code(config, action_id, template_arg, args):
    parent = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)

    callback_id_ = yield cg.templatable(
        config[CONF_CALLBACK_QUERY_ID], args, cg.std_string
    )
    cg.add(var.set_callback_id(callback_id_))
    message_ = yield cg.templatable(config[CONF_MESSAGE], args, cg.std_string)
    cg.add(var.set_message(message_))

    yield var
