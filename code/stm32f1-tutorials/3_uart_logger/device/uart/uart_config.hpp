#pragma once

extern "C" {
// IWYU pragma: begin_keep
#include "stm32f1xx_hal.h"
// IWYU pragma: end_keep
}

#include <cstdint>

namespace device::uart {

enum class UartInstance : uintptr_t {
    Usart1 = USART1_BASE,
    Usart2 = USART2_BASE,
    Usart3 = USART3_BASE,
};

enum class WordLength : uint32_t {
    Bits8 = UART_WORDLENGTH_8B,
    Bits9 = UART_WORDLENGTH_9B,
};

enum class Parity : uint32_t {
    None = UART_PARITY_NONE,
    Even = UART_PARITY_EVEN,
    Odd  = UART_PARITY_ODD,
};

enum class StopBits : uint32_t {
    One = UART_STOPBITS_1,
    Two = UART_STOPBITS_2,
};

enum class Mode : uint32_t {
    Rx   = UART_MODE_RX,
    Tx   = UART_MODE_TX,
    TxRx = UART_MODE_TX_RX,
};

enum class HwFlowControl : uint32_t {
    None   = UART_HWCONTROL_NONE,
    Rts    = UART_HWCONTROL_RTS,
    Cts    = UART_HWCONTROL_CTS,
    RtsCts = UART_HWCONTROL_RTS_CTS,
};

struct UartConfig {
    uint32_t baud_rate      = 115200;
    WordLength word_length  = WordLength::Bits8;
    Parity parity           = Parity::None;
    StopBits stop_bits      = StopBits::One;
    Mode mode               = Mode::TxRx;
    HwFlowControl hw_flow   = HwFlowControl::None;
};

/// Compile-time baud rate error check, ensures error < 3%.
template <uint32_t APBClockHz, uint32_t BaudRate>
consteval bool is_baud_rate_valid() {
    uint32_t brr    = (APBClockHz + BaudRate / 2) / BaudRate;
    uint32_t actual = APBClockHz / brr;
    uint32_t error_permille =
        (actual > BaudRate) ? (actual - BaudRate) * 1000 / BaudRate
                            : (BaudRate - actual) * 1000 / BaudRate;
    return error_permille < 30;
}

} // namespace device::uart
