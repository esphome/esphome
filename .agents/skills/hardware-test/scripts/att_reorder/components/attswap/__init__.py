"""Test only: wrap Bluedroid's l2c_rcv_acl_data so attswap.h can reorder a Read By Type response."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.types import ConfigType

CONFIG_SCHEMA = cv.Schema({})


async def to_code(config: ConfigType) -> None:
    cg.add_build_flag("-Wl,--wrap=l2c_rcv_acl_data")
