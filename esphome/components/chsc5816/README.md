# CHSC5816 work-in-progress

*This is the start of a CHSC5816 touchscreen component.*

So far this is non-functional, but it does at least seem to be getting *some* messages from the I2C bus, although they don't seem to be valid/correct.

This component is based off of the `cst816` with a simple find and replace for the naming, and the commands / constants from [lewisxhe/SensorLib](https://github.com/lewisxhe/SensorLib/blob/master/src/TouchDrvCHSC5816.hpp).

Thus far, I've only tried to read the `CHSC5816_REG_BOOT_STATE`, which should contain the `CHSC5816_SIG_VALUE`, but it does not, nor, (as far as I can tell) a version of it with various bits/bytes swapped or in different orders, but that may need to be explored further.

The only documentation I've been able to find is [this datasheet](https://github.com/lewisxhe/SensorLib/blob/master/datasheet/CHSC5816%E8%A7%A6%E6%8E%A7%E8%8A%AF%E7%89%87%E4%BD%BF%E7%94%A8%E8%AF%B4%E6%98%8EV1-20221114.pdf) and there is not much info in there.
