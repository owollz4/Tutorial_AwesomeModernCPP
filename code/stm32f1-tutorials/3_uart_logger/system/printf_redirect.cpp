#include "device/uart/uart_manager.hpp"

extern "C" {

int _write(int fd [[maybe_unused]], char* ptr, int len) {
    auto* huart = device::uart::UartManager<device::uart::UartInstance::Usart1>::handle();
    HAL_UART_Transmit(huart, reinterpret_cast<uint8_t*>(ptr), len, HAL_MAX_DELAY);
    return len;
}

} // extern "C"
