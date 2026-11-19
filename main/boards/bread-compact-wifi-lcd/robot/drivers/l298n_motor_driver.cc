#include "l298n_motor_driver.h"

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <sdkconfig.h>

#include <cstdint>
#include <iterator>

#include "../../config.h"

namespace ruby_robot {
namespace {

constexpr auto kMode = LEDC_LOW_SPEED_MODE;
constexpr auto kTimer = LEDC_TIMER_1;
struct MotorPins {
    gpio_num_t enable;
    gpio_num_t in1;
    gpio_num_t in2;
    ledc_channel_t channel;
};

// Channel 0 belongs to the backlight; channel 3 belongs to the servo.
constexpr MotorPins kMotors[] = {
    {MOTOR_LEFT_EN_PIN, MOTOR_LEFT_IN1_PIN, MOTOR_LEFT_IN2_PIN, LEDC_CHANNEL_1},
    {MOTOR_RIGHT_EN_PIN, MOTOR_RIGHT_IN1_PIN, MOTOR_RIGHT_IN2_PIN, LEDC_CHANNEL_2},
    {MOTOR_REAR_LEFT_EN_PIN, MOTOR_REAR_LEFT_IN1_PIN, MOTOR_REAR_LEFT_IN2_PIN, LEDC_CHANNEL_4},
    {MOTOR_REAR_RIGHT_EN_PIN, MOTOR_REAR_RIGHT_IN1_PIN, MOTOR_REAR_RIGHT_IN2_PIN, LEDC_CHANNEL_5},
};

const MotorPins* Pins(Wheel wheel) {
    const auto index = static_cast<unsigned>(wheel);
    return index < std::size(kMotors) ? &kMotors[index] : nullptr;
}

bool Inverted(Wheel wheel) {
    switch (wheel) {
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_L298N_INVERT_LEFT
        case Wheel::kLeft:
            return true;
#endif
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_L298N_INVERT_RIGHT
        case Wheel::kRight:
            return true;
#endif
        default:
            return false;
    }
}

void RecordFirstError(esp_err_t result, esp_err_t& first_error) {
    if (first_error == ESP_OK && result != ESP_OK)
        first_error = result;
}

}  // namespace

L298nMotorDriver::L298nMotorDriver() {
    uint64_t pins = 0;
    for (const auto& motor : kMotors)
        pins |= (1ULL << motor.enable) | (1ULL << motor.in1) | (1ULL << motor.in2);
    gpio_config_t gpio = {};
    gpio.pin_bit_mask = pins;
    gpio.mode = GPIO_MODE_OUTPUT;
    gpio.pull_down_en = GPIO_PULLDOWN_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&gpio));
    for (const auto& motor : kMotors) {
        ESP_ERROR_CHECK(gpio_set_level(motor.enable, 0));
        ESP_ERROR_CHECK(gpio_set_level(motor.in1, 0));
        ESP_ERROR_CHECK(gpio_set_level(motor.in2, 0));
    }

    ledc_timer_config_t pwm = {};
    pwm.speed_mode = kMode;
    pwm.duty_resolution = LEDC_TIMER_10_BIT;
    pwm.timer_num = kTimer;
    pwm.freq_hz = 1000;
    pwm.clk_cfg = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&pwm));
    for (const auto& motor : kMotors) {
        ledc_channel_config_t output = {};
        output.gpio_num = motor.enable;
        output.speed_mode = kMode;
        output.channel = motor.channel;
        output.timer_sel = kTimer;
        output.duty = 0;
        ESP_ERROR_CHECK(ledc_channel_config(&output));
    }
}

esp_err_t L298nMotorDriver::Stop() {
    esp_err_t error = ESP_OK;
    for (const auto& motor : kMotors)
        RecordFirstError(ledc_stop(kMode, motor.channel, 0), error);
    for (const auto& motor : kMotors) {
        RecordFirstError(gpio_set_level(motor.in1, 0), error);
        RecordFirstError(gpio_set_level(motor.in2, 0), error);
    }
    return error;
}

esp_err_t L298nMotorDriver::SetPwm(Wheel wheel, int percent) {
    const auto* motor = Pins(wheel);
    if (motor == nullptr || percent < -100 || percent > 100)
        return ESP_ERR_INVALID_ARG;
    if (percent == 0)
        return ledc_stop(kMode, motor->channel, 0);
    const uint32_t duty = static_cast<uint32_t>(percent < 0 ? -percent : percent) * 1023 / 100;
    const esp_err_t error = ledc_set_duty(kMode, motor->channel, duty);
    return error == ESP_OK ? ledc_update_duty(kMode, motor->channel) : error;
}

esp_err_t L298nMotorDriver::SetDirection(Wheel wheel, int sign) {
    const auto* motor = Pins(wheel);
    if (motor == nullptr)
        return ESP_ERR_INVALID_ARG;
    const bool forward = (sign > 0) != Inverted(wheel);
    const esp_err_t first = gpio_set_level(motor->in1, sign == 0 ? 0 : forward);
    const esp_err_t second = gpio_set_level(motor->in2, sign == 0 ? 0 : !forward);
    return first != ESP_OK ? first : second;
}

}  // namespace ruby_robot
