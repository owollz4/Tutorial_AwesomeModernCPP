#pragma once

#include "uart_driver.hpp"

namespace device::uart {

/// Manages the lifecycle of a UART driver instance and exposes the native HAL
/// handle for scenarios that require C linkage (printf redirect, IRQ handlers).
/// Replaces the traditional `extern UART_HandleTypeDef huart1` pattern.
template <UartInstance INSTANCE>
class UartManager {
  public:
    using Driver = UartDriver<INSTANCE>;

    static auto driver() -> Driver& {
        static Driver drv;
        return drv;
    }

    /// Access the underlying HAL handle without extern globals.
    static auto handle() -> UART_HandleTypeDef* { return Driver::native_handle(); }

    UartManager()                        = delete;
    UartManager(const UartManager&)      = delete;
    UartManager(UartManager&&)           = delete;
    UartManager& operator=(const UartManager&) = delete;
    UartManager& operator=(UartManager&&)      = delete;
};

} // namespace device::uart
