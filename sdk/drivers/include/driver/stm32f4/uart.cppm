module;
#include <cstddef>
#include <cstdint>
#include <span>
#include "cmsis/stm32f4xx.h"
#ifdef STM32_USE_FREERTOS
#include "FreeRTOS.h"
#include "semphr.h"
#endif
export module driver.stm32f4.uart;

import driver.types;
import driver.uart;
import driver.reg;
import driver.circular_buffer;
import driver.stm32f4.clock;
import driver.stm32f4.dma;

namespace driver::stm32f4::detail {

void dsb() {
    __DSB();
}

void nvicSetPriority(IRQn_Type irq, uint32_t prio) {
    NVIC_SetPriority(irq, prio);
}

void nvicEnableIRQ(IRQn_Type irq) {
    NVIC_EnableIRQ(irq);
}

void nvicDisableIRQ(IRQn_Type irq) {
    NVIC_DisableIRQ(irq);
}

}  // namespace driver::stm32f4::detail

export namespace driver {
namespace stm32f4 {

enum class UartMode : uint8_t {
    Interrupt,
    Dma,
};

template <size_t RxBufSize = 256, size_t TxBufSize = 256, UartMode Mode = UartMode::Interrupt>
class Uart {
    static_assert((RxBufSize & (RxBufSize - 1)) == 0, "RxBufSize must be power of 2");
    static_assert((TxBufSize & (TxBufSize - 1)) == 0, "TxBufSize must be power of 2");
    static_assert(RxBufSize >= 16, "RxBufSize too small");
    static_assert(TxBufSize >= 16, "TxBufSize too small");

public:
    struct Config {
        uint32_t baudrate;
        uint8_t dataBits;
        uint8_t stopBits;
        Parity parity;
    };

private:
    USART_TypeDef &_periph;
    IRQn_Type const _irqn;
    CircularBuffer<uint8_t, RxBufSize> _rxBuf;
    CircularBuffer<uint8_t, TxBufSize> _txBuf;

#ifdef STM32_USE_FREERTOS
    SemaphoreHandle_t _rxSem = nullptr;
    SemaphoreHandle_t _txSem = nullptr;
    SemaphoreHandle_t _mutex = nullptr;
#endif

public:
    Uart(USART_TypeDef &periph, IRQn_Type irqn, const Config &cfg) : _periph(periph), _irqn(irqn) {
        uint32_t pclk;
        if (&_periph == USART1 || &_periph == USART6) {
            pclk = getApb2Clock();
        } else {
            pclk = getApb1Clock();
        }

        reg::write(_periph.CR1, 0);
        reg::write(_periph.CR2, 0);
        reg::write(_periph.CR3, 0);

        reg::write(_periph.BRR, (pclk + cfg.baudrate / 2) / cfg.baudrate);

        uint32_t cr1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;

        if (cfg.dataBits == 9) {
            cr1 |= USART_CR1_M;
        }

        if (cfg.parity == Parity::Even) {
            cr1 |= USART_CR1_PCE;
        } else if (cfg.parity == Parity::Odd) {
            cr1 |= USART_CR1_PCE | USART_CR1_PS;
        }

        if (cfg.stopBits == 2) {
            reg::set(_periph.CR2, USART_CR2_STOP_1);
        }

        reg::write(_periph.CR1, cr1);

        detail::nvicSetPriority(_irqn, 6);
        detail::nvicEnableIRQ(_irqn);

#ifdef STM32_USE_FREERTOS
        _rxSem = xSemaphoreCreateBinary();
        _txSem = xSemaphoreCreateBinary();
        _mutex = xSemaphoreCreateMutex();
        configASSERT(_rxSem);
        configASSERT(_txSem);
        configASSERT(_mutex);
        xSemaphoreGive(_txSem);
#endif
    }

    ~Uart() {
        detail::nvicDisableIRQ(_irqn);
        reg::write(_periph.CR1, 0);

#ifdef STM32_USE_FREERTOS
        if (_rxSem) {
            vSemaphoreDelete(_rxSem);
        }
        if (_txSem) {
            vSemaphoreDelete(_txSem);
        }
        if (_mutex) {
            vSemaphoreDelete(_mutex);
        }
#endif
    }

    Uart(const Uart &) = delete;
    Uart &operator=(const Uart &) = delete;

    [[nodiscard]] size_t write(std::span<const uint8_t> data) {
#ifdef STM32_USE_FREERTOS
        xSemaphoreTake(_mutex, portMAX_DELAY);
#endif
        size_t sent = 0;
        size_t total = data.size();
        while (sent < total) {
            if (_txBuf.push(data[sent]) == Status::Ok) {
                reg::set(_periph.CR1, USART_CR1_TXEIE);
                ++sent;
            } else {
#ifdef STM32_USE_FREERTOS
                reg::set(_periph.CR1, USART_CR1_TXEIE);
                xSemaphoreTake(_txSem, portMAX_DELAY);
#else
                while (_txBuf.full()) {
                }
#endif
            }
        }
#ifdef STM32_USE_FREERTOS
        xSemaphoreGive(_mutex);
#endif
        return sent;
    }

