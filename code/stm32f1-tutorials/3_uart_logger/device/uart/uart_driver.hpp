#pragma once

#include "uart_config.hpp"
#include "uart_error.hpp"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

namespace device::uart {

/// Concept: a callable that sets up TX/RX GPIO pins for the UART peripheral.
template <typename F> concept UartGpioInitializer =
    std::invocable<F> && std::is_nothrow_invocable_v<F>;

/// Zero-size UART driver template. All state is stored as static inline members
/// per UartInstance, so sizeof(UartDriver<X>) == 1 (empty class).
/// Each USART peripheral gets exactly one handle in BSS — no per-object overhead.
template <UartInstance INSTANCE> class UartDriver {
  public:
    void init(const UartConfig& config) {
        enable_clock();
        if (gpio_init_) {
            gpio_init_();
        }
        huart_.Instance = native_instance();
        huart_.Init.BaudRate = config.baud_rate;
        huart_.Init.WordLength = static_cast<uint32_t>(config.word_length);
        huart_.Init.StopBits = static_cast<uint32_t>(config.stop_bits);
        huart_.Init.Parity = static_cast<uint32_t>(config.parity);
        huart_.Init.Mode = static_cast<uint32_t>(config.mode);
        huart_.Init.HwFlowCtl = static_cast<uint32_t>(config.hw_flow);
        huart_.Init.OverSampling = UART_OVERSAMPLING_16;
        HAL_UART_Init(&huart_);
    }

    /// Register a GPIO init callback. Must be called before init().
    /// F must be nothrow-invocable with no arguments (typically a stateless lambda).
    template <UartGpioInitializer F> static void set_gpio_init(F fn) noexcept { gpio_init_ = fn; }

    void deinit() { HAL_UART_DeInit(&huart_); }

    // ── Blocking API ──────────────────────────────────────

    auto send(std::span<const std::byte> data, uint32_t timeout_ms)
        -> std::expected<size_t, UartError> {
        auto* ptr = reinterpret_cast<const uint8_t*>(data.data());
        HAL_StatusTypeDef result = HAL_UART_Transmit(&huart_, ptr, data.size(), timeout_ms);
        if (result == HAL_OK)
            return data.size();
        if (result == HAL_TIMEOUT)
            return std::unexpected(UartError::Timeout);
        return std::unexpected(UartError::HardwareFault);
    }

    auto receive(std::span<std::byte> buffer, uint32_t timeout_ms)
        -> std::expected<size_t, UartError> {
        auto* ptr = reinterpret_cast<uint8_t*>(buffer.data());
        HAL_StatusTypeDef result = HAL_UART_Receive(&huart_, ptr, buffer.size(), timeout_ms);
        if (result == HAL_OK)
            return buffer.size();
        if (result == HAL_TIMEOUT)
            return std::unexpected(UartError::Timeout);
        return std::unexpected(UartError::HardwareFault);
    }

    // ── Convenience API ───────────────────────────────────

    void send_string(std::string_view str) {
        auto bytes = std::as_bytes(std::span<const char>{str});
        [[maybe_unused]] auto result = send(bytes, HAL_MAX_DELAY);
    }

    // ── Interrupt API ─────────────────────────────────────

    auto send_it(std::span<const std::byte> data) -> std::expected<void, UartError> {
        auto* ptr = reinterpret_cast<const uint8_t*>(data.data());
        HAL_StatusTypeDef result = HAL_UART_Transmit_IT(&huart_, ptr, data.size());
        if (result == HAL_OK)
            return {};
        if (result == HAL_BUSY)
            return std::unexpected(UartError::Busy);
        return std::unexpected(UartError::HardwareFault);
    }

    auto receive_it(std::span<std::byte> buffer) -> std::expected<void, UartError> {
        auto* ptr = reinterpret_cast<uint8_t*>(buffer.data());
        HAL_StatusTypeDef result = HAL_UART_Receive_IT(&huart_, ptr, buffer.size());
        if (result == HAL_OK)
            return {};
        if (result == HAL_BUSY)
            return std::unexpected(UartError::Busy);
        return std::unexpected(UartError::HardwareFault);
    }

    void enable_interrupt() {
        if constexpr (INSTANCE == UartInstance::Usart1) {
            HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
            HAL_NVIC_EnableIRQ(USART1_IRQn);
        } else if constexpr (INSTANCE == UartInstance::Usart2) {
            HAL_NVIC_SetPriority(USART2_IRQn, 0, 0);
            HAL_NVIC_EnableIRQ(USART2_IRQn);
        } else if constexpr (INSTANCE == UartInstance::Usart3) {
            HAL_NVIC_SetPriority(USART3_IRQn, 0, 0);
            HAL_NVIC_EnableIRQ(USART3_IRQn);
        }
    }

    // ── Callback registration ─────────────────────────────

    using RxCallback = void (*)(std::span<const std::byte>);
    using TxCallback = void (*)();

    void set_rx_callback(RxCallback cb) { rx_callback_ = cb; }
    void set_tx_callback(TxCallback cb) { tx_callback_ = cb; }

    /// Called from HAL UART callback — dispatches to user-registered callback.
    void on_rx_complete(std::span<const std::byte> data) {
        if (rx_callback_) {
            rx_callback_(data);
        }
    }

    void on_tx_complete() {
        if (tx_callback_) {
            tx_callback_();
        }
    }

    // ── Manager access ────────────────────────────────────

    static auto native_handle() -> UART_HandleTypeDef* { return &huart_; }

  private:
    static constexpr USART_TypeDef* native_instance() noexcept {
        return reinterpret_cast<USART_TypeDef*>(static_cast<uintptr_t>(INSTANCE));
    }

    static inline void enable_clock() {
        if constexpr (INSTANCE == UartInstance::Usart1) {
            __HAL_RCC_USART1_CLK_ENABLE();
        } else if constexpr (INSTANCE == UartInstance::Usart2) {
            __HAL_RCC_USART2_CLK_ENABLE();
        } else if constexpr (INSTANCE == UartInstance::Usart3) {
            __HAL_RCC_USART3_CLK_ENABLE();
        }
    }

    // Static storage: one copy per INSTANCE in BSS. Object itself is zero-size.
    static inline UART_HandleTypeDef huart_{};
    static inline void (*gpio_init_)() = nullptr;
    static inline RxCallback rx_callback_ = nullptr;
    static inline TxCallback tx_callback_ = nullptr;
};

} // namespace device::uart
