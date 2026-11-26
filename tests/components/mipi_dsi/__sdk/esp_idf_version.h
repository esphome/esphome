#pragma once
#if defined(USE_HOST) && defined(MIPI_TEST_IDF_MAJOR) && defined(MIPI_DSI_HOST_TEST)
#define ESP_IDF_VERSION_VAL(a, b, c) (((a) << 16) | ((b) << 8) | (c))
#define ESP_IDF_VERSION \
  ESP_IDF_VERSION_VAL(MIPI_TEST_IDF_MAJOR, (MIPI_TEST_IDF_MAJOR >= 6 ? 0 : 5), MIPI_TEST_IDF_PATCH)
#endif
