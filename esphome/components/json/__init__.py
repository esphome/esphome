import esphome.codegen as cg

json_ns = cg.esphome_ns.namespace('json')


def to_code(config):
    cg.add_library('ArduinoJson-esphomelib', '5.13.3')
    cg.add_define('USE_JSON')
    cg.add_global(json_ns.using)
