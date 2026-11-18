import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The C++ tests build the hubs directly and register two enable pins
    manifest.dependencies = manifest.dependencies + ["sensor"]
    manifest.multi_conf = False

    async def to_code_testing(config: ConfigType) -> None:
        cg.add_define("VL6180X_ENABLE_PIN_COUNT", 2)

    manifest.to_code = to_code_testing
