from esphome.components import gnss
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # nmea declares uart on its gnss platform, which the harness does not walk
    manifest.dependencies = manifest.dependencies + ["uart", "gnss"]

    # The opt-in fields are compiled only when a component requests them
    async def to_code_testing(config):
        gnss.request_field_age()
        gnss.request_millisecond()
        gnss.request_geoid_separation()

    manifest.config_schema = None
    manifest.to_code = to_code_testing
