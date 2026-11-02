from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The harness walks the component manifest, not the climate platform, so declare its auto-load here
    manifest.auto_load = ["climate_ir", "climate"]
