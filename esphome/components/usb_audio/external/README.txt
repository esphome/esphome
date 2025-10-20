Only file changed from version 1.5.1 source is "idf_component.yml" adding "esp32p4" as "target".

Copy from
https://github.com/espressif/esp-iot-solution/tree/0a268e15c4645edf2bcfdde22356ce26a361817b/components/usb/usb_stream
into sub folder "usb_stream" and change file above.


If build with "python script/test_build_components -c usb_audio -t esp32-p4-idf" fails:
Remove-Item -Recurse -Force tests\test_build_components\build\.esphome\build\componenttestesp32p4idf -ErrorAction SilentlyContinue; $env:PLATFORMIO_CORE_DIR = "${PWD}\.platformio"; python script/test_build_components -c usb_audio -t esp32-p4-idf

In case of caching permissions error:
$env:PLATFORMIO_CORE_DIR = "${PWD}\.platformio"; python script/run-in-env.py esphome run config/usb_audio_test.yaml --device COM35
