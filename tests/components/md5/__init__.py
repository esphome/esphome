from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # md5's to_code calls cg.add_define("USE_MD5"), which gates md5.h. C++ unit
    # test builds that pull md5 in transitively (e.g. ota's host backend, which
    # has an md5::MD5Digest member) need that define, otherwise md5.h compiles to
    # nothing and the dependent headers fail to find md5::MD5Digest.
    manifest.enable_codegen()
