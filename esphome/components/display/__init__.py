# coding=utf-8
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import core
from esphome.automation import ACTION_REGISTRY, maybe_simple_id
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_PAGES, CONF_ROTATION, CONF_UPDATE_INTERVAL
from esphome.core import coroutine

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

display_ns = cg.esphome_ns.namespace('display')
DisplayBuffer = display_ns.class_('DisplayBuffer')
DisplayPage = display_ns.class_('DisplayPage')
DisplayPagePtr = DisplayPage.operator('ptr')
DisplayBufferRef = DisplayBuffer.operator('ref')
DisplayPageShowAction = display_ns.class_('DisplayPageShowAction', cg.Action)
DisplayPageShowNextAction = display_ns.class_('DisplayPageShowNextAction', cg.Action)
DisplayPageShowPrevAction = display_ns.class_('DisplayPageShowPrevAction', cg.Action)

DISPLAY_ROTATIONS = {
    0: display_ns.DISPLAY_ROTATION_0_DEGREES,
    90: display_ns.DISPLAY_ROTATION_90_DEGREES,
    180: display_ns.DISPLAY_ROTATION_180_DEGREES,
    270: display_ns.DISPLAY_ROTATION_270_DEGREES,
}


def validate_rotation(value):
    value = cv.string(value)
    if value.endswith(u"°"):
        value = value[:-1]
    try:
        value = int(value)
    except ValueError:
        raise cv.Invalid(u"Expected integer for rotation")
    return cv.one_of(*DISPLAY_ROTATIONS)(value)


BASIC_DISPLAY_PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend({
    cv.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
    cv.Optional(CONF_LAMBDA): cv.lambda_,
})

FULL_DISPLAY_PLATFORM_SCHEMA = BASIC_DISPLAY_PLATFORM_SCHEMA.extend({
    cv.Optional(CONF_ROTATION): validate_rotation,
    cv.Optional(CONF_PAGES): cv.All(cv.ensure_list({
        cv.GenerateID(): cv.declare_variable_id(DisplayPage),
        cv.Required(CONF_LAMBDA): cv.lambda_,
    }), cv.Length(min=1)),
})


@coroutine
def setup_display_core_(var, config):
    if CONF_UPDATE_INTERVAL in config:
        cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    if CONF_ROTATION in config:
        cg.add(var.set_rotation(DISPLAY_ROTATIONS[config[CONF_ROTATION]]))
    if CONF_PAGES in config:
        pages = []
        for conf in config[CONF_PAGES]:
            lambda_ = yield cg.process_lambda(conf[CONF_LAMBDA], [(DisplayBufferRef, 'it')],
                                              return_type=cg.void)
            page = cg.new_Pvariable(conf[CONF_ID], lambda_)
            pages.append(page)
        cg.add(var.set_pages(pages))


@coroutine
def register_display(var, config):
    yield setup_display_core_(var, config)


@ACTION_REGISTRY.register('display.page.show', maybe_simple_id({
    cv.Required(CONF_ID): cv.templatable(cv.use_variable_id(DisplayPage)),
}))
def display_page_show_to_code(config, action_id, template_arg, args):
    type = DisplayPageShowAction.template(template_arg)
    action = cg.Pvariable(action_id, type.new(), type=type)
    if isinstance(config[CONF_ID], core.Lambda):
        template_ = yield cg.templatable(config[CONF_ID], args, DisplayPagePtr)
        cg.add(action.set_page(template_))
    else:
        var = yield cg.get_variable(config[CONF_ID])
        cg.add(action.set_page(var))
    yield action


@ACTION_REGISTRY.register('display.page.show_next', maybe_simple_id({
    cv.Required(CONF_ID): cv.templatable(cv.use_variable_id(DisplayBuffer)),
}))
def display_page_show_next_to_code(config, action_id, template_arg, args):
    var = yield cg.get_variable(config[CONF_ID])
    type = DisplayPageShowNextAction.template(template_arg)
    yield cg.Pvariable(action_id, type.new(var), type=type)


@ACTION_REGISTRY.register('display.page.show_previous', maybe_simple_id({
    cv.Required(CONF_ID): cv.templatable(cv.use_variable_id(DisplayBuffer)),
}))
def display_page_show_previous_to_code(config, action_id, template_arg, args):
    var = yield cg.get_variable(config[CONF_ID])
    type = DisplayPageShowPrevAction.template(template_arg)
    yield cg.Pvariable(action_id, type.new(var), type=type)


def to_code(config):
    cg.add_global(display_ns.using)
