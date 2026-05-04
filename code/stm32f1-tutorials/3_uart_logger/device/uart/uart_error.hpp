#pragma once

namespace device::uart {

enum class UartError {
    Timeout,
    NotInitialized,
    HardwareFault,
    Busy,
};

} // namespace device::uart
