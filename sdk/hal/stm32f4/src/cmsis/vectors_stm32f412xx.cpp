/*
 * This file was automatically generated from the Arm assembly file.
 * Copyright (c) 2020 Liviu Ionescu.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

// The list of external handlers is from the Arm assembly startup files.

// ----------------------------------------------------------------------------

#include "cortexm/exception-handlers.h"

// ----------------------------------------------------------------------------

void __attribute__( ( weak ) ) Default_Handler( void );

// Forward declaration of the specific IRQ handlers. These are aliased
// to the Default_Handler, which is a 'forever' loop. When the application
// defines a handler (with the same name), this will automatically take
// precedence over these weak definitions

void __attribute__( ( weak, alias( "Default_Handler" ) ) ) WWDG_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) PVD_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) TAMP_STAMP_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) RTC_WKUP_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) FLASH_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) RCC_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) EXTI0_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) EXTI1_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) EXTI2_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) EXTI3_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) EXTI4_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) DMA1_Stream0_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) DMA1_Stream1_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) DMA1_Stream2_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) DMA1_Stream3_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) DMA1_Stream4_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) DMA1_Stream5_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) DMA1_Stream6_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) ADC_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) CAN1_TX_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) CAN1_RX0_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) CAN1_RX1_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) CAN1_SCE_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) EXTI9_5_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) TIM1_BRK_TIM9_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) TIM1_UP_TIM10_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) TIM1_TRG_COM_TIM11_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) TIM1_CC_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) TIM2_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) TIM3_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) TIM4_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) I2C1_EV_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) I2C1_ER_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) I2C2_EV_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) I2C2_ER_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) SPI1_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) SPI2_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) USART1_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) USART2_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) USART3_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) EXTI15_10_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) RTC_Alarm_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) OTG_FS_WKUP_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) TIM8_BRK_TIM12_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) TIM8_UP_TIM13_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) TIM8_TRG_COM_TIM14_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) TIM8_CC_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) DMA1_Stream7_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) SDIO_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) TIM5_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) SPI3_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) TIM6_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) TIM7_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) DMA2_Stream0_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) DMA2_Stream1_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) DMA2_Stream2_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) DMA2_Stream3_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) DMA2_Stream4_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) DFSDM1_FLT0_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) DFSDM1_FLT1_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) CAN2_TX_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) CAN2_RX0_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) CAN2_RX1_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) CAN2_SCE_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) OTG_FS_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) DMA2_Stream5_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) DMA2_Stream6_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) DMA2_Stream7_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) USART6_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) I2C3_EV_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) I2C3_ER_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) RNG_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) FPU_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) SPI4_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) SPI5_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) QUADSPI_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) FMPI2C1_EV_IRQHandler( void );
void __attribute__( ( weak, alias( "Default_Handler" ) ) ) FMPI2C1_ER_IRQHandler( void );

// ----------------------------------------------------------------------------

extern unsigned int _estack;

typedef void ( *const pHandler )( void );

// ----------------------------------------------------------------------------

// The table of interrupt handlers. It has an explicit section name
// and relies on the linker script to place it at the correct location
// in memory.

__attribute__( ( section( ".isr_vector" ), used ) ) pHandler __isr_vectors[] = {
    // Cortex-M Core Handlers
    (pHandler) &_estack, // The initial stack pointer
    Reset_Handler,       // The reset handler

    NMI_Handler,       // The NMI handler
    HardFault_Handler, // The hard fault handler

#if defined( __ARM_ARCH_7M__ ) || defined( __ARM_ARCH_7EM__ )
    MemManage_Handler,  // The MPU fault handler
    BusFault_Handler,   // The bus fault handler
    UsageFault_Handler, // The usage fault handler
#else
    0, // Reserved
    0, // Reserved
    0, // Reserved
#endif
    0,           // Reserved
    0,           // Reserved
    0,           // Reserved
    0,           // Reserved
    SVC_Handler, // SVCall handler
#if defined( __ARM_ARCH_7M__ ) || defined( __ARM_ARCH_7EM__ )
    DebugMon_Handler, // Debug monitor handler
#else
    0, // Reserved
#endif
    0,               // Reserved
    PendSV_Handler,  // The PendSV handler
    SysTick_Handler, // The SysTick handler

    // ----------------------------------------------------------------------
    // External Interrupts
    WWDG_IRQHandler,               // 0  - Window WatchDog
    PVD_IRQHandler,                // 1  - PVD through EXTI Line detection
    TAMP_STAMP_IRQHandler,         // 2  - Tamper and TimeStamps through the EXTI line
    RTC_WKUP_IRQHandler,           // 3  - RTC Wakeup through the EXTI line
    FLASH_IRQHandler,              // 4  - FLASH
    RCC_IRQHandler,                // 5  - RCC
    EXTI0_IRQHandler,              // 6  - EXTI Line0
    EXTI1_IRQHandler,              // 7  - EXTI Line1
    EXTI2_IRQHandler,              // 8  - EXTI Line2
    EXTI3_IRQHandler,              // 9  - EXTI Line3
    EXTI4_IRQHandler,              // 10 - EXTI Line4
    DMA1_Stream0_IRQHandler,       // 11 - DMA1 Stream 0
    DMA1_Stream1_IRQHandler,       // 12 - DMA1 Stream 1
    DMA1_Stream2_IRQHandler,       // 13 - DMA1 Stream 2
    DMA1_Stream3_IRQHandler,       // 14 - DMA1 Stream 3
    DMA1_Stream4_IRQHandler,       // 15 - DMA1 Stream 4
    DMA1_Stream5_IRQHandler,       // 16 - DMA1 Stream 5
    DMA1_Stream6_IRQHandler,       // 17 - DMA1 Stream 6
    ADC_IRQHandler,                // 18 - ADC1, ADC2 and ADC3s
    CAN1_TX_IRQHandler,            // 19 - CAN1 TX
    CAN1_RX0_IRQHandler,           // 20 - CAN1 RX0
    CAN1_RX1_IRQHandler,           // 21 - CAN1 RX1
    CAN1_SCE_IRQHandler,           // 22 - CAN1 SCE
    EXTI9_5_IRQHandler,            // 23 - External Line[9:5]s
    TIM1_BRK_TIM9_IRQHandler,     // 24 - TIM1 Break and TIM9
    TIM1_UP_TIM10_IRQHandler,     // 25 - TIM1 Update and TIM10
    TIM1_TRG_COM_TIM11_IRQHandler, // 26 - TIM1 Trigger and Commutation and TIM11
    TIM1_CC_IRQHandler,            // 27 - TIM1 Capture Compare
    TIM2_IRQHandler,               // 28 - TIM2
    TIM3_IRQHandler,               // 29 - TIM3
    TIM4_IRQHandler,               // 30 - TIM4
    I2C1_EV_IRQHandler,            // 31 - I2C1 Event
    I2C1_ER_IRQHandler,            // 32 - I2C1 Error
    I2C2_EV_IRQHandler,            // 33 - I2C2 Event
    I2C2_ER_IRQHandler,            // 34 - I2C2 Error
    SPI1_IRQHandler,               // 35 - SPI1
    SPI2_IRQHandler,               // 36 - SPI2
    USART1_IRQHandler,             // 37 - USART1
    USART2_IRQHandler,             // 38 - USART2
    USART3_IRQHandler,             // 39 - USART3
    EXTI15_10_IRQHandler,          // 40 - External Line[15:10]s
    RTC_Alarm_IRQHandler,          // 41 - RTC Alarm (A and B) through EXTI Line
    OTG_FS_WKUP_IRQHandler,        // 42 - USB OTG FS Wakeup through EXTI line
    TIM8_BRK_TIM12_IRQHandler,    // 43 - TIM8 Break and TIM12
    TIM8_UP_TIM13_IRQHandler,     // 44 - TIM8 Update and TIM13
    TIM8_TRG_COM_TIM14_IRQHandler, // 45 - TIM8 Trigger and Commutation and TIM14
    TIM8_CC_IRQHandler,            // 46 - TIM8 Capture Compare
    DMA1_Stream7_IRQHandler,       // 47 - DMA1 Stream7
    0,                             // 48 - Reserved
    SDIO_IRQHandler,               // 49 - SDIO
    TIM5_IRQHandler,               // 50 - TIM5
    SPI3_IRQHandler,               // 51 - SPI3
    0,                             // 52 - Reserved
    0,                             // 53 - Reserved
    TIM6_IRQHandler,               // 54 - TIM6
    TIM7_IRQHandler,               // 55 - TIM7
    DMA2_Stream0_IRQHandler,       // 56 - DMA2 Stream 0
    DMA2_Stream1_IRQHandler,       // 57 - DMA2 Stream 1
    DMA2_Stream2_IRQHandler,       // 58 - DMA2 Stream 2
    DMA2_Stream3_IRQHandler,       // 59 - DMA2 Stream 3
    DMA2_Stream4_IRQHandler,       // 60 - DMA2 Stream 4
    DFSDM1_FLT0_IRQHandler,       // 61 - DFSDM1 Filter 0
    DFSDM1_FLT1_IRQHandler,       // 62 - DFSDM1 Filter 1
    CAN2_TX_IRQHandler,            // 63 - CAN2 TX
    CAN2_RX0_IRQHandler,           // 64 - CAN2 RX0
    CAN2_RX1_IRQHandler,           // 65 - CAN2 RX1
    CAN2_SCE_IRQHandler,           // 66 - CAN2 SCE
    OTG_FS_IRQHandler,             // 67 - USB OTG FS
    DMA2_Stream5_IRQHandler,       // 68 - DMA2 Stream 5
    DMA2_Stream6_IRQHandler,       // 69 - DMA2 Stream 6
    DMA2_Stream7_IRQHandler,       // 70 - DMA2 Stream 7
    USART6_IRQHandler,             // 71 - USART6
    I2C3_EV_IRQHandler,            // 72 - I2C3 event
    I2C3_ER_IRQHandler,            // 73 - I2C3 error
    0,                             // 74 - Reserved
    0,                             // 75 - Reserved
    0,                             // 76 - Reserved
    0,                             // 77 - Reserved
    0,                             // 78 - Reserved
    0,                             // 79 - Reserved
    RNG_IRQHandler,                // 80 - RNG
    FPU_IRQHandler,                // 81 - FPU
    0,                             // 82 - Reserved
    0,                             // 83 - Reserved
    SPI4_IRQHandler,               // 84 - SPI4
    SPI5_IRQHandler,               // 85 - SPI5
    0,                             // 86 - Reserved
    0,                             // 87 - Reserved
    0,                             // 88 - Reserved
    0,                             // 89 - Reserved
    0,                             // 90 - Reserved
    0,                             // 91 - Reserved
    QUADSPI_IRQHandler,            // 92 - QuadSPI
    0,                             // 93 - Reserved
    0,                             // 94 - Reserved
    FMPI2C1_EV_IRQHandler,        // 95 - FMPI2C1 Event
    FMPI2C1_ER_IRQHandler,        // 96 - FMPI2C1 Error
};

// ----------------------------------------------------------------------------

// Processor ends up here if an unexpected interrupt occurs or a
// specific handler is not present in the application code.
// When in DEBUG, trigger a debug exception to clearly notify
// the user of the exception and help identify the cause.

void __attribute__( ( section( ".after_vectors" ) ) ) Default_Handler( void )
{
#if defined( DEBUG )
    __DEBUG_BKPT();
#endif
    while ( 1 )
    {
        ;
    }
}

// ----------------------------------------------------------------------------
