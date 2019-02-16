# coding=utf-8
import voluptuous as vol

from esphome import core
from esphome.automation import ACTION_REGISTRY, maybe_simple_id
import esphome.config_validation as cv
from esphome.const import CONF_LAMBDA, CONF_ROTATION, CONF_UPDATE_INTERVAL, CONF_PAGES, CONF_ID
from esphome.core import CORE
from esphome.cpp_generator import add, process_lambda, Pvariable, templatable, get_variable
from esphome.cpp_types import esphome_ns, void, Action

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

display_ns = esphome_ns.namespace('display')
DisplayBuffer = display_ns.class_('DisplayBuffer')
DisplayPage = display_ns.class_('DisplayPage')
DisplayPagePtr = DisplayPage.operator('ptr')
DisplayBufferRef = DisplayBuffer.operator('ref')
DisplayPageShowAction = display_ns.class_('DisplayPageShowAction', Action)
DisplayPageShowNextAction = display_ns.class_('DisplayPageShowNextAction', Action)
DisplayPageShowPrevAction = display_ns.class_('DisplayPageShowPrevAction', Action)

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
        raise vol.Invalid(u"Expected integer for rotation")
    return cv.one_of(*DISPLAY_ROTATIONS)(value)


BASIC_DISPLAY_PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend({
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
    vol.Optional(CONF_LAMBDA): cv.lambda_,
})

FULL_DISPLAY_PLATFORM_SCHEMA = BASIC_DISPLAY_PLATFORM_SCHEMA.extend({
    vol.Optional(CONF_ROTATION): validate_rotation,
    vol.Optional(CONF_PAGES): vol.All(cv.ensure_list({
        cv.GenerateID(): cv.declare_variable_id(DisplayPage),
        vol.Required(CONF_LAMBDA): cv.lambda_,
    }), vol.Length(min=1)),
})


def setup_display_core_(display_var, config):
    if CONF_UPDATE_INTERVAL in config:
        add(display_var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    if CONF_ROTATION in config:
        add(display_var.set_rotation(DISPLAY_ROTATIONS[config[CONF_ROTATION]]))
    if CONF_PAGES in config:
        pages = []
        for conf in config[CONF_PAGES]:
            for lambda_ in process_lambda(conf[CONF_LAMBDA], [(DisplayBufferRef, 'it')],
                                          return_type=void):
                yield
            var = Pvariable(conf[CONF_ID], DisplayPage.new(lambda_))
            pages.append(var)
        add(display_var.set_pages(pages))


CONF_DISPLAY_PAGE_SHOW = 'display.page.show'
DISPLAY_PAGE_SHOW_ACTION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.templatable(cv.use_variable_id(DisplayPage)),
})


@ACTION_REGISTRY.register(CONF_DISPLAY_PAGE_SHOW, DISPLAY_PAGE_SHOW_ACTION_SCHEMA)
def display_page_show_to_code(config, action_id, arg_type, template_arg):
    type = DisplayPageShowAction.template(arg_type)
    action = Pvariable(action_id, type.new(), type=type)
    if isinstance(config[CONF_ID], core.Lambda):
        for template_ in templatable(config[CONF_ID], arg_type, DisplayPagePtr):
            yield None
        add(action.set_page(template_))
    else:
        for var in get_variable(config[CONF_ID]):
            yield None
        add(action.set_page(var))
    yield action


CONF_DISPLAY_PAGE_SHOW_NEXT = 'display.page.show_next'
DISPLAY_PAGE_SHOW_NEXT_ACTION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.templatable(cv.use_variable_id(DisplayBuffer)),
})


@ACTION_REGISTRY.register(CONF_DISPLAY_PAGE_SHOW_NEXT, DISPLAY_PAGE_SHOW_NEXT_ACTION_SCHEMA)
def display_page_show_next_to_code(config, action_id, arg_type, template_arg):
    for var in get_variable(config[CONF_ID]):
        yield None
    type = DisplayPageShowNextAction.template(arg_type)
    yield Pvariable(action_id, type.new(var), type=type)


CONF_DISPLAY_PAGE_SHOW_PREVIOUS = 'display.page.show_previous'
DISPLAY_PAGE_SHOW_PREVIOUS_ACTION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.templatable(cv.use_variable_id(DisplayBuffer)),
})


@ACTION_REGISTRY.register(CONF_DISPLAY_PAGE_SHOW_PREVIOUS, DISPLAY_PAGE_SHOW_PREVIOUS_ACTION_SCHEMA)
def display_page_show_previous_to_code(config, action_id, arg_type, template_arg):
    for var in get_variable(config[CONF_ID]):
        yield None
    type = DisplayPageShowPrevAction.template(arg_type)
    yield Pvariable(action_id, type.new(var), type=type)


def setup_display(display_var, config):
    CORE.add_job(setup_display_core_, display_var, config)


BUILD_FLAGS = '-DUSE_DISPLAY'
