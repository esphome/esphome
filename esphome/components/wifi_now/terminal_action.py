from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.const as ehc
from esphome.core import coroutine, Lambda

from . import const as c
from . import cpp_types as t


def maybe_simple_value_or_default(*validators, **kwargs):
    key = kwargs.pop('key', ehc.CONF_VALUE)
    validator = cv.All(*validators)

    def validate(value):
        if isinstance(value, dict):
            return validator(value)
        return validator({key: value})

    return validate


@automation.register_action(c.ACTION_ABORT, t.TerminalAction, cv.All({}))
@coroutine
def abort_action_to_code(config, action_id, template_arg, args):
    text = "sendaction->stop(); return true;"
    lambda_ = yield cg.process_lambda(Lambda(text), args, return_type=cg.bool_)
    yield cg.new_Pvariable(action_id, template_arg, lambda_)


@automation.register_action(c.ACTION_RETRY_SEND, t.TerminalAction, maybe_simple_value_or_default(
    cv.Schema({
        cv.Optional(c.CONF_MAX_RETRIES, default=2): cv.int_,
        }), key=c.CONF_MAX_RETRIES)
    )
@coroutine
def retry_action_to_code(config, action_id, template_arg, args):
    max_retries = config[c.CONF_MAX_RETRIES]
    text = "if(sendaction->get_send_attempts() < {}) " \
        "{{sendaction->stop(); sendaction->send(); return true;}} " \
        "else " \
        "{{return false;}}".format(max_retries)
    lambda_ = yield cg.process_lambda(Lambda(text), args, return_type=cg.bool_)
    yield cg.new_Pvariable(action_id, template_arg, lambda_)
