module;
#include <cstddef>
#include <cstdint>
#include "cmsis/stm32f4xx.h"
export module driver.stm32f4.dma;

import driver.reg;

export namespace driver {
namespace stm32f4 {

enum class DmaDir : uint8_t {
  PeriphToMem = 0,
  MemToPeriph = 1,
  MemToMem = 2,
};

enum class DmaMode : uint8_t {
  Normal = 0,
  Circular = 1,
};

enum class DmaPrio : uint8_t {
  Low = 0,
  Medium = 1,
  High = 2,
  VeryHigh = 3,
};

enum class DmaSize : uint8_t {
  Byte = 0,
  HalfWord = 1,
  Word = 2,
};

struct DmaStreamId {
  uintptr_t dmaBase;
  uint8_t streamIndex;
  uint8_t channel;
};

struct DmaConfig {
  DmaDir dir;
  DmaMode mode;
  DmaPrio priority;
  bool memInc;
  bool periphInc;
  DmaSize memSize;
  DmaSize periphSize;
};

namespace detail {

constexpr uint32_t streamFlagShift(uint8_t idx) {
  const uint8_t local = idx & 0x3U;
  return static_cast<uint32_t>(local) * 6U + (local >= 2 ? 4U : 0U);
}

constexpr uint32_t tcifMask(uint8_t idx) {
  return 1U << (streamFlagShift(idx) + 5U);
}

constexpr uint32_t htifMask(uint8_t idx) {
  return 1U << (streamFlagShift(idx) + 4U);
}

constexpr uint32_t teifMask(uint8_t idx) {
  return 1U << (streamFlagShift(idx) + 3U);
}

constexpr uint32_t allFlagsMask(uint8_t idx) {
  return 0x3DU << streamFlagShift(idx);
}

inline DMA_TypeDef *dmaRegs(uintptr_t base) {
  return reinterpret_cast<DMA_TypeDef *>(base);
}

inline DMA_Stream_TypeDef *streamRegs(uintptr_t dmaBase, uint8_t idx) {
  constexpr uintptr_t kFirstStreamOffset = 0x10U;
  constexpr uintptr_t kStreamStride = 0x18U;
  return reinterpret_cast<DMA_Stream_TypeDef *>(
      dmaBase + kFirstStreamOffset + static_cast<uintptr_t>(idx) * kStreamStride
  );
}

inline volatile uint32_t &flagsReg(DMA_TypeDef *dma, uint8_t idx) {
  return idx < 4 ? dma->LISR : dma->HISR;
}

inline volatile uint32_t &clearReg(DMA_TypeDef *dma, uint8_t idx) {
  return idx < 4 ? dma->LIFCR : dma->HIFCR;
}

}  // namespace detail

class DmaStream {
  DmaStreamId _id;
  DMA_TypeDef *_dma;
  DMA_Stream_TypeDef *_stream;

public:
  explicit DmaStream(DmaStreamId id)
      : _id(id),
        _dma(detail::dmaRegs(id.dmaBase)),
        _stream(detail::streamRegs(id.dmaBase, id.streamIndex)) {}

  DmaStream(const DmaStream &) = delete;
  DmaStream &operator=(const DmaStream &) = delete;

  void configure(
      const DmaConfig &cfg,
      volatile void *periphAddr,
      void *memAddr,
      size_t count
  ) {
    reg::clear(_stream->CR, DMA_SxCR_EN);
    while (reg::read(_stream->CR, DMA_SxCR_EN)) {
    }
    clearAllFlags();

    reg::write(_stream->PAR, reinterpret_cast<uint32_t>(periphAddr));
    reg::write(_stream->M0AR, reinterpret_cast<uint32_t>(memAddr));
    reg::write(_stream->NDTR, static_cast<uint32_t>(count));
    reg::write(_stream->FCR, 0);

    uint32_t cr = 0;
    cr |= (static_cast<uint32_t>(_id.channel) & 0x7U) << DMA_SxCR_CHSEL_Pos;
    cr |= (static_cast<uint32_t>(cfg.priority) & 0x3U) << DMA_SxCR_PL_Pos;
    cr |= (static_cast<uint32_t>(cfg.memSize) & 0x3U) << DMA_SxCR_MSIZE_Pos;
    cr |= (static_cast<uint32_t>(cfg.periphSize) & 0x3U) << DMA_SxCR_PSIZE_Pos;
    cr |= (static_cast<uint32_t>(cfg.dir) & 0x3U) << DMA_SxCR_DIR_Pos;
    if (cfg.memInc) {
      cr |= DMA_SxCR_MINC;
    }
    if (cfg.periphInc) {
      cr |= DMA_SxCR_PINC;
    }
    if (cfg.mode == DmaMode::Circular) {
      cr |= DMA_SxCR_CIRC;
    }
    reg::write(_stream->CR, cr);
  }

  void enableInterrupts(
      bool transferComplete,
      bool halfTransfer,
      bool transferError
  ) {
    uint32_t mask = 0;
    if (transferComplete) {
      mask |= DMA_SxCR_TCIE;
    }
    if (halfTransfer) {
      mask |= DMA_SxCR_HTIE;
    }
    if (transferError) {
      mask |= DMA_SxCR_TEIE;
    }
    reg::set(_stream->CR, mask);
  }

