#include "motor_driver.h"

#include <driver/gpio.h>
#include <driver/ledc.h>

#include <cstdint>

namespace ruby_robot {
namespace {

void RecordFirstError(esp_err_t result, esp_err_t& first_error) {
    if (first_error == ESP_OK && result != ESP_OK)
        first_error = result;
}

}  // namespace

MotorDriver::MotorDriver(const MotorDriverConfig& config) : config_(config) {
    uint64_t pins = 0;
    for (const auto& motor : config_.motors)
        pins |= (1ULL << motor.enable) | (1ULL << motor.in1) | (1ULL << motor.in2);
    gpio_config_t gpio = {};
    gpio.pin_bit_mask = pins;
    gpio.mode = GPIO_MODE_OUTPUT;
    gpio.pull_down_en = GPIO_PULLDOWN_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&gpio));
    for (const auto& motor : config_.motors) {
        ESP_ERROR_CHECK(gpio_set_level(motor.enable, 0));
        ESP_ERROR_CHECK(gpio_set_level(motor.in1, 0));
        ESP_ERROR_CHECK(gpio_set_level(motor.in2, 0));
    }

    ledc_timer_config_t pwm = {};
    pwm.speed_mode = config_.mode;
    pwm.duty_resolution = config_.duty_resolution;
    pwm.timer_num = config_.timer;
    pwm.freq_hz = config_.freq_hz;
    pwm.clk_cfg = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&pwm));
    for (const auto& motor : config_.motors) {
        ledc_channel_config_t output = {};
        output.gpio_num = motor.enable;
        output.speed_mode = config_.mode;
        output.channel = motor.channel;
        output.timer_sel = config_.timer;
        output.duty = 0;
        ESP_ERROR_CHECK(ledc_channel_config(&output));
    }
}

const MotorConfig* MotorDriver::Pins(Wheel wheel) const {
    const auto index = static_cast<unsigned>(wheel);
    return index < config_.motors.size() ? &config_.motors[index] : nullptr;
}

esp_err_t MotorDriver::Stop() {
    esp_err_t error = ESP_OK;
    for (const auto& motor : config_.motors)
        RecordFirstError(ledc_stop(config_.mode, motor.channel, 0), error);
    for (const auto& motor : config_.motors) {
        RecordFirstError(gpio_set_level(motor.in1, 0), error);
        RecordFirstError(gpio_set_level(motor.in2, 0), error);
    }
    return error;
}

esp_err_t MotorDriver::SetPwm(Wheel wheel, int percent) {
    const auto* motor = Pins(wheel);
    if (motor == nullptr || percent < -100 || percent > 100)
        return ESP_ERR_INVALID_ARG;
    if (percent == 0)
        return ledc_stop(config_.mode, motor->channel, 0);
    const uint32_t max_duty = (uint32_t{1} << config_.duty_resolution) - 1;
    const uint32_t duty = static_cast<uint32_t>(percent < 0 ? -percent : percent) * max_duty / 100;
    const esp_err_t error = ledc_set_duty(config_.mode, motor->channel, duty);
    return error == ESP_OK ? ledc_update_duty(config_.mode, motor->channel) : error;
}

esp_err_t MotorDriver::SetDirection(Wheel wheel, int sign) {
    const auto* motor = Pins(wheel);
    if (motor == nullptr)
        return ESP_ERR_INVALID_ARG;
    const bool forward = (sign > 0) != motor->inverted;
    const esp_err_t first = gpio_set_level(motor->in1, sign == 0 ? 0 : forward);
    const esp_err_t second = gpio_set_level(motor->in2, sign == 0 ? 0 : !forward);
    return first != ESP_OK ? first : second;
}

}  // namespace ruby_robot
