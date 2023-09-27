import sys
from esphome.components.ade7953_i2c import sensor as ade7953_i2c
sys.path.insert(0,"..") 

# ade7953 is an alias of ade7953_i2c. ade7953 was used before
# support for spi was added

DEPENDENCIES = ade7953_i2c.DEPENDENCIES
AUTO_LOAD = ade7953_i2c.AUTO_LOAD + ["ade7953_i2c"]

CONFIG_SCHEMA = ade7953_i2c.CONFIG_SCHEMA

async def to_code(config):
    await ade7953_i2c.to_code(config)