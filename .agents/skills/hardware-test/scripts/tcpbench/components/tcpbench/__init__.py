"""tcpbench: lwIP socket throughput tasks for Wi-Fi tuning A/B runs on ESP-IDF (esphome#23122).

TCP sink on 5001, TCP source on 5002, UDP counter on 5003 (bench.py drives them); asks for high performance
networking and logs the Wi-Fi buffer sdkconfig values at boot and the internal heap every 10 s.
"""

import esphome.codegen as cg
from esphome.components import network
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

DOMAIN = "tcpbench"
DEPENDENCIES = ["network"]
tcpbench_ns = cg.esphome_ns.namespace("tcpbench")
TcpBench = tcpbench_ns.class_("TcpBench", cg.Component)


def _hp(config: ConfigType) -> ConfigType:
    network.require_high_performance_networking()
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema({cv.GenerateID(): cv.declare_id(TcpBench)}).extend(cv.COMPONENT_SCHEMA),
    _hp,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
