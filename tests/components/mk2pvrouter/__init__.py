import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        # The gtests install their own stand-in for the generated function.
        cg.add_define("USE_MK2PVROUTER_VALUE_PUBLISHER")

    manifest.to_code = to_code_testing
