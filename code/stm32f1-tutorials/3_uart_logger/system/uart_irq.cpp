#include "base/circular_buffer.hpp"
#include "device/uart/uart_manager.hpp"

#include <cstddef>

namespace {

/// Shared ISR receive buffer — one byte re-used by HAL_UART_Receive_IT each time.
std::byte rx_byte{};

/// Ring buffer fed by ISR, drained by main loop.
base::CircularBuffer<128> rx_ring;

using Manager = device::uart::UartManager<device::uart::UartInstance::Usart1>;

void restart_receive() {
    [[maybe_unused]] auto r =
        Manager::driver().receive_it(std::span<std::byte, 1>{&rx_byte, 1});
}

} // namespace

/// Expose the ring buffer so main.cpp can pop from it.
base::CircularBuffer<128>& uart_rx_buffer() { return rx_ring; }

extern "C" {

void USART1_IRQHandler(void) {
    HAL_UART_IRQHandler(Manager::handle());
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart) {
    if (huart->Instance == USART1) {
        rx_ring.push(rx_byte);
        restart_receive();
    }
}

} // extern "C"

void uart_start_receive() { restart_receive(); }
