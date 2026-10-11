import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The gtest binary has no YAML switch to emit USE_SWITCH_RESTORE_MODE_ON_RESET, so define it here.
    async def to_code_testing(config: ConfigType) -> None:
        cg.add_define("USE_SWITCH_RESTORE_MODE_ON_RESET")

    manifest.to_code = to_code_testing
