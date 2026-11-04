#include "mpu6050_imu.h"

#include <driver/i2c_master.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "../../config.h"

namespace {

constexpr char kTag[] = "MPU6050";
#define IMU_LOGI(...)                    \
    do {                                 \
        if constexpr (MPU6050_DEBUG)     \
            ESP_LOGI(kTag, __VA_ARGS__); \
    } while (false)
#define IMU_LOGW(...)                    \
    do {                                 \
        if constexpr (MPU6050_DEBUG)     \
            ESP_LOGW(kTag, __VA_ARGS__); \
    } while (false)
#define IMU_LOGE(...)                    \
    do {                                 \
        if constexpr (MPU6050_DEBUG)     \
            ESP_LOGE(kTag, __VA_ARGS__); \
    } while (false)
constexpr uint8_t kWhoAmI = 0x75;
constexpr uint8_t kMpu6050Identity = 0x68;
constexpr uint8_t kMpu6500Identity = 0x70;
constexpr uint8_t kPowerManagement = 0x6B;
constexpr uint8_t kSampleRateDivider = 0x19;
constexpr uint8_t kLowPassConfig = 0x1A;
constexpr uint8_t kGyroConfig = 0x1B;
constexpr uint8_t kGyroZHigh = 0x47;
constexpr int kCalibrationSamples = 200;
constexpr int kSamplePeriodMs = 10;
constexpr int64_t kReadHealthLogIntervalUs = 5000000;
constexpr int64_t kHeadingLogIntervalUs = 1000000;
constexpr int64_t kMaximumIntegrationGapUs = 100000;
constexpr int64_t kStationaryDelayUs = 2000000;
constexpr double kStationaryRateDps = 0.3;
constexpr double kBiasTimeConstantSeconds = 30.0;
constexpr int kPersistentReadErrorSamples = 5;
constexpr double kGyroScale = 131.0;  // LSB/(degree/s) at +/-250 degrees/s.
static_assert(MPU6050_SDA_PIN != MPU6050_SCL_PIN);
static_assert(MPU6050_SDA_PIN != LAMP_GPIO && MPU6050_SCL_PIN != LAMP_GPIO);

i2c_master_bus_handle_t bus = nullptr;
i2c_master_dev_handle_t device = nullptr;
TaskHandle_t task = nullptr;
portMUX_TYPE status_lock = portMUX_INITIALIZER_UNLOCKED;
Mpu6050Status status;
std::atomic<bool> motor_active{false};

esp_err_t ReadRegister(uint8_t reg, uint8_t* data, size_t size) {
    return i2c_master_transmit_receive(device, &reg, 1, data, size, 20);
}

esp_err_t WriteRegister(uint8_t reg, uint8_t value) {
    const uint8_t data[] = {reg, value};
    return i2c_master_transmit(device, data, sizeof(data), 20);
}

void SampleTask(void*) {
    vTaskDelay(pdMS_TO_TICKS(100));  // Let the sensor settle after leaving sleep mode.
    IMU_LOGI("Gyro Z calibration started; keep the car still for about 2 seconds");
    TickType_t last_wake = xTaskGetTickCount();
    double calibration_sum = 0;
    double calibration_squares = 0;
    int calibration_count = 0;
    double bias = 0;
    double heading = 0;
    double previous_rate = 0;
    bool calibrated = false;
    bool heading_valid = false;
    int64_t previous_sample_us = 0;
    int64_t stationary_since_us = 0;
    int consecutive_errors = 0;
    int64_t last_heading_log_us = 0;
    int64_t last_read_health_log_us = esp_timer_get_time();
    int read_attempts = 0;
    int read_errors = 0;
    esp_err_t last_read_error = ESP_OK;
    bool reported_motor_active = false;
    auto report_read_health = [&](int64_t now) {
        if (now - last_read_health_log_us < kReadHealthLogIntervalUs)
            return;
        if (read_errors > 0) {
            IMU_LOGW("Gyro Z I2C read errors: %d/%d samples in 5 s (last: %s)", read_errors,
                     read_attempts, esp_err_to_name(last_read_error));
        }
        read_attempts = 0;
        read_errors = 0;
        last_read_health_log_us = now;
    };
    while (true) {
        uint8_t data[2] = {};
        const esp_err_t error = ReadRegister(kGyroZHigh, data, sizeof(data));
        const int64_t now = esp_timer_get_time();
        ++read_attempts;
        if (error != ESP_OK) {
            stationary_since_us = 0;
            if (!calibrated) {
                calibration_sum = 0;
                calibration_squares = 0;
                calibration_count = 0;
            }
            ++read_errors;
            last_read_error = error;
            ++consecutive_errors;
            if (consecutive_errors == kPersistentReadErrorSamples)
                IMU_LOGW("Gyro Z read failed at register 0x%02x: %s (%d consecutive)", kGyroZHigh,
                         esp_err_to_name(error), consecutive_errors);
            portENTER_CRITICAL(&status_lock);
            status.ready = false;
            status.error = error;
            portEXIT_CRITICAL(&status_lock);
            report_read_health(now);
            vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(kSamplePeriodMs));
            continue;
        }
        if (consecutive_errors >= kPersistentReadErrorSamples)
            IMU_LOGI("Gyro Z reads recovered after %d errors", consecutive_errors);
        consecutive_errors = 0;
        const int16_t raw = static_cast<int16_t>((data[0] << 8) | data[1]);
        const double rate = raw / kGyroScale * MPU6050_GYRO_Z_SIGN;
        if (!calibrated && motor_active.load(std::memory_order_relaxed)) {
            if (!reported_motor_active) {
                IMU_LOGW("Calibration paused: motors active; stop the car for 2 seconds");
                reported_motor_active = true;
            }
            calibration_sum = 0;
            calibration_squares = 0;
            calibration_count = 0;
        } else if (!calibrated) {
            reported_motor_active = false;
            calibration_sum += rate;
            calibration_squares += rate * rate;
            if (++calibration_count == kCalibrationSamples) {
                const double mean = calibration_sum / kCalibrationSamples;
                const double variance =
                    std::max(0.0, calibration_squares / kCalibrationSamples - mean * mean);
                if (variance <= 4.0 && std::abs(mean) <= 20.0) {
                    bias = mean;
                    calibrated = true;
                    heading_valid = true;
                    previous_sample_us = now;
                    previous_rate = 0;
                    IMU_LOGI("Gyro Z bias %.3f deg/s; calibration complete", bias);
                } else {
                    IMU_LOGW(
                        "Keep car still while calibrating gyro (mean %.2f deg/s, variance "
                        "%.2f)",
                        mean, variance);
                    calibration_sum = 0;
                    calibration_squares = 0;
                    calibration_count = 0;
                }
            }
        }
        const bool motors_running = motor_active.load(std::memory_order_relaxed);
        double corrected_rate = calibrated ? rate - bias : 0;
        if (calibrated) {
            const int64_t elapsed_us = previous_sample_us == 0 ? 0 : now - previous_sample_us;
            const bool continuous = elapsed_us > 0 && elapsed_us <= kMaximumIntegrationGapUs;
            if (continuous) {
                const double dt_s = elapsed_us / 1000000.0;
                heading += (previous_rate + corrected_rate) * 0.5 * dt_s;
            } else {
                stationary_since_us = 0;
                if (previous_sample_us != 0 && elapsed_us > kMaximumIntegrationGapUs) {
                    if (heading_valid) {
                        IMU_LOGW("Heading lost sample continuity (%lld us); restart with car "
                                 "still to restore its zero reference",
                                 static_cast<long long>(elapsed_us));
                    }
                    heading_valid = false;
                }
            }

            // A stopped motor and near-zero yaw rate provide a conservative window
            // for tracking thermal bias. This cannot identify a very slow hand turn.
            if (continuous && !motors_running && std::abs(corrected_rate) <= kStationaryRateDps) {
                if (stationary_since_us == 0)
                    stationary_since_us = now;
                if (now - stationary_since_us >= kStationaryDelayUs) {
                    const double dt_s = elapsed_us / 1000000.0;
                    bias += (rate - bias) * dt_s / kBiasTimeConstantSeconds;
                    corrected_rate = rate - bias;
                }
            } else {
                stationary_since_us = 0;
            }
            previous_sample_us = now;
            previous_rate = corrected_rate;
        }
        portENTER_CRITICAL(&status_lock);
        status.ready = calibrated;
        status.heading_valid = heading_valid;
        status.heading_degrees = heading;
        status.gyro_z_dps = corrected_rate;
        status.bias_z_dps = bias;
        status.sampled_at_us = now;
        status.error = ESP_OK;
        portEXIT_CRITICAL(&status_lock);
        report_read_health(now);
        if (calibrated && now - last_heading_log_us >= kHeadingLogIntervalUs) {
            IMU_LOGI("heading=%.2f deg, valid=%d, gyro_z=%.3f deg/s, bias_z=%.3f deg/s, "
                     "bias_tracking=%d", heading, heading_valid, corrected_rate, bias,
                     stationary_since_us != 0 && now - stationary_since_us >= kStationaryDelayUs);
            last_heading_log_us = now;
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(kSamplePeriodMs));
    }
}

