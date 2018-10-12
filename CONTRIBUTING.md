# Contributing to esphomeyaml

esphomeyaml is a part of esphomelib and is responsible for reading in YAML configuration files,
converting them to C++ code. This code is then converted to a platformio project and compiled
with [esphomelib](https://github.com/OttoWinter/esphomelib), the C++ framework behind the project.

For a detailed guide, please see https://esphomelib.com/esphomeyaml/guides/contributing.html#contributing-to-esphomeyaml

Things to note when contributing:

 - Please test your changes :)
 - If a new feature is added or an existing user-facing feature is changed, you should also 
   update the [docs](https://github.com/OttoWinter/esphomedocs). See [contributing to esphomedocs](https://esphomelib.com/esphomeyaml/guides/contributing.html#contributing-to-esphomedocs)
   for more information.
 - Please also update the tests in the `tests/` folder. You can do so by just adding a line in one of the YAML files
   which checks if your new feature compiles correctly.
 - Sometimes I will let pull requests linger because I'm not 100% sure about them. Please feel free to ping
   me after some time.
