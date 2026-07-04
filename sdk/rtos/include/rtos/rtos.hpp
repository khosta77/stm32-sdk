#pragma once

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"

#include "util/thread_safety.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>

namespace rtos {

class CAPABILITY("mutex") Mutex {
  SemaphoreHandle_t _handle;

public:
  Mutex() : _handle(xSemaphoreCreateMutex()) { configASSERT(_handle); }

  ~Mutex() {
    if (_handle) {
      vSemaphoreDelete(_handle);
    }
  }

  Mutex(const Mutex &) = delete;
  Mutex &operator=(const Mutex &) = delete;

  bool lock(TickType_t timeout = portMAX_DELAY) ACQUIRE() {
    return xSemaphoreTake(_handle, timeout) == pdTRUE;
  }

  void unlock() RELEASE() { xSemaphoreGive(_handle); }

  SemaphoreHandle_t handle() const { return _handle; }
};

class SCOPED_CAPABILITY LockGuard {
  Mutex &_mtx;

public:
  explicit LockGuard(Mutex &m) ACQUIRE(m) : _mtx(m) { _mtx.lock(); }
  ~LockGuard() RELEASE() { _mtx.unlock(); }
  LockGuard(const LockGuard &) = delete;
  LockGuard &operator=(const LockGuard &) = delete;
};

class BinarySemaphore {
  SemaphoreHandle_t _handle;

public:
  BinarySemaphore() : _handle(xSemaphoreCreateBinary()) {
    configASSERT(_handle);
  }

  ~BinarySemaphore() {
    if (_handle) {
      vSemaphoreDelete(_handle);
    }
  }

  BinarySemaphore(const BinarySemaphore &) = delete;
  BinarySemaphore &operator=(const BinarySemaphore &) = delete;

  bool take(TickType_t timeout = portMAX_DELAY) {
    return xSemaphoreTake(_handle, timeout) == pdTRUE;
  }

  void give() { xSemaphoreGive(_handle); }

  bool giveFromISR(BaseType_t *pxHigherPriorityTaskWoken = nullptr) {
    BaseType_t dummy = pdFALSE;
    if (!pxHigherPriorityTaskWoken) {
      pxHigherPriorityTaskWoken = &dummy;
    }
    return xSemaphoreGiveFromISR(_handle, pxHigherPriorityTaskWoken) == pdTRUE;
  }

  SemaphoreHandle_t handle() const { return _handle; }
};

template <typename T, size_t N>
class Queue {
  QueueHandle_t _handle;

public:
  Queue() : _handle(xQueueCreate(N, sizeof(T))) { configASSERT(_handle); }

  ~Queue() {
    if (_handle) {
      vQueueDelete(_handle);
    }
  }

  Queue(const Queue &) = delete;
  Queue &operator=(const Queue &) = delete;

  bool send(const T &item, TickType_t timeout = portMAX_DELAY) {
    return xQueueSend(_handle, &item, timeout) == pdTRUE;
  }

  bool
  sendFromISR(const T &item, BaseType_t *pxHigherPriorityTaskWoken = nullptr) {
    BaseType_t dummy = pdFALSE;
    if (!pxHigherPriorityTaskWoken) {
      pxHigherPriorityTaskWoken = &dummy;
    }
    return xQueueSendFromISR(_handle, &item, pxHigherPriorityTaskWoken) ==
           pdTRUE;
  }

  bool receive(T &item, TickType_t timeout = portMAX_DELAY) {
    return xQueueReceive(_handle, &item, timeout) == pdTRUE;
  }

  UBaseType_t size() const { return uxQueueMessagesWaiting(_handle); }
  bool empty() const { return size() == 0; }
  QueueHandle_t handle() const { return _handle; }
};

class Task {
  TaskHandle_t _handle = nullptr;

public:
  Task() = default;

  Task(
      const char *name,
      uint16_t stackDepth,
      UBaseType_t priority,
      void (*fn)(void *),
      void *param = nullptr
  ) {
    xTaskCreate(fn, name, stackDepth, param, priority, &_handle);
    configASSERT(_handle);
  }

  bool create(
      const char *name,
      uint16_t stackDepth,
      UBaseType_t priority,
      void (*fn)(void *),
      void *param = nullptr
  ) {
    if (_handle) {
      return false;
    }
    xTaskCreate(fn, name, stackDepth, param, priority, &_handle);
    configASSERT(_handle);
    return _handle != nullptr;
  }

  bool created() const { return _handle != nullptr; }

  ~Task() {
    if (_handle) {
      vTaskDelete(_handle);
    }
  }

  Task(const Task &) = delete;
  Task &operator=(const Task &) = delete;

  Task(Task &&other) noexcept : _handle(other._handle) {
    other._handle = nullptr;
  }

  Task &operator=(Task &&other) noexcept {
    if (this != &other) {
      if (_handle) {
        vTaskDelete(_handle);
      }
      _handle = other._handle;
      other._handle = nullptr;
    }
    return *this;
  }

  TaskHandle_t handle() const { return _handle; }

  static void delay(TickType_t ticks) { vTaskDelay(ticks); }
  static void delayUntil(TickType_t *prev, TickType_t increment) {
    vTaskDelayUntil(prev, increment);
  }
  static void startScheduler() { vTaskStartScheduler(); }
};

}  // namespace rtos
