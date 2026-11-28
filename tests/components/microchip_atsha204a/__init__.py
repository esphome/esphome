import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Exercise both receive paths and entity callbacks on the host.
    manifest.config_schema = cv.Schema({})
    manifest.final_validate_schema = None
    manifest.multi_conf = False
    manifest.auto_load = ["binary_sensor", "i2c", "text_sensor", "uart"]

    async def to_code(config: ConfigType) -> None:
        cg.add_define("USE_BINARY_SENSOR")
        cg.add_define("USE_MICROCHIP_ATSHA204A_I2C")
        cg.add_define("USE_MICROCHIP_ATSHA204A_SWI_UART")
        cg.add_define("USE_TEXT_SENSOR")

    manifest.to_code = to_code
