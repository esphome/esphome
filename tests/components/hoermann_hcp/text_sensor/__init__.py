import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The platform's own to_code needs a configured hub; only its define is wanted here.
    async def to_code_testing(config: ConfigType) -> None:
        cg.add_define("USE_HOERMANN_HCP_TEXT_SENSOR")

    manifest.to_code = to_code_testing
