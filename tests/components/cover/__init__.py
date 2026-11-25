import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The tests attach the state triggers, which a cover config with one of them enables.
    async def to_code_testing(config: ConfigType) -> None:
        cg.add_define("USE_COVER_STATE_TRIGGERS")

    manifest.to_code = to_code_testing
