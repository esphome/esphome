import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # One empty config entry so to_code runs
    manifest.multi_conf = False

    async def to_code_testing(config: ConfigType) -> None:
        # The timer sampler is plain C++ behind this define; the BK72xx pin and clock reads stay out.
        cg.add_define("USE_REMOTE_RECEIVER_TIMER_SAMPLING")

    manifest.to_code = to_code_testing
    # AUTO_LOAD is not resolved by the unit test build.
    manifest.dependencies = manifest.dependencies + ["remote_base"]
