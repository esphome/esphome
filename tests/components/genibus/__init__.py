import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        # The gtests register up to two devices on one bus
        cg.add_define("GENIBUS_DEVICE_COUNT", 2)

    manifest.to_code = to_code_testing
