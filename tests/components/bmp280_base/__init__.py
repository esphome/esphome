from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The base driver publishes to sensor entities, so pull in what it builds against.
    manifest.dependencies = manifest.dependencies + ["sensor"]
