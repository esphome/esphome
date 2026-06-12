/*
 * FreeRTOS static allocation callbacks for LibreTiny platforms.
 *
 * Required when configSUPPORT_STATIC_ALLOCATION is enabled. These callbacks
 * provide memory for the idle and timer tasks. Following ESP-IDF's approach,
 * we allocate from the FreeRTOS heap (pvPortMalloc) rather than using truly
 * static buffers, to avoid assumptions about memory layout.
 *
 * This enables xQueueCreateStatic, xTaskCreateStatic, etc. throughout ESPHome,
 * allowing queue storage to live in BSS with zero runtime heap allocation.
 */

#ifdef USE_BK72XX

#include <FreeRTOS.h>
#include <task.h>

#if (configSUPPORT_STATIC_ALLOCATION == 1)

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize) {
  /* Stack grows down on ARM — allocate stack first, then TCB,
   * so the stack does not grow into the TCB. */
  StackType_t *stack = (StackType_t *) pvPortMalloc(configMINIMAL_STACK_SIZE * sizeof(StackType_t));
  StaticTask_t *tcb = (StaticTask_t *) pvPortMalloc(sizeof(StaticTask_t));
  configASSERT(stack != NULL);
  configASSERT(tcb != NULL);

  *ppxIdleTaskTCBBuffer = tcb;
  *ppxIdleTaskStackBuffer = stack;
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

#if (configUSE_TIMERS == 1)

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize) {
  StackType_t *stack = (StackType_t *) pvPortMalloc(configTIMER_TASK_STACK_DEPTH * sizeof(StackType_t));
  StaticTask_t *tcb = (StaticTask_t *) pvPortMalloc(sizeof(StaticTask_t));
  configASSERT(stack != NULL);
  configASSERT(tcb != NULL);

  *ppxTimerTaskTCBBuffer = tcb;
  *ppxTimerTaskStackBuffer = stack;
  *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

#endif /* configUSE_TIMERS */

#endif /* configSUPPORT_STATIC_ALLOCATION */

#endif /* USE_BK72XX */
