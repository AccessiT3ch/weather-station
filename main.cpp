#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include "httplib.h"
#include "nlohmann/json.hpp"
#include <random>

// BME280 I2C Address and Registers
#define BME280_I2C_ADDR 0x76
#define BME280_REG_DIG_T1 0x88
#define BME280_REG_CTRL_MEAS 0xF4
#define BME280_REG_CONFIG 0xF5
#define BME280_REG_DATA 0xF7

struct CalibrationData {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;
    uint8_t  dig_H1;
    int16_t  dig_H2;
    uint8_t  dig_H3;
    int16_t  dig_H4;
    int16_t  dig_H5;
    int8_t   dig_H6;
};

// Simple UUIDv4 generator
std::string generate_uuid() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    std::uniform_int_distribution<> dis2(8, 11);
    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; i++) ss << dis(gen);
    ss << "-4";
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    ss << dis2(gen);
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 12; i++) ss << dis(gen);
    return ss.str();
}

// ISO-8601 Timestamp
std::string get_iso_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

// Read and compute live sensor values
bool read_bme280(double &temp_f, double &humidity, double &pressure) {
    int file = open("/dev/i2c-1", O_RDWR);
    if (file < 0) return false;

    if (ioctl(file, I2C_SLAVE, BME280_I2C_ADDR) < 0) {
        close(file);
        return false;
    }

    char config[3];
    config[0] = 0xF2; // ctrl_hum
    config[1] = 0x01; // oversampling x1
    write(file, config, 2);

    config[0] = 0xF4; // ctrl_meas
    config[1] = 0x27; // temp x1, pressure x1, normal mode
    write(file, config, 2);

    CalibrationData calib;
    
    // Read temperature/pressure calibration data (0x88 to 0xA1)
    char reg = 0x88;
    write(file, &reg, 1);
    char calib_data[26];
    read(file, calib_data, 26);

    calib.dig_T1 = (calib_data[1] << 8) | calib_data[0];
    calib.dig_T2 = (int16_t)((calib_data[3] << 8) | calib_data[2]);
    calib.dig_T3 = (int16_t)((calib_data[5] << 8) | calib_data[4]);
    calib.dig_P1 = (calib_data[7] << 8) | calib_data[6];
    calib.dig_P2 = (int16_t)((calib_data[9] << 8) | calib_data[8]);
    calib.dig_P3 = (int16_t)((calib_data[11] << 8) | calib_data[10]);
    calib.dig_P4 = (int16_t)((calib_data[13] << 8) | calib_data[12]);
    calib.dig_P5 = (int16_t)((calib_data[15] << 8) | calib_data[14]);
    calib.dig_P6 = (int16_t)((calib_data[17] << 8) | calib_data[16]);
    calib.dig_P7 = (int16_t)((calib_data[19] << 8) | calib_data[18]);
    calib.dig_P8 = (int16_t)((calib_data[21] << 8) | calib_data[20]);
    calib.dig_P9 = (int16_t)((calib_data[23] << 8) | calib_data[22]);
    calib.dig_H1 = calib_data[25];

    // Read humidity calibration data (0xE1 to 0xE7)
    reg = 0xE1;
    write(file, &reg, 1);
    char hum_calib[7];
    read(file, hum_calib, 7);

    calib.dig_H2 = (int16_t)((hum_calib[1] << 8) | hum_calib[0]);
    calib.dig_H3 = hum_calib[2];
    calib.dig_H4 = (int16_t)((hum_calib[3] << 4) | (hum_calib[4] & 0x0F));
    calib.dig_H5 = (int16_t)((hum_calib[5] << 4) | ((hum_calib[4] >> 4) & 0x0F));
    calib.dig_H6 = (int8_t)hum_calib[6];

    // Read raw data registers
    reg = 0xF7;
    write(file, &reg, 1);
    char data[8];
    read(file, data, 8);
    close(file);

    int32_t adc_p = ((uint32_t)data[0] << 12) | ((uint32_t)data[1] << 4) | ((data[2] >> 4) & 0x0F);
    int32_t adc_t = ((uint32_t)data[3] << 12) | ((uint32_t)data[4] << 4) | ((data[5] >> 4) & 0x0F);
    int32_t adc_h = ((uint32_t)data[6] << 8) | data[7];

    // Temperature compensation
    int32_t var1 = ((((adc_t >> 3) - ((int32_t)calib.dig_T1 << 1))) * ((int32_t)calib.dig_T2)) >> 11;
    int32_t var2 = (((((adc_t >> 4) - ((int32_t)calib.dig_T1)) * ((adc_t >> 4) - ((int32_t)calib.dig_T1))) >> 12) * ((int32_t)calib.dig_T3)) >> 14;
    int32_t t_fine = var1 + var2;
    double temp_c = ((t_fine * 5 + 128) >> 8) / 100.0;
    temp_f = (temp_c * 9.0 / 5.0) + 32.0;

    // Pressure compensation
    int64_t p_var1, p_var2, p;
    p_var1 = ((int64_t)t_fine) - 128000;
    p_var2 = p_var1 * p_var1 * (int64_t)calib.dig_P6;
    p_var2 = p_var2 + ((p_var1 * (int64_t)calib.dig_P5) << 17);
    p_var2 = p_var2 + (((int64_t)calib.dig_P4) << 35);
    p_var1 = ((p_var1 * p_var1 * (int64_t)calib.dig_P3) >> 8) + ((p_var1 * (int64_t)calib.dig_P2) << 12);
    p_var1 = (((int64_t)1 << 47) + p_var1) * ((int64_t)calib.dig_P1) >> 33;
    if (p_var1 == 0) {
        pressure = 0;
    } else {
        p = 1048576 - adc_p;
        p = (((p << 31) - p_var2) * 3125) / p_var1;
        p_var1 = (((int64_t)calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
        p_var2 = (((int64_t)calib.dig_P8) * p) >> 19;
        p = ((p + p_var1 + p_var2) >> 8) + (((int64_t)calib.dig_P7) << 4);
        pressure = ((double)p / 256.0) / 100.0;
    }

    // Humidity compensation
    int32_t v_x1_u32r = (t_fine - ((int32_t)76800));
    v_x1_u32r = (((((adc_h << 14) - (((int32_t)calib.dig_H4) << 20) - (((int32_t)calib.dig_H5) * v_x1_u32r)) +
                 ((int32_t)16384)) >> 15) * (((((((v_x1_u32r * ((int32_t)calib.dig_H6)) >> 10) *
                 (((v_x1_u32r * ((int32_t)calib.dig_H3)) >> 11) + ((int32_t)32768))) >> 10) +
                 ((int32_t)2097152)) * ((int32_t)calib.dig_H2) + 8192) >> 14));
    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)calib.dig_H1)) >> 4));
    v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
    v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);
    humidity = (double)(v_x1_u32r >> 12) / 1024.0;

    return true;
}

int main() {
    httplib::Server svr;

    svr.Get("/weather", [](const httplib::Request&, httplib::Response& res) {
        double temp_f = 0.0;
        double humidity = 0.0;
        double pressure = 0.0;

        if (!read_bme280(temp_f, humidity, pressure)) {
            res.status = 500;
            res.set_content("{\"error\":\"Failed to read BME280 sensor\"}", "application/json");
            return;
        }

        nlohmann::json j;
        j["id"] = generate_uuid();
        j["timestamp"] = get_iso_timestamp();
        j["temperature_f"] = temp_f;
        j["humidity_pct"] = humidity;
        j["pressure_hpa"] = pressure;

        res.set_content(j.dump(), "application/json");
    });

    std::cout << "Starting Live BME280 Weather Station HTTP server on port 8080..." << std::endl;
    svr.listen("0.0.0.0", 8080);
    return 0;
}