    [[nodiscard]] size_t read(std::span<uint8_t> data) {
        size_t received = 0;
        size_t total = data.size();
        while (received < total) {
            uint8_t byte;
            if (_rxBuf.pop(byte) == Status::Ok) {
                data[received++] = byte;
            } else {
#ifdef STM32_USE_FREERTOS
                if (xSemaphoreTake(_rxSem, portMAX_DELAY) != pdTRUE) {
                    break;
                }
#else
                if (received > 0) {
                    break;
                }
                while (_rxBuf.empty()) {
                }
#endif
            }
        }
        return received;
    }

    [[nodiscard]] size_t writeNonBlocking(std::span<const uint8_t> data) {
        size_t written = _txBuf.write(data.data(), data.size());
        if (written > 0) {
            reg::set(_periph.CR1, USART_CR1_TXEIE);
        }
        return written;
    }

    [[nodiscard]] size_t readNonBlocking(std::span<uint8_t> data) { return _rxBuf.read(data.data(), data.size()); }

    [[nodiscard]] size_t rxAvailable() const { return _rxBuf.size(); }
    [[nodiscard]] size_t txFree() const { return _txBuf.free_space(); }

    void irqHandler() {
        const uint32_t sr = reg::get(_periph.SR);
#ifdef STM32_USE_FREERTOS
        BaseType_t woken = pdFALSE;
#endif

        if (sr & USART_SR_RXNE) {
            const uint8_t byte = static_cast<uint8_t>(reg::get(_periph.DR));
            (void) _rxBuf.push(byte);  // ISR: drop byte if RX buffer is full
#ifdef STM32_USE_FREERTOS
            BaseType_t w = pdFALSE;
            if (_rxSem) {
                xSemaphoreGiveFromISR(_rxSem, &w);
            }
            woken |= w;
#endif
        }

        if (sr & USART_SR_TXE) {
            uint8_t byte;
            if (_txBuf.pop(byte) == Status::Ok) {
                reg::write(_periph.DR, byte);
            } else {
                reg::clear(_periph.CR1, USART_CR1_TXEIE);
#ifdef STM32_USE_FREERTOS
                BaseType_t w = pdFALSE;
                if (_txSem) {
                    xSemaphoreGiveFromISR(_txSem, &w);
                }
                woken |= w;
#endif
            }
        }

        if (sr & (USART_SR_ORE | USART_SR_NE | USART_SR_FE | USART_SR_PE)) {
            (void) reg::get(_periph.DR);
        }

#ifdef STM32_USE_FREERTOS
        portYIELD_FROM_ISR(woken);
#endif
    }
};

template <size_t RxBufSize, size_t TxBufSize>
class Uart<RxBufSize, TxBufSize, UartMode::Dma> {
    static_assert((RxBufSize & (RxBufSize - 1)) == 0, "RxBufSize must be power of 2");
    static_assert(RxBufSize >= 16, "RxBufSize too small");

public:
    struct Config {
        uint32_t baudrate;
        uint8_t dataBits;
        uint8_t stopBits;
        Parity parity;
    };

private:
    USART_TypeDef &_periph;
    IRQn_Type const _irqn;
    DmaStream _txDma;
    CircularBuffer<uint8_t, RxBufSize> _rxBuf;

#ifdef STM32_USE_FREERTOS
    SemaphoreHandle_t _rxSem = nullptr;
    SemaphoreHandle_t _txSem = nullptr;
    SemaphoreHandle_t _mutex = nullptr;
#endif

public:
    Uart(USART_TypeDef &periph, IRQn_Type irqn, DmaStreamId txStreamId, const Config &cfg)
        : _periph(periph), _irqn(irqn), _txDma(txStreamId) {
        uint32_t pclk;
        if (&_periph == USART1 || &_periph == USART6) {
            pclk = getApb2Clock();
        } else {
            pclk = getApb1Clock();
        }

        reg::write(_periph.CR1, 0);
        reg::write(_periph.CR2, 0);
        reg::write(_periph.CR3, 0);

        reg::write(_periph.BRR, (pclk + cfg.baudrate / 2) / cfg.baudrate);

        uint32_t cr1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;
        if (cfg.dataBits == 9) {
            cr1 |= USART_CR1_M;
        }
        if (cfg.parity == Parity::Even) {
            cr1 |= USART_CR1_PCE;
        } else if (cfg.parity == Parity::Odd) {
            cr1 |= USART_CR1_PCE | USART_CR1_PS;
        }
        if (cfg.stopBits == 2) {
            reg::set(_periph.CR2, USART_CR2_STOP_1);
        }
        reg::write(_periph.CR1, cr1);

        reg::set(_periph.CR3, USART_CR3_DMAT);

        _txDma.configure(
            DmaConfig{
                .dir = DmaDir::MemToPeriph,
                .mode = DmaMode::Normal,
                .priority = DmaPrio::Medium,
                .memInc = true,
                .periphInc = false,
                .memSize = DmaSize::Byte,
                .periphSize = DmaSize::Byte,
            },
            &_periph.DR,
            nullptr,
            0);
        _txDma.enableInterrupts(true, false, true);

        IRQn_Type txDmaIrq = DmaStream::getIrqn(txStreamId);
        detail::nvicSetPriority(_irqn, 6);
        detail::nvicEnableIRQ(_irqn);
        detail::nvicSetPriority(txDmaIrq, 6);
        detail::nvicEnableIRQ(txDmaIrq);

#ifdef STM32_USE_FREERTOS
        _rxSem = xSemaphoreCreateBinary();
        _txSem = xSemaphoreCreateBinary();
        _mutex = xSemaphoreCreateMutex();
        configASSERT(_rxSem);
        configASSERT(_txSem);
        configASSERT(_mutex);
        xSemaphoreGive(_txSem);
#endif
    }

