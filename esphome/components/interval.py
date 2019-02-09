import voluptuous as vol

from esphome import automation
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INTERVAL
from esphome.cpp_generator import Pvariable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, NoArg, PollingComponent, Trigger, esphome_ns

IntervalTrigger = esphome_ns.class_('IntervalTrigger', Trigger.template(NoArg), PollingComponent)

CONFIG_SCHEMA = automation.validate_automation(vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(IntervalTrigger),
    vol.Required(CONF_INTERVAL): cv.positive_time_period_milliseconds,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    for conf in config:
        rhs = App.register_component(IntervalTrigger.new(conf[CONF_INTERVAL]))
        trigger = Pvariable(conf[CONF_ID], rhs)
        setup_component(trigger, conf)

        automation.build_automation(trigger, NoArg, conf)
