# Minimal component "cyclic_dep" that depends on itself
import esphome.config_validation as cv

DEPENDENCIES = ["building_cyclic_dependency"]

CONFIG_SCHEMA = cv.Schema({})
