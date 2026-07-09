.. |tflite_micro_helper| replace:: ``tflite_micro_helper``

tflite_micro_helper
=====================

    TensorFlow Lite Micro helper component for ESPHome

The ``tflite_micro_helper`` component provides a reusable TensorFlow Lite Micro runtime for ESP32 microcontrollers using the ESP-IDF framework. It manages model loading, tensor arena memory allocation, operator registration, and inference execution, serving as a foundation for TFLite-based components such as meter readers or object classifiers.

.. note::

    This is a **helper/library component** that is typically used as a dependency by other components. It does not expose sensors or entities directly. It currently supports **ESP32** targets with the **ESP-IDF** framework only.

.. code-block:: yaml

    # Example configuration entry
    tflite_micro_helper:
      debug: false

Configuration variables:
------------------------

- **debug** (*Optional*, boolean): Enable debug logging and diagnostic features. Defaults to ``false``. When enabled, the ``DEBUG_TFLITE_MICRO_HELPER`` define is set, which enables additional verbose logging for model loading, tensor inspection, and inference debugging.

ESP-IDF Component Dependencies
-------------------------------

This component automatically adds the following ESP-IDF components via ``esp32.add_idf_component()``:

- `espressif/esp-tflite-micro` (version 1.3.7)
- `espressif/esp-nn` (version 1.2.3)

Build flags:
- ``-DTF_LITE_STATIC_MEMORY``
- ``-DTF_LITE_DISABLE_X86_NEON``
- ``-DESP_NN``
- ``-DOPTIMIZED_KERNEL=esp_nn``

Platform Support
------------------

+------------+------------+------------------+
| Platform   | Supported  | Framework        |
+============+============+==================+
| ESP32      | Yes        | ESP-IDF          |
+------------+------------+------------------+
| ESP32-S2   | Yes        | ESP-IDF          |
+------------+------------+------------------+
| ESP32-S3   | Yes        | ESP-IDF          |
+------------+------------+------------------+
| ESP32-C3   | Yes        | ESP-IDF          |
+------------+------------+------------------+
| ESP32-C6   | Yes        | ESP-IDF          |
+------------+------------+------------------+
| ESP32-H2   | Yes        | ESP-IDF          |
+------------+------------+------------------+
| ESP8266    | No         | N/A              |
+------------+------------+------------------+
| RP2040     | No         | N/A              |
+------------+------------+------------------+

Usage as a Dependency
----------------------

Components that require TFLite Micro should declare a dependency:

.. code-block:: python

    DEPENDENCIES = ["tflite_micro_helper"]

This ensures the required build flags and IDF components are configured before the dependent component's code generation runs.

Memory Management
------------------

The tensor arena is allocated using one of three strategies (in priority order):

1. **PSRAM** (preferred): When PSRAM is available, the tensor arena is allocated there to avoid exhausting internal SRAM.
2. **Internal SRAM** (fallback): When no PSRAM is present, the arena is allocated from internal RAM.
3. **Force flags**: Build flags ``-DTFLITE_FORCE_SRAM`` or ``-DTFLITE_FORCE_PSRAM`` override the automatic selection.

C++ API
--------

The component exposes C++ classes in the ``esphome::tflite_micro_helper`` namespace:

- ``TFLiteMicroHelper``: High-level class wrapping model loading, memory management, and inference.
- ``ModelHandler``: Low-level model interpreter management with operator resolution.
- ``MemoryManager``: Memory allocation strategies for the tensor arena.
- ``OpResolverManager``: Template-based operator registration using the ``tflm_operators.h`` X-macro list.

See Also
---------

- :doc:`/components/meter_reader_tflite`
- :doc:`/components/esp32`
- :doc:`/components/esp32_ble`
- `TensorFlow Lite Micro <https://www.tensorflow.org/lite/micro>`__
- `ESP-TFLite-Micro <https://components.espressif.com/components/espressif/esp-tflite-micro>`__
