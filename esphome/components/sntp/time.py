from esphome.components import time as time_
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.core import CORE
from esphome.const import CONF_ID, CONF_SERVERS


DEPENDENCIES = ["network"]
sntp_ns = cg.esphome_ns.namespace("sntp")
SNTPComponent = sntp_ns.class_("SNTPComponent", time_.RealTimeClock)


DEFAULT_SERVERS = ["0.pool.ntp.org", "1.pool.ntp.org", "2.pool.ntp.org"]

CONFIG_SCHEMA = time_.TIME_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(SNTPComponent),
        cv.Optional(CONF_SERVERS, default=DEFAULT_SERVERS): cv.All(
            cv.ensure_list(cv.domain), cv.Length(min=1, max=3)
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    servers = config[CONF_SERVERS]
    servers += [""] * (3 - len(servers))
    cg.add(var.set_servers(*servers))

    yield cg.register_component(var, config)
    yield time_.register_time(var, config)

    if CORE.is_esp8266 and len(servers) > 1:
        # We need LwIP features enabled to get 3 SNTP servers (not just one)
        cg.add_build_flag("-DPIO_FRAMEWORK_ARDUINO_LWIP2_LOW_MEMORY")
