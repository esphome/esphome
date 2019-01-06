import voluptuous as vol

from esphomeyaml import automation
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ID, CONF_INTERVAL
from esphomeyaml.cpp_generator import Pvariable
from esphomeyaml.cpp_helpers import setup_component
from esphomeyaml.cpp_types import App, NoArg, PollingComponent, Trigger, esphomelib_ns

IntervalTrigger = esphomelib_ns.class_('IntervalTrigger', Trigger.template(NoArg), PollingComponent)

CONFIG_SCHEMA = automation.validate_automation(vol.Schema({
    vol.Required(CONF_ID): cv.declare_variable_id(IntervalTrigger),
    vol.Required(CONF_INTERVAL): cv.positive_time_period_milliseconds,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    for conf in config:
        rhs = App.register_component(IntervalTrigger.new(config[CONF_INTERVAL]))
        trigger = Pvariable(conf[CONF_ID], rhs)
        setup_component(trigger, conf)

        automation.build_automation(trigger, NoArg, conf)
