from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The combination sensors build against the sensor component.
    manifest.dependencies = manifest.dependencies + ["sensor"]
