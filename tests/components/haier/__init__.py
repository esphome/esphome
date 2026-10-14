from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Include the real entity classes used by the controller fixtures.
    manifest.auto_load = ["uart", "water_heater", "sensor", "binary_sensor", "text_sensor"]
