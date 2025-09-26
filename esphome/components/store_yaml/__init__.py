import copy

from esphome import automation, yaml_util
import esphome.codegen as cg
from esphome.components import web_server_base
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
from esphome.config import strip_default_ids
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_URL,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_RTL87XX,
)
from esphome.core import CORE

platforms = [
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_BK72XX,
    PLATFORM_RTL87XX,
]

CODEOWNERS = ["@gabest11"]


def AUTO_LOAD():
    if CORE.target_platform in platforms:
        return ["web_server_base"]
    return []


CONF_SHOW_IN_DUMP_CONFIG = "show_in_dump_config"
CONF_SHOW_SECRETS = "show_secrets"
CONF_HTTP = "http"

store_yaml_ns = cg.esphome_ns.namespace("store_yaml")
StoreYamlComponent = store_yaml_ns.class_("StoreYamlComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(StoreYamlComponent),
        cv.Optional(CONF_SHOW_IN_DUMP_CONFIG, default=False): cv.boolean,
        cv.Optional(CONF_SHOW_SECRETS, default=False): cv.boolean,
        cv.Optional(CONF_HTTP): cv.Schema(
            {
                cv.Optional(CONF_URL): cv.string_strict,
                cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.All(
                    cv.use_id(web_server_base.WebServerBase),
                    cv.only_on(platforms),
                ),
            }
        ),
    }
)


class BitWriter:
    def __init__(self, output, next, bits):
        self.output = output
        self.buff = 0
        self.size = 0
        self.next = next
        self.bits = bits

    def put_bits(self, value, bits):
        self.buff = (self.buff << bits) | (value & ((1 << bits) - 1))
        self.size += bits
        while self.size >= 8:
            b = (self.buff >> (self.size - 8)) & 255
            self.buff &= (1 << (self.size - 8)) - 1
            self.output.append(b)
            self.size -= 8

    def put_code(self, code):
        if self.next == (1 << self.bits):
            self.bits += 1
        self.put_bits(code, self.bits)

    def flush(self):
        if self.size > 0:
            b = (self.buff << (8 - self.size)) & 255
            self.put_bits(b, 8)


def compress(s):
    input = s.encode("utf-8")
    output = bytearray()
    codes = {}
    for i in range(256):
        codes[chr(i)] = i
    writer = BitWriter(output, len(codes), 8)
    prev = None
    for c in input:
        pc = prev + chr(c) if prev is not None else chr(c)
        if pc not in codes:
            writer.put_code(codes[prev])
            prev = chr(c)
            codes[pc] = writer.next
            writer.next += 1
        else:
            prev = pc
    writer.put_code(codes[prev])
    writer.flush()
    return output


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    store_yaml = await cg.register_component(var, config)
    cg.add(store_yaml.set_show_in_dump_config(config[CONF_SHOW_IN_DUMP_CONFIG]))
    yaml = strip_default_ids(copy.deepcopy(CORE.config))
    dump = yaml_util.dump(yaml, show_secrets=config[CONF_SHOW_SECRETS])
    data = compress(dump)
    size = len(data)
    bytes_as_int = ", ".join(str(x) for x in data)
    uint8_t = f"const uint8_t ESPHOME_YAML[{size}] PROGMEM = {{{bytes_as_int}}}"
    size_t = f"const size_t ESPHOME_YAML_SIZE = {size}"
    cg.add_global(cg.RawExpression(uint8_t))
    cg.add_global(cg.RawExpression(size_t))
    if CONF_HTTP in config:
        http = config[CONF_HTTP]
        if CONF_URL in http:
            webserver = await cg.get_variable(http[CONF_WEB_SERVER_BASE_ID])
            cg.add(var.set_web_server(webserver, http[CONF_URL]))


LogAction = store_yaml_ns.class_("LogAction", automation.Action)

LOG_ACTION_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(): cv.use_id(StoreYamlComponent)}
)


@automation.register_action("store_yaml.log", LogAction, LOG_ACTION_SCHEMA)
async def log_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
