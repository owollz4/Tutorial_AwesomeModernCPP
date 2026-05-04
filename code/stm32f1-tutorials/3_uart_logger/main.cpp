#include "base/circular_buffer.hpp"
#include "device/button.hpp"
#include "device/button_event.hpp"
#include "device/led.hpp"
#include "device/uart/uart_manager.hpp"
#include "system/clock.h"

extern "C" {
#include "stm32f1xx_hal.h"
}

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>

// Provided by system/uart_irq.cpp
extern base::CircularBuffer<128>& uart_rx_buffer();
extern void uart_start_receive();

using Logger = device::uart::UartManager<device::uart::UartInstance::Usart1>;

/// USART1 on Blue Pill: PA9 (TX) AF Push-Pull, PA10 (RX) Input Pull-Up.
static void usart1_gpio_init() noexcept {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef gpio{};
    gpio.Pin = GPIO_PIN_9;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = GPIO_PIN_10;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &gpio);
}

/// Process a completed line received from UART.
static void handle_command(std::string_view cmd,
                           device::LED<device::gpio::GpioPort::C, GPIO_PIN_13>& led) {
    if (cmd == "LED ON") {
        led.on();
        Logger::driver().send_string("OK: LED ON\r\n");
    } else if (cmd == "LED OFF") {
        led.off();
        Logger::driver().send_string("OK: LED OFF\r\n");
    } else if (cmd == "HELP") {
        Logger::driver().send_string("Commands: LED ON, LED OFF, HELP\r\n");
    } else if (!cmd.empty()) {
        Logger::driver().send_string("ERR: unknown command\r\n");
    }
}

int main() {
    HAL_Init();
    clock::ClockConfig::instance().setup_system_clock();

    device::LED<device::gpio::GpioPort::C, GPIO_PIN_13> led;
    device::Button<device::gpio::GpioPort::A, GPIO_PIN_0> button;

    Logger::driver().set_gpio_init(usart1_gpio_init);
    Logger::driver().init(device::uart::UartConfig{.baud_rate = 115200});
    Logger::driver().enable_interrupt();
    Logger::driver().send_string("UART Logger Ready!\r\n");

    uart_start_receive();

    std::array<char, 128> line_buf{};
    size_t line_len = 0;

    while (1) {
        // ── Button events → UART log ──────────────────────
        button.poll_events(
            [&](device::ButtonEvent event) {
                std::visit(
                    [&](auto&& e) {
                        using T = std::decay_t<decltype(e)>;
                        if constexpr (std::is_same_v<T, device::Pressed>) {
                            led.on();
                            Logger::driver().send_string("Button pressed!\r\n");
                        } else {
                            led.off();
                            Logger::driver().send_string("Button released!\r\n");
                        }
                    },
                    event);
            },
            HAL_GetTick());

        // ── UART receive → command parser ─────────────────
        auto& rx = uart_rx_buffer();
        std::byte b{};
        while (rx.pop(b)) {
            char c = static_cast<char>(b);
            if (c == '\r' || c == '\n') {
                if (line_len > 0) {
                    handle_command({line_buf.data(), line_len}, led);
                    line_len = 0;
                }
            } else if (line_len < line_buf.size() - 1) {
                line_buf[line_len++] = c;
            }
        }
    }
}
