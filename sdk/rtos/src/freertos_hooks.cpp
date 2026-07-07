#include "FreeRTOS.h"
#include "task.h"

// The application hooks keep C linkage: the FreeRTOS kernel (compiled as C)
// resolves them by their unmangled names.

#if (configSUPPORT_STATIC_ALLOCATION == 1)

namespace {
StaticTask_t xIdleTaskTCB;
StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];
StaticTask_t xTimerTaskTCB;
StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];
}  // namespace

#endif

extern "C" {

void vApplicationStackOverflowHook(
    [[maybe_unused]] TaskHandle_t xTask,
    [[maybe_unused]] char *pcTaskName
) {
  __asm volatile("cpsid i" ::: "memory");
  while (1) {
  }
}

void vApplicationMallocFailedHook() {
  __asm volatile("cpsid i" ::: "memory");
  while (1) {
  }
}

#if (configSUPPORT_STATIC_ALLOCATION == 1)

void vApplicationGetIdleTaskMemory(
    StaticTask_t **ppxIdleTaskTCBBuffer,
    StackType_t **ppxIdleTaskStackBuffer,
    uint32_t *pulIdleTaskStackSize
) {
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
  *ppxIdleTaskStackBuffer = uxIdleTaskStack;
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(
    StaticTask_t **ppxTimerTaskTCBBuffer,
    StackType_t **ppxTimerTaskStackBuffer,
    uint32_t *pulTimerTaskStackSize
) {
  *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
  *ppxTimerTaskStackBuffer = uxTimerTaskStack;
  *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

#endif

}  // extern "C"
