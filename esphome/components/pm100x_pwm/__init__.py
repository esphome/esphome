import esphome.codegen as cg
from esphome.components import pm100x

pm100x_pwm_ns = cg.esphome_ns.namespace("pm100x_pwm")
PM100XComponentPWM = pm100x_pwm_ns.class_("PM100XComponentPWM", pm100x.PM100XComponent)
