#pragma once
#ifdef USE_STM32

#if STM32F0
#include "stm32f0xx_hal.h"
#elif STM32F1
#include "stm32f1xx_hal.h"
#elif STM32F2
#include "stm32f2xx_hal.h"
#elif STM32F3
#include "stm32f3xx_hal.h"
#elif STM32F4
#include "stm32f4xx_hal.h"
#elif STM32F7
#include "stm32f7xx_hal.h"
#elif STM32H7
#include "stm32h7xx_hal.h"
#elif STM32G0
#include "stm32g0xx_hal.h"
#elif STM32G4
#include "stm32g4xx_hal.h"
#elif STM32H5
#include "stm32h5xx_hal.h"
#elif STM32H7
#include "stm32h7xx_hal.h"
#elif STM32L0
#include "stm32l0xx_hal.h"
#elif STM32L1
#include "stm32l1xx_hal.h"
#elif STM32L4
#include "stm32l4xx_hal.h"
#elif STM32L5
#include "stm32l5xx_hal.h"
#elif STM32U3
#include "stm32u3xx_hal.h"
#elif STM32U5
#include "stm32u5xx_hal.h"
#else
#error "Unsupported STM32 Family"
#endif

#endif
