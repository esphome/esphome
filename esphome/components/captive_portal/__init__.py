import esphome.codegen as cg

AUTO_LOAD = ['web_server_base']


def to_code(config):
    cg.add_define('USE_CAPTIVE_PORTAL')
