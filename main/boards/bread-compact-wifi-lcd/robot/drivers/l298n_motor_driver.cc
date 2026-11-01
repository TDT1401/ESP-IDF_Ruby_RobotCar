#include "l298n_motor_driver.h"

#include <driver/gpio.h>
#include <driver/ledc.h>

#include <cstdint>
#include <initializer_list>

#include "../../config.h"

namespace ruby_robot {
namespace {

constexpr auto kMode = LEDC_LOW_SPEED_MODE;
constexpr auto kTimer = LEDC_TIMER_1;  // LCD backlight owns timer/channel 0.
constexpr auto kLeftChannel = LEDC_CHANNEL_1;
constexpr auto kRightChannel = LEDC_CHANNEL_2;

ledc_channel_t Channel(Wheel wheel) {
    return wheel == Wheel::kLeft ? kLeftChannel : kRightChannel;
}

gpio_num_t In1(Wheel wheel) {
    return wheel == Wheel::kLeft ? MOTOR_LEFT_IN1_PIN : MOTOR_RIGHT_IN1_PIN;
}

gpio_num_t In2(Wheel wheel) {
    return wheel == Wheel::kLeft ? MOTOR_LEFT_IN2_PIN : MOTOR_RIGHT_IN2_PIN;
}

void RecordFirstError(esp_err_t result, esp_err_t& first_error) {
    if (first_error == ESP_OK && result != ESP_OK)
        first_error = result;
}

}  // namespace

L298nMotorDriver::L298nMotorDriver() {
    constexpr uint64_t pins = (1ULL << MOTOR_LEFT_EN_PIN) | (1ULL << MOTOR_LEFT_IN1_PIN) |
                              (1ULL << MOTOR_LEFT_IN2_PIN) | (1ULL << MOTOR_RIGHT_EN_PIN) |
                              (1ULL << MOTOR_RIGHT_IN1_PIN) | (1ULL << MOTOR_RIGHT_IN2_PIN);
    gpio_config_t gpio = {};
    gpio.pin_bit_mask = pins;
    gpio.mode = GPIO_MODE_OUTPUT;
    gpio.pull_down_en = GPIO_PULLDOWN_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&gpio));
    for (auto pin : {MOTOR_LEFT_EN_PIN, MOTOR_LEFT_IN1_PIN, MOTOR_LEFT_IN2_PIN, MOTOR_RIGHT_EN_PIN,
                     MOTOR_RIGHT_IN1_PIN, MOTOR_RIGHT_IN2_PIN}) {
        ESP_ERROR_CHECK(gpio_set_level(pin, 0));
    }

    ledc_timer_config_t pwm = {};
    pwm.speed_mode = kMode;
    pwm.duty_resolution = LEDC_TIMER_10_BIT;
    pwm.timer_num = kTimer;
    pwm.freq_hz = 1000;
    pwm.clk_cfg = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&pwm));
    for (auto channel : {kLeftChannel, kRightChannel}) {
        ledc_channel_config_t output = {};
        output.gpio_num = channel == kLeftChannel ? MOTOR_LEFT_EN_PIN : MOTOR_RIGHT_EN_PIN;
        output.speed_mode = kMode;
        output.channel = channel;
        output.timer_sel = kTimer;
        output.duty = 0;
        ESP_ERROR_CHECK(ledc_channel_config(&output));
    }
}

esp_err_t L298nMotorDriver::Stop() {
    esp_err_t error = ESP_OK;
    RecordFirstError(ledc_stop(kMode, kLeftChannel, 0), error);
    RecordFirstError(ledc_stop(kMode, kRightChannel, 0), error);
    for (auto pin :
         {MOTOR_LEFT_IN1_PIN, MOTOR_LEFT_IN2_PIN, MOTOR_RIGHT_IN1_PIN, MOTOR_RIGHT_IN2_PIN}) {
        RecordFirstError(gpio_set_level(pin, 0), error);
    }
    return error;
}

esp_err_t L298nMotorDriver::SetPwm(Wheel wheel, int percent) {
    const auto channel = Channel(wheel);
    if (percent == 0)
        return ledc_stop(kMode, channel, 0);
    const uint32_t duty = static_cast<uint32_t>(percent < 0 ? -percent : percent) * 1023 / 100;
    const esp_err_t error = ledc_set_duty(kMode, channel, duty);
    return error == ESP_OK ? ledc_update_duty(kMode, channel) : error;
}

esp_err_t L298nMotorDriver::SetDirection(Wheel wheel, int sign, bool inverted) {
    const bool forward = (sign > 0) != inverted;
    const int in1_level = sign == 0 ? 0 : forward;
    const int in2_level = sign == 0 ? 0 : !forward;
    const esp_err_t first = gpio_set_level(In1(wheel), in1_level);
    const esp_err_t second = gpio_set_level(In2(wheel), in2_level);
    return first != ESP_OK ? first : second;
}

}  // namespace ruby_robot
