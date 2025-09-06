#ifndef AXS15231B_Touch_H
#define AXS15231B_Touch_H

#include <Arduino.h>
#include <Wire.h>

class AXS15231B_Touch {
public:
    AXS15231B_Touch(uint8_t scl, uint8_t sda, uint8_t int_pin, 
                   uint8_t addr = 0x38, uint8_t rotation = 0);
    ~AXS15231B_Touch();
    
    bool begin();
    bool touched();
    void setRotation(uint8_t rotation);
    void readData(uint16_t *x, uint16_t *y);
    void enOffsetCorrection(bool enable);
    void setOffsets(uint16_t x_real_min, uint16_t x_real_max, uint16_t x_ideal_max, 
                   uint16_t y_real_min, uint16_t y_real_max, uint16_t y_ideal_max);
    
    // Новые методы
    bool isDevicePresent();
    uint8_t getLastError() const;
    const char* getErrorString() const;
    void calibrate();

    // Коды ошибок
    enum ErrorCodes {
        ERROR_NONE = 0,
        ERROR_I2C_INIT,
        ERROR_DEVICE_NOT_FOUND,
        ERROR_I2C_COMMUNICATION,
        ERROR_INVALID_DATA,
        ERROR_INVALID_PIN
    };

protected:
    volatile bool touch_int = false;

private:
    uint8_t scl, sda, int_pin, addr, rotation;
    uint8_t last_error = ERROR_NONE;
    
    bool en_offset_correction = false;
    uint16_t point_X = 0, point_Y = 0;
    uint16_t x_real_min = 0, x_real_max = 4095;
    uint16_t y_real_min = 0, y_real_max = 4095;
    uint16_t x_ideal_max = 4095, y_ideal_max = 4095;

    #ifdef ESP32
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    #endif

    bool update();
    void correctOffset(uint16_t *x, uint16_t *y);
    static void isrTouched();
    static AXS15231B_Touch* instance;

    // Удалить копирование
    AXS15231B_Touch(const AXS15231B_Touch&) = delete;
    AXS15231B_Touch& operator=(const AXS15231B_Touch&) = delete;
};

#define AXS_GET_GESTURE_TYPE(buf)  (buf[0])
#define AXS_GET_FINGER_NUMBER(buf) (buf[1])
#define AXS_GET_EVENT(buf)         ((buf[2] >> 0) & 0x03)
#define AXS_GET_POINT_X(buf)       (((uint16_t)(buf[2] & 0x0F) <<8) + (uint16_t)buf[3])
#define AXS_GET_POINT_Y(buf)       (((uint16_t)(buf[4] & 0x0F) <<8) + (uint16_t)buf[5])

#ifndef ISR_PREFIX
  #if defined(ESP8266)
    #define ISR_PREFIX ICACHE_RAM_ATTR
  #elif defined(ESP32)
    #define ISR_PREFIX IRAM_ATTR
  #else
    #define ISR_PREFIX
  #endif
#endif

#endif