#pragma once
// Appended to esp_config.h by ESPHome's esp32 component (MBEDTLS_USER_CONFIG_FILE).
#if !defined(CONFIG_MBEDTLS_DHM_C) && !defined(CONFIG_MBEDTLS_SSL_PROTO_TLS1_3)
#undef PSA_WANT_ALG_FFDH
#undef PSA_WANT_DH_RFC7919_2048
#undef PSA_WANT_DH_RFC7919_3072
#undef PSA_WANT_DH_RFC7919_4096
#undef PSA_WANT_DH_RFC7919_6144
#undef PSA_WANT_DH_RFC7919_8192
#undef PSA_WANT_KEY_TYPE_DH_PUBLIC_KEY
#undef PSA_WANT_KEY_TYPE_DH_KEY_PAIR_BASIC
#undef PSA_WANT_KEY_TYPE_DH_KEY_PAIR_IMPORT
#undef PSA_WANT_KEY_TYPE_DH_KEY_PAIR_EXPORT
#undef PSA_WANT_KEY_TYPE_DH_KEY_PAIR_GENERATE
#undef PSA_WANT_KEY_TYPE_DH_KEY_PAIR_DERIVE
#endif
#ifndef CONFIG_MBEDTLS_RSA_C
#undef PSA_WANT_ALG_RSA_PSS
#undef PSA_WANT_ALG_RSA_PSS_ANY_SALT
#endif
// Only OpenThread (its PSA backend with platform key references) stores PSA
// keys persistently, but ESP-IDF keeps the storage layer (esp_psa_its) linked
// whenever its NVS backend is available. A custom ITS backend keeps it too.
#if !defined(CONFIG_OPENTHREAD_ENABLED) && !defined(CONFIG_MBEDTLS_PSA_ITS_CUSTOM_STORAGE_BACKEND)
#undef MBEDTLS_PSA_CRYPTO_STORAGE_C
#endif
// Only TLS uses SHA-224. Chips without SHA-224 hardware otherwise keep the
// software SHA-256 built in next to the hardware driver.
#ifndef CONFIG_MBEDTLS_TLS_ENABLED
#undef PSA_WANT_ALG_SHA_224
#endif
