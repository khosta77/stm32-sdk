#pragma once

#ifdef STM32_USE_FREERTOS

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

import driver.types;

namespace sensor {

template <typename DataType>
class CachedSensor {
  DataType _cached{};
  mutable SemaphoreHandle_t _mutex;
  TaskHandle_t _task = nullptr;
  TickType_t _periodTicks;

  using ReadFn = driver::Status (*)(DataType &, void *);
  ReadFn _readFn;
  void *_ctx;

  static void taskEntry(void *param) {
    auto *self = static_cast<CachedSensor *>(param);
    TickType_t lastWake = xTaskGetTickCount();
    while (true) {
      DataType sample{};
      if (self->_readFn(sample, self->_ctx) == driver::Status::Ok) {
        xSemaphoreTake(self->_mutex, portMAX_DELAY);
        self->_cached = sample;
        xSemaphoreGive(self->_mutex);
      }
      vTaskDelayUntil(&lastWake, self->_periodTicks);
    }
  }

public:
  CachedSensor(
      TickType_t periodMs,
      ReadFn fn,
      void *ctx,
      const char *name,
      uint16_t stackDepth,
      UBaseType_t priority
  )
      : _periodTicks(pdMS_TO_TICKS(periodMs)), _readFn(fn), _ctx(ctx) {
    _mutex = xSemaphoreCreateMutex();
    configASSERT(_mutex);
    xTaskCreate(taskEntry, name, stackDepth, this, priority, &_task);
    configASSERT(_task);
  }

  ~CachedSensor() {
    if (_task) {
      vTaskDelete(_task);
    }
    if (_mutex) {
      vSemaphoreDelete(_mutex);
    }
  }

  CachedSensor(const CachedSensor &) = delete;
  CachedSensor &operator=(const CachedSensor &) = delete;

  [[nodiscard]] DataType get() const {
    DataType copy;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    copy = _cached;
    xSemaphoreGive(_mutex);
    return copy;
  }
};

}  // namespace sensor

#endif
