import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The host exercises the SWI receive path with a virtual UART.
    manifest.config_schema = cv.Schema({})
    manifest.final_validate_schema = None
    manifest.multi_conf = False
    manifest.auto_load = ["uart"]

    async def to_code(config: ConfigType) -> None:
        cg.add_define("USE_MICROCHIP_ATSHA204A_SWI_UART")

    manifest.to_code = to_code
