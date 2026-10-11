import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        # Listener storage is sized by code generation; the gtests register one listener per hub.
        cg.add_define("MK2PVROUTER_LISTENER_COUNT", 1)

    manifest.to_code = to_code_testing
