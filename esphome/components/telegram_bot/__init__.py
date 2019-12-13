import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID, CONF_TRIGGER_ID, CONF_ON_MESSAGE

DEPENDENCIES = ['network']
AUTO_LOAD = ['json', 'http_request']

telegram_bot_ns = cg.esphome_ns.namespace('telegram_bot')
TelegramBotComponent = telegram_bot_ns.class_('TelegramBotComponent', cg.Component)
# TODO: Actions
# TelegramBotSendAction = telegram_bot_ns.class_('TelegramBotSendAction', automation.Action)
TelegramBotMessageUpdater = telegram_bot_ns.class_('TelegramBotMessageUpdater', cg.PollingComponent)
TelegramBotMessageTrigger = telegram_bot_ns.class_('TelegramBotMessageTrigger', automation.Trigger.template(cg.std_string))

CONF_TOKEN = 'token'
CONF_MESSAGE = 'message'
CONF_ALLOWED_CHAT_IDS = 'allowed_chat_ids'
CONF_UPDATER_ID = 'updater_id'

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(TelegramBotComponent),
    cv.Required(CONF_TOKEN): cv.string_strict,
    cv.GenerateID(CONF_UPDATER_ID): cv.declare_id(TelegramBotMessageUpdater),
    cv.Optional(CONF_ON_MESSAGE): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(TelegramBotMessageTrigger),
        cv.Optional(CONF_MESSAGE): cv.string_strict,
    }),
    cv.Optional(CONF_ALLOWED_CHAT_IDS): cv.ensure_list(cv.string),
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_token(config[CONF_TOKEN]))
    yield cg.register_component(var, config)

    for chat_id in config.get(CONF_ALLOWED_CHAT_IDS, []):
        cg.add(var.add_chat_id(chat_id))

    if CONF_ON_MESSAGE in config:
        updater = cg.new_Pvariable(config[CONF_UPDATER_ID], var)
        yield cg.register_component(updater, config)

        for conf in config[CONF_ON_MESSAGE]:
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], updater)
            if CONF_MESSAGE in conf:
                cg.add(trigger.set_message(conf[CONF_MESSAGE]))
            yield automation.build_automation(trigger, [(cg.std_string, 'x')], conf)