void SetInitializationError(const char* stage, esp_err_t error) {
    portENTER_CRITICAL(&status_lock);
    status.error = error;
    portEXIT_CRITICAL(&status_lock);
    if (device != nullptr) {
        i2c_master_bus_rm_device(device);
        device = nullptr;
    }
    if (bus != nullptr) {
        i2c_del_master_bus(bus);
        bus = nullptr;
    }
    IMU_LOGE("%s failed: %s; IMU unavailable, using encoders only", stage, esp_err_to_name(error));
}

}  // namespace

void InitializeMpu6050() {
    if (task != nullptr || bus != nullptr)
        return;
    IMU_LOGI("Starting I2C0: SDA=GPIO%d, SCL=GPIO%d, 100 kHz, configured address=0x%02x",
             MPU6050_SDA_PIN, MPU6050_SCL_PIN, MPU6050_I2C_ADDRESS);
    i2c_master_bus_config_t bus_config = {};
    bus_config.i2c_port = I2C_NUM_0;
    bus_config.sda_io_num = MPU6050_SDA_PIN;
    bus_config.scl_io_num = MPU6050_SCL_PIN;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;
    esp_err_t error = i2c_new_master_bus(&bus_config, &bus);
    if (error != ESP_OK) {
        SetInitializationError("Create I2C bus", error);
        return;
    }
    const esp_err_t probe_68 = i2c_master_probe(bus, 0x68, 50);
    const esp_err_t probe_69 = i2c_master_probe(bus, 0x69, 50);
    IMU_LOGI("I2C probe: 0x68=%s, 0x69=%s", esp_err_to_name(probe_68), esp_err_to_name(probe_69));
    if ((MPU6050_I2C_ADDRESS == 0x68 ? probe_68 : probe_69) != ESP_OK) {
        IMU_LOGE(
            "No response at configured address 0x%02x; check 3V3, GND, SDA/SCL, "
            "pull-ups and AD0",
            MPU6050_I2C_ADDRESS);
        SetInitializationError("I2C address probe",
                               MPU6050_I2C_ADDRESS == 0x68 ? probe_68 : probe_69);
        return;
    }
    i2c_device_config_t device_config = {};
    device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    device_config.device_address = MPU6050_I2C_ADDRESS;
    device_config.scl_speed_hz = 100000;
    error = i2c_master_bus_add_device(bus, &device_config, &device);
    if (error != ESP_OK) {
        SetInitializationError("Add I2C device", error);
        return;
    }
    uint8_t identity = 0;
    error = ReadRegister(kWhoAmI, &identity, 1);
    if (error != ESP_OK) {
        SetInitializationError("Read WHO_AM_I (0x75)", error);
        return;
    }
    IMU_LOGI("WHO_AM_I register 0x75 = 0x%02x", identity);
    if (identity != kMpu6050Identity && identity != kMpu6500Identity) {
        SetInitializationError("Unexpected WHO_AM_I value", ESP_ERR_INVALID_RESPONSE);
        return;
    }
    if (identity == kMpu6500Identity) {
        IMU_LOGW(
            "Module reports MPU-6500 identity (0x70), not MPU-6050 (0x68); using compatible "
            "gyro registers");
    }
    constexpr uint8_t settings[][2] = {{kPowerManagement, 0x00},
                                       {kGyroConfig, 0x00},
                                       {kLowPassConfig, 0x03},
                                       {kSampleRateDivider, 0x09}};
    for (const auto& setting : settings) {
        error = WriteRegister(setting[0], setting[1]);
        if (error != ESP_OK) {
            IMU_LOGE("Configuration register 0x%02x = 0x%02x failed", setting[0], setting[1]);
            SetInitializationError("Write IMU configuration", error);
            return;
        }
    }
    if (xTaskCreate(SampleTask, "mpu6050", 3072, nullptr, 3, &task) != pdPASS) {
        SetInitializationError("Start gyro sampling task", ESP_ERR_NO_MEM);
        return;
    }
    IMU_LOGI("%s detected at I2C address 0x%02x; gyro sampling task started",
             identity == kMpu6500Identity ? "MPU-6500-compatible IMU" : "MPU-6050",
             MPU6050_I2C_ADDRESS);
}

Mpu6050Status GetMpu6050Status() {
    portENTER_CRITICAL(&status_lock);
    Mpu6050Status copy = status;
    portEXIT_CRITICAL(&status_lock);
    if (!copy.ready)
        copy.heading_valid = false;
    if (copy.ready && esp_timer_get_time() - copy.sampled_at_us > kMaximumIntegrationGapUs) {
        copy.ready = false;
        copy.heading_valid = false;
        copy.error = ESP_ERR_TIMEOUT;
    }
    return copy;
}

void SetMpu6050MotorActive(bool active) { motor_active.store(active, std::memory_order_relaxed); }