  void setMemoryAndCount(void *memAddr, size_t count) {
    reg::write(_stream->M0AR, reinterpret_cast<uint32_t>(memAddr));
    reg::write(_stream->NDTR, static_cast<uint32_t>(count));
  }

  void start() { reg::set(_stream->CR, DMA_SxCR_EN); }

  void stop() {
    reg::clear(_stream->CR, DMA_SxCR_EN);
    while (reg::read(_stream->CR, DMA_SxCR_EN)) {
    }
  }

  bool isEnabled() const { return reg::read(_stream->CR, DMA_SxCR_EN); }

  size_t remaining() const {
    return static_cast<size_t>(reg::get(_stream->NDTR));
  }

  bool transferComplete() const {
    return reg::read(
        detail::flagsReg(_dma, _id.streamIndex),
        detail::tcifMask(_id.streamIndex)
    );
  }

  bool halfTransfer() const {
    return reg::read(
        detail::flagsReg(_dma, _id.streamIndex),
        detail::htifMask(_id.streamIndex)
    );
  }

  bool transferError() const {
    return reg::read(
        detail::flagsReg(_dma, _id.streamIndex),
        detail::teifMask(_id.streamIndex)
    );
  }

  void clearTransferComplete() {
    reg::write(
        detail::clearReg(_dma, _id.streamIndex),
        detail::tcifMask(_id.streamIndex)
    );
  }

  void clearHalfTransfer() {
    reg::write(
        detail::clearReg(_dma, _id.streamIndex),
        detail::htifMask(_id.streamIndex)
    );
  }

  void clearTransferError() {
    reg::write(
        detail::clearReg(_dma, _id.streamIndex),
        detail::teifMask(_id.streamIndex)
    );
  }

  void clearAllFlags() {
    reg::write(
        detail::clearReg(_dma, _id.streamIndex),
        detail::allFlagsMask(_id.streamIndex)
    );
  }

  DmaStreamId id() const { return _id; }

  static IRQn_Type getIrqn(DmaStreamId id) {
    if (id.dmaBase == DMA1_BASE) {
      switch (id.streamIndex) {
        case 0:
          return DMA1_Stream0_IRQn;
        case 1:
          return DMA1_Stream1_IRQn;
        case 2:
          return DMA1_Stream2_IRQn;
        case 3:
          return DMA1_Stream3_IRQn;
        case 4:
          return DMA1_Stream4_IRQn;
        case 5:
          return DMA1_Stream5_IRQn;
        case 6:
          return DMA1_Stream6_IRQn;
        case 7:
          return DMA1_Stream7_IRQn;
        default:
          break;
      }
    } else if (id.dmaBase == DMA2_BASE) {
      switch (id.streamIndex) {
        case 0:
          return DMA2_Stream0_IRQn;
        case 1:
          return DMA2_Stream1_IRQn;
        case 2:
          return DMA2_Stream2_IRQn;
        case 3:
          return DMA2_Stream3_IRQn;
        case 4:
          return DMA2_Stream4_IRQn;
        case 5:
          return DMA2_Stream5_IRQn;
        case 6:
          return DMA2_Stream6_IRQn;
        case 7:
          return DMA2_Stream7_IRQn;
        default:
          break;
      }
    }
    return DMA1_Stream0_IRQn;
  }
};

namespace dmaMap {

constexpr DmaStreamId usart1_tx{DMA2_BASE, 7, 4};
constexpr DmaStreamId usart1_rx{DMA2_BASE, 5, 4};
constexpr DmaStreamId usart2_tx{DMA1_BASE, 6, 4};
constexpr DmaStreamId usart2_rx{DMA1_BASE, 5, 4};
constexpr DmaStreamId usart3_tx{DMA1_BASE, 3, 4};
constexpr DmaStreamId usart3_rx{DMA1_BASE, 1, 4};
constexpr DmaStreamId uart4_tx{DMA1_BASE, 4, 4};
constexpr DmaStreamId uart4_rx{DMA1_BASE, 2, 4};
constexpr DmaStreamId uart5_tx{DMA1_BASE, 7, 4};
constexpr DmaStreamId uart5_rx{DMA1_BASE, 0, 4};
constexpr DmaStreamId usart6_tx{DMA2_BASE, 6, 5};
constexpr DmaStreamId usart6_rx{DMA2_BASE, 1, 5};

constexpr DmaStreamId spi1_tx{DMA2_BASE, 3, 3};
constexpr DmaStreamId spi1_rx{DMA2_BASE, 0, 3};
constexpr DmaStreamId spi2_tx{DMA1_BASE, 4, 0};
constexpr DmaStreamId spi2_rx{DMA1_BASE, 3, 0};
constexpr DmaStreamId spi3_tx{DMA1_BASE, 5, 0};
constexpr DmaStreamId spi3_rx{DMA1_BASE, 0, 0};

}  // namespace dmaMap

}  // namespace stm32f4
}  // namespace driver
