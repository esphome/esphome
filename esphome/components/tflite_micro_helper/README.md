# TFLite Micro Helper

A wrapper component for the TensorFlow Lite Micro runtime on ESP32. It encapsulates the model loading, tensor arena memory management, and ESP-NN optimizations.

## ✨ Features

- **Runtime Management**: Handles the `MicroInterpreter` and `OpResolver`.
- **ESP-NN Optimization**: Automatically enables Espressif's optimized kernels for faster inference.
- **Model Verification**: Checks CRC32 of loaded models to ensure integrity.
- **Memory Management**: simplifies tensor arena allocation and monitoring.

## ⚙️ Configuration

Direct configuration is rarely needed as this is an internal helper, but debug mode can be enabled:

```yaml
tflite_micro_helper:
  debug: true
```

## 📦 Dependencies

This component pulls in the necessary IDF components:
- `espressif/esp-tflite-micro` (v1.3.7)
- `espressif/esp-nn` (1.2.3)

It also applies the necessary build flags:
- `-DTF_LITE_STATIC_MEMORY`
- `-DTF_LITE_DISABLE_X86_NEON`
- `-DESP_NN`
- `-DOPTIMIZED_KERNEL=esp_nn`

## 🧩 Usage

The `ModelHandler` class is the main entry point:

```cpp
#include "esphome/components/tflite_micro_helper/model_handler.h"

ModelHandler handler;
ModelConfig config;
// ... setup config ...

// Load
handler.load_model(model_data, model_size, config);

// Run
handler.invoke();

// Get Result
float* output = handler.output_tensor()->data.f;
```
