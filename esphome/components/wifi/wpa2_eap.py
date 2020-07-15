"""Module for all WPA2 Utilities.

The cryptography package is loaded lazily in the functions
so that it doesn't crash if it's not installed.
"""
import logging

import esphome.config_validation as cv
from esphome.const import CONF_USERNAME, CONF_IDENTITY, CONF_PASSWORD, CONF_CERTIFICATE, \
    CONF_KEY


_LOGGER = logging.getLogger(__name__)


def validate_cryptography_installed():
    try:
        import cryptography
    except ImportError:
        raise cv.Invalid("This settings requires the cryptography python package. "
                         "Please install it with `pip install cryptography`")

    if cryptography.__version__[0] < '2':
        raise cv.Invalid("Please update your python cryptography installation to least 2.x "
                         "(pip install -U cryptography)")


def wrapped_load_pem_x509_certificate(value):
    validate_cryptography_installed()

    from cryptography import x509
    from cryptography.hazmat.backends import default_backend

    return x509.load_pem_x509_certificate(value.encode('UTF-8'), default_backend())


def wrapped_load_pem_private_key(value, password):
    validate_cryptography_installed()

    from cryptography.hazmat.primitives.serialization import load_pem_private_key
    from cryptography.hazmat.backends import default_backend

    if password:
        password = password.encode("UTF-8")
    return load_pem_private_key(value.encode('UTF-8'), password, default_backend())


def validate_certificate(value):
    value = cv.string_strict(value)
    try:
        wrapped_load_pem_x509_certificate(value)  # raises ValueError
        return value
    except ValueError as err:
        return cv.Invalid(f"Invalid certificate: {err}")


def validate_eap(value):
    validate_cryptography_installed()

    from cryptography.hazmat.primitives.asymmetric import rsa, ec, ed448, ed25519

    if CONF_USERNAME in value:
        if CONF_IDENTITY not in value:
            _LOGGER.info("EAP 'identity:' is not set, assuming username.")
            value = value.copy()
            value[CONF_IDENTITY] = value[CONF_USERNAME]
        if CONF_PASSWORD not in value:
            raise cv.Invalid("You cannot use the EAP 'username:' option without a 'password:'. "
                             "Please provide the 'password:' key")

    if CONF_CERTIFICATE in value or CONF_KEY in value:
        if CONF_CERTIFICATE not in value and CONF_KEY not in value:
            raise cv.Invalid("You have provided an EAP 'certificate:' or 'key:' without providing "
                             "the other. Please check you have provided both.")

        # Check the key is valid and for this certificate, just to check the user hasn't pasted
        # the wrong thing. I write this after I spent a while debugging that exact issue.
        # This may require a password to decrypt to key, so we should verify that at the same time.
        cert_pw = value.get(CONF_PASSWORD)

        cert = wrapped_load_pem_x509_certificate(value[CONF_CERTIFICATE])
        try:
            key = wrapped_load_pem_private_key(value[CONF_KEY], cert_pw)
        except ValueError as e:
            raise cv.Invalid(f"There was an error with the EAP 'password:' provided for 'key' {e}")
        except TypeError as e:
            raise cv.Invalid(f"There was an error with the EAP 'key:' provided: {e}")

        doesnt_match = False

        if isinstance(key, rsa.RSAPrivateKey):
            if key.public_key().public_numbers() != cert.public_key().public_numbers():
                doesnt_match = True
        elif isinstance(key, ec.EllipticCurvePrivateKey):
            if key.public_key().public_numbers() != cert.public_key().public_numbers():
                doesnt_match = True
        elif isinstance(key, ed448.Ed448PrivateKey):
            if key.public_key() != cert:
                doesnt_match = True
        elif isinstance(key, ed25519.Ed25519PrivateKey):
            if key.public_key() != cert:
                doesnt_match = True
        else:
            _LOGGER.warning(
                "Unrecognised EAP 'certificate:' 'key:' pair format: %s. Proceed with caution!",
                type(key)
            )

        if doesnt_match:
            raise cv.Invalid("The provided EAP 'key' is not valid for the 'certificate'.")
    return value
