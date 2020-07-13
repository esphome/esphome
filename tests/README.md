# Tests for ESPHome

This directory contains some tests for ESPHome.
At the moment, all the tests only work by simply executing
`esphome` over some YAML files that are made to test
whether the yaml gets converted to the proper C++ code.

Of course this is all just very high-level and things like
unit tests would be much better. So if you have time and know
how to set up a unit testing framework for python, please do
give it a try.

When adding entries in test_.yaml files we usually need only
one file updated, unless conflicting code is generated for
different configurations, e.g. `wifi` and `ethernet` cannot
be tested on the same device.

Current test_.yaml file contents.

| Test name | Platform | Network |
|-|-|-|
| test1.yaml | ESP32 | wifi |
| test2.yaml | ESP32 | ethernet |
| test3.yaml | ESP8266 | wifi |
| test4.yaml | ESP32 | ethernet |
