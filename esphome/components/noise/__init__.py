import esphome.codegen as cg
import esphome.config_validation as cv

CODEOWNERS = ["@clydebarrow"]

CONFIG_SCHEMA = cv.Schema({})


def load_noise():
    cg.add_library("esphome/noise-c", "0.1.6")


def to_code(config):
    load_noise()
