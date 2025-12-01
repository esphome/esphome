# XENSIV&trade; DPS-310/368 pressure sensor

### Overview

This library provides functions for interfacing with the XENSIV&trade; DPS-310/368 barometric pressure sensors. These are miniaturized digital barometric air pressure sensors with ultra-high precision (±2 cm) and a low current consumption, which can measure both pressure and temperature. The pressure sensor element is based on a capacitive sensing principle, which guarantees high precision during temperature changes. This library can be setup to use the ModusToolbox&trade; HAL interface used on the [PSOC&trade; Edge E84 AI Kit](https://www.infineon.com/KIT_PSE84_AI), or using user-provided communication functions.

[Pressure sensors for IoT](https://www.infineon.com/products/sensor/pressure-sensors/pressure-sensors-for-iot)


## Quick start

Follow these steps to create a simple application, which outputs the magnetometer data from the sensor to the UART.

1. Create a new application in the ModusToolbox&trade; and select the appropriate board support package (BSP)
2. Select the [PSOC&trade; Edge MCU: Hello world](https://github.com/Infineon/mtb-example-psoc-edge-hello-world) application and create it
3. Add the sensor-xensiv-dps3xx library to the application
4. Place the following code in the *main.c* file of the non-secure application of your project (*proj_cm33_ns*)

    ```
    #include "cybsp.h"
    #include "mtb_hal.h"
    #include "retarget_io_init.h"
    #include "mtb_xensiv_dps3xx.h"

    cy_stc_scb_i2c_context_t i2c_controller_context;
    mtb_hal_i2c_t CYBSP_I2C_CONTROLLER_hal_obj;

    xensiv_dps3xx_t pressure_sensor;

    int main(void)
    {
        cy_rslt_t result;

        /* Initializes the device and board peripherals. */
        result = cybsp_init();

        /* Board initialization failed. Stops program execution. */
        if (CY_RSLT_SUCCESS != result)
        {
            CY_ASSERT(0);
        }

        /* Enables global interrupts. */
        __enable_irq();

        /* Initializes retarget-io middleware. */
        init_retarget_io();

        /* Initializes I2C. */
        result = Cy_SCB_I2C_Init(CYBSP_I2C_CONTROLLER_HW,
                                 &CYBSP_I2C_CONTROLLER_config,
                                 &i2c_controller_context);

        if (CY_RSLT_SUCCESS != result)
        {
            CY_ASSERT(0);
        }

        Cy_SCB_I2C_Enable(CYBSP_I2C_CONTROLLER_HW);

        /* Configures the I2C master interface with the desired clock frequency. */
        result = mtb_hal_i2c_setup(&CYBSP_I2C_CONTROLLER_hal_obj,
                                   &CYBSP_I2C_CONTROLLER_hal_config,
                                   &i2c_controller_context,
                                   NULL);

        if (CY_RSLT_SUCCESS != result)
        {
            CY_ASSERT(0);
        }

        /* Initializes pressure sensor. */
        result = mtb_xensiv_dps3xx_init_i2c(&pressure_sensor, &CYBSP_I2C_CONTROLLER_hal_obj,
                                            XENSIV_DPS3XX_I2C_ADDR_DEFAULT);
        if (CY_RSLT_SUCCESS != result)
        {
            CY_ASSERT(0);
        }

        for (;;)
        {
            /* Gets the pressure and temperature data and print the results to the UART. */
            float pressure, temperature;
            xensiv_dps3xx_read(&pressure_sensor, &pressure, &temperature);

            printf("Pressure   : %8f\r\n", (double)pressure);
            printf("Temperature: %8f\r\n\r\n", (double)temperature);

            Cy_SysLib_Delay(1000);
        }
    }
    ```
5. Builds the application and program the kit


## More information

For more information, see the following documents:

* [API reference guide](https://infineon.github.io/sensor-xensiv-dps3xx/html/index.html)
* [ModusToolbox&trade; software environment, quick start guide, documentation, and videos](https://www.infineon.com/modustoolbox)
* [AN235935](https://www.infineon.com/AN235935) – Getting started with PSOC&trade; Edge MCU on ModusToolbox&trade; application note
* [Infineon Technologies AG](https://www.infineon.com)

-----
© 2025, Cypress Semiconductor Corporation (an Infineon company) or an affiliate of Cypress Semiconductor Corporation.
