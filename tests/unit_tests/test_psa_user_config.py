"""The esp32 PSA user config header keeps each mbedTLS algorithm for exactly the
sdkconfig options that need it. The C preprocessor alone decides this, so each
condition is checked on its own with the host compiler."""

from __future__ import annotations

import shutil
import subprocess

import pytest

from esphome.components.esp32 import PSA_USER_CONFIG_SOURCE

_COMPILER = next(
    (cc for cc in ("cc", "gcc", "clang") if shutil.which(cc) is not None), None
)

# TF-PSA-Crypto's crypto_config.h names, written out here rather than read
# from the header so a misspelled #undef there fails the test.
_FFDH = (
    "PSA_WANT_ALG_FFDH",
    "PSA_WANT_DH_RFC7919_2048",
    "PSA_WANT_DH_RFC7919_3072",
    "PSA_WANT_DH_RFC7919_4096",
    "PSA_WANT_DH_RFC7919_6144",
    "PSA_WANT_DH_RFC7919_8192",
    "PSA_WANT_KEY_TYPE_DH_PUBLIC_KEY",
    "PSA_WANT_KEY_TYPE_DH_KEY_PAIR_BASIC",
    "PSA_WANT_KEY_TYPE_DH_KEY_PAIR_IMPORT",
    "PSA_WANT_KEY_TYPE_DH_KEY_PAIR_EXPORT",
    "PSA_WANT_KEY_TYPE_DH_KEY_PAIR_GENERATE",
    "PSA_WANT_KEY_TYPE_DH_KEY_PAIR_DERIVE",
)
_RSA_PSS = ("PSA_WANT_ALG_RSA_PSS", "PSA_WANT_ALG_RSA_PSS_ANY_SALT")
_SHA_224 = ("PSA_WANT_ALG_SHA_224",)


def _defines(config: tuple[str, ...]) -> set[str]:
    source = "".join(
        f"#define {name} 1\n" for name in (*config, *_FFDH, *_RSA_PSS, *_SHA_224)
    )
    source += f'#include "{PSA_USER_CONFIG_SOURCE.as_posix()}"\n'
    out = subprocess.run(
        [_COMPILER, "-dM", "-E", "-x", "c", "-"],
        input=source,
        capture_output=True,
        text=True,
        check=True,
    ).stdout
    return {line.split()[1] for line in out.splitlines() if line.startswith("#define")}


@pytest.mark.skipif(_COMPILER is None, reason="needs a host C compiler")
@pytest.mark.parametrize(
    ("config", "ffdh", "rsa_pss", "sha_224"),
    [
        pytest.param((), False, False, False, id="nothing_needs_them"),
        pytest.param(
            ("CONFIG_MBEDTLS_DHM_C",), True, False, False, id="dhe_keeps_ffdh"
        ),
        pytest.param(
            ("CONFIG_MBEDTLS_SSL_PROTO_TLS1_3",),
            True,
            False,
            False,
            id="tls13_keeps_ffdh",
        ),
        pytest.param(("CONFIG_MBEDTLS_RSA_C",), False, True, False, id="rsa_keeps_pss"),
        pytest.param(
            ("CONFIG_MBEDTLS_TLS_ENABLED",), False, False, True, id="tls_keeps_sha224"
        ),
    ],
)
def test_psa_user_config_conditions(
    config: tuple[str, ...], ffdh: bool, rsa_pss: bool, sha_224: bool
) -> None:
    defines = _defines(config)
    assert {name for name in _FFDH if (name in defines) is not ffdh} == set()
    assert {name for name in _RSA_PSS if (name in defines) is not rsa_pss} == set()
    assert {name for name in _SHA_224 if (name in defines) is not sha_224} == set()
