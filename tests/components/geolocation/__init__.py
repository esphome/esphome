from esphome.components import geolocation
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The opt-in fields are compiled only when a component requests them
    async def to_code_testing(config):
        geolocation.request_field_age()
        geolocation.request_millisecond()
        geolocation.request_geoid_separation()

    manifest.config_schema = None
    manifest.to_code = to_code_testing