    ~Uart() {
        _txDma.stop();
        detail::nvicDisableIRQ(_irqn);
        detail::nvicDisableIRQ(DmaStream::getIrqn(_txDma.id()));
        reg::write(_periph.CR1, 0);
        reg::write(_periph.CR3, 0);

#ifdef STM32_USE_FREERTOS
        if (_rxSem) {
            vSemaphoreDelete(_rxSem);
        }
        if (_txSem) {
            vSemaphoreDelete(_txSem);
        }
        if (_mutex) {
            vSemaphoreDelete(_mutex);
        }
#endif
    }

    Uart(const Uart &) = delete;
    Uart &operator=(const Uart &) = delete;

    [[nodiscard]] size_t write(std::span<const uint8_t> data) {
        if (data.empty()) {
            return 0;
        }
#ifdef STM32_USE_FREERTOS
        xSemaphoreTake(_mutex, portMAX_DELAY);
#endif
        while (_txDma.isEnabled()) {
        }
        _txDma.clearAllFlags();
        _txDma.setMemoryAndCount(
            const_cast<void *>(static_cast<const void *>(data.data())), data.size());
        _txDma.start();

#ifdef STM32_USE_FREERTOS
        xSemaphoreTake(_txSem, portMAX_DELAY);
#else
        while (!_txDma.transferComplete()) {
        }
        _txDma.clearAllFlags();
#endif
        while (!reg::read(_periph.SR, USART_SR_TC)) {
        }

#ifdef STM32_USE_FREERTOS
        xSemaphoreGive(_mutex);
#endif
        return data.size();
    }

    [[nodiscard]] size_t read(std::span<uint8_t> data) {
        size_t received = 0;
        size_t total = data.size();
        while (received < total) {
            uint8_t byte;
            if (_rxBuf.pop(byte) == Status::Ok) {
                data[received++] = byte;
            } else {
#ifdef STM32_USE_FREERTOS
                if (xSemaphoreTake(_rxSem, portMAX_DELAY) != pdTRUE) {
                    break;
                }
#else
                if (received > 0) {
                    break;
                }
                while (_rxBuf.empty()) {
                }
#endif
            }
        }
        return received;
    }

    [[nodiscard]] size_t writeNonBlocking(std::span<const uint8_t> data) {
        if (data.empty() || _txDma.isEnabled()) {
            return 0;
        }
        return write(data);
    }

    [[nodiscard]] size_t readNonBlocking(std::span<uint8_t> data) {
        return _rxBuf.read(data.data(), data.size());
    }

    [[nodiscard]] size_t rxAvailable() const { return _rxBuf.size(); }
    [[nodiscard]] size_t txFree() const { return _txDma.isEnabled() ? 0 : SIZE_MAX; }

    void irqHandler() {
        const uint32_t sr = reg::get(_periph.SR);
#ifdef STM32_USE_FREERTOS
        BaseType_t woken = pdFALSE;
#endif

        if (sr & USART_SR_RXNE) {
            const uint8_t byte = static_cast<uint8_t>(reg::get(_periph.DR));
            (void) _rxBuf.push(byte);  // ISR: drop byte if RX buffer is full
#ifdef STM32_USE_FREERTOS
            BaseType_t w = pdFALSE;
            if (_rxSem) {
                xSemaphoreGiveFromISR(_rxSem, &w);
            }
            woken |= w;
#endif
        }

        if (sr & (USART_SR_ORE | USART_SR_NE | USART_SR_FE | USART_SR_PE)) {
            (void) reg::get(_periph.DR);
        }

#ifdef STM32_USE_FREERTOS
        portYIELD_FROM_ISR(woken);
#endif
    }

    void dmaTxIrqHandler() {
        const bool tc = _txDma.transferComplete();
        const bool te = _txDma.transferError();
        if (!tc && !te) {
            return;
        }
        _txDma.clearAllFlags();
#ifdef STM32_USE_FREERTOS
        if (_txSem) {
            BaseType_t w = pdFALSE;
            xSemaphoreGiveFromISR(_txSem, &w);
            portYIELD_FROM_ISR(w);
        }
#endif
    }
};

static_assert(IUart<Uart<>>, "Uart (interrupt mode) must model driver::IUart");
static_assert(IUart<Uart<256, 256, UartMode::Dma>>, "Uart (DMA mode) must model driver::IUart");

}  // namespace stm32f4
}  // namespace driver
