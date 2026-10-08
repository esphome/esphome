import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        # the session array is sized from the max_clients option
        cg.add_define("QNETD_MAX_CLIENTS", 4)

    manifest.to_code = to_code_testing
