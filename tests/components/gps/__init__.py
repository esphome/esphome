from esphome.components import gps
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The opt-in fields are compiled only when another component requests them. As a single
    # entry the test config gives the stub one run instead of none.
    async def to_code_testing(config):
        gps.request_field_age()
        gps.request_millisecond()
        gps.request_geoid_separation()

    manifest.multi_conf = False
    manifest.config_schema = None
    manifest.to_code = to_code_testing
