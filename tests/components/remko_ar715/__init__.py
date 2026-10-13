from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The harness walks dependencies from the component manifest, not from the
    # climate platform, so declare the platform's auto-load here.
    manifest.auto_load = ["climate_ir", "climate"]
