"""I2C FRAM platform for NVM component.

This provides I2C FRAM (Ferroelectric RAM) support as an NVM platform.
FRAM features:
- Unlimited write cycles (no wear)
- No write delays (instant writes)
- Fast read/write operations
- Non-volatile storage

Example configuration:

    nvm:
      - platform: fram_i2c
        id: my_fram
        address: 0x50
        model: MB85RC256
        partitions:
          - id: preferences
            type: preferences
            size: 4KB
          - id: sensor_cache
            type: raw
            size: 12KB
"""

from .nvm import CONFIG_SCHEMA as CONFIG_SCHEMA, to_code as to_code

__all__ = ["CONFIG_SCHEMA", "to_code"]

CODEOWNERS = ["@pgolawsk"]
DEPENDENCIES = ["i2c"]
