import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The host unit test constructs two hubs directly instead of using YAML.
    manifest.multi_conf = False

    async def to_code_testing(config: ConfigType) -> None:
        cg.add_define("VL6180X_INSTANCE_COUNT", 2)

    manifest.to_code = to_code_testing
