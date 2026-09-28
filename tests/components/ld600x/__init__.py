import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        # The hub components size these through cg.slot_counter; the gtests build
        # the base without a hub, so give it the LD6002B dimensions.
        cg.add_define("LD600X_MAX_TARGETS", 3)
        cg.add_define("LD600X_AREA_KINDS", 2)

    manifest.to_code = to_code_testing
