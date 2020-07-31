
Library still in development but ready for first testing.

In the YAML config file the following entries can be added:
```yaml
binary_sensor:  <-- No changes made
  - platform: nextion
    page_id: 0
    component_id: 3
    name: "Nextion Component Binary"

switch:
  - platform: nextion
    page_id: 0        <-- Page number of item in Nextion
    component_id: 3   <-- Item number in Nextion
    name: "Nextion Component Switch"
    button_id: "bt0"  <-- ID Name of item in Nextion

sensor:
  - platform: nextion
    page_id: 0        <-- Page number of item in Nextion
    component_id: 5   <-- Item number in Nextion
    name: "Nextion sensor"
```
To enable switch and sensor and to enhance the binary sensor some code has to be added to the Nextion when configuring the items.

For Switch:

During press or release add the following code to Dual state push button:
```
printh 90 <-- 90 is special code I created to register feedback from switch 
printh 00 <-- Page ID
printh 03 <-- ITEM ID could be replaced with prints ID.val,1
prints bt0.val,1 Where bt0 the id of the button is = equal to id in configuration
printh FF FF FF
```
You don't have to enable the button press and release event. It still can trigger a binary sensor with same ID's

For Sensor:
```
printh 91 <-- 91 is special code I created to register feedback from value change (for example slider)
printh 00 <-- Page ID
printh 03 <-- ITEM ID could be replaced with prints ID.val,1
prints ht0.val,1 Where ht0 the id of the slider
printh FF FF FF
```
For Binary sensor:

Possible already in existing configuration with dual state push button
```
printh 65 <-- 65 is the ID to register the PRESS / RELEASE of a item  
printh 00 <-- Page ID
printh 03 <-- ITEM ID could be replaced with prints ID.val,1
prints bt0.val,1 Where bt0 the id of the button is = equal to id in configuration
printh FF FF FF
```
In this way the binary sensor registers the state of the button instead of registering the actual push. 
