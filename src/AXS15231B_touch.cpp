#include "AXS15231B_touch.h"

AXS15231B_Touch* AXS15231B_Touch::instance = nullptr;

AXS15231B_Touch::AXS15231B_Touch(uint8_t scl, uint8_t sda, uint8_t int_pin, 
                               uint8_t addr, uint8_t rotation) {
    this->scl = scl;
    this->sda = sda;
    this->int_pin = int_pin;
    this->addr = addr;
    this->rotation = rotation % 4; // Гарантировать валидное значение
    this->last_error = ERROR_NONE;
}

AXS15231B_Touch::~AXS15231B_Touch() {
    // Очистка прерывания при уничтожении объекта
    if (instance == this) {
        detachInterrupt(digitalPinToInterrupt(int_pin));
        instance = nullptr;
    }
}

bool AXS15231B_Touch::begin() {
    // Проверка валидности пинов
    if (scl == 0 || sda == 0 || int_pin == 0) {
        last_error = ERROR_INVALID_PIN;
        return false;
    }

    instance = this;

    // Инициализация I2C
    if (!Wire.begin(sda, scl)) {
        last_error = ERROR_I2C_INIT;
        return false;
    }

    // Проверка наличия устройства
    if (!isDevicePresent()) {
        last_error = ERROR_DEVICE_NOT_FOUND;
        return false;
    }

    // Настройка пина прерывания
    pinMode(int_pin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(int_pin), isrTouched, FALLING);

    last_error = ERROR_NONE;
    return true;
}

bool AXS15231B_Touch::isDevicePresent() {
    Wire.beginTransmission(addr);
    uint8_t error = Wire.endTransmission();
    return (error == 0);
}

ISR_PREFIX
void AXS15231B_Touch::isrTouched() {
    if (instance) {
        #ifdef ESP32
        portENTER_CRITICAL_ISR(&instance->mux);
        #endif
        instance->touch_int = true;
        #ifdef ESP32
        portEXIT_CRITICAL_ISR(&instance->mux);
        #endif
    }
}

void AXS15231B_Touch::setRotation(uint8_t rot) {
    rotation = rot % 4; // Гарантировать валидное значение
}

bool AXS15231B_Touch::touched() {
    return update();
}

void AXS15231B_Touch::readData(uint16_t *x, uint16_t *y) {
    *x = point_X;
    *y = point_Y;
}

void AXS15231B_Touch::enOffsetCorrection(bool enable) {
    en_offset_correction = enable;
}

void AXS15231B_Touch::setOffsets(uint16_t x_real_min, uint16_t x_real_max, uint16_t x_ideal_max, 
                               uint16_t y_real_min, uint16_t y_real_max, uint16_t y_ideal_max) {
    this->x_real_min = x_real_min;
    this->x_real_max = x_real_max;
    this->x_ideal_max = x_ideal_max;
    this->y_real_min = y_real_min;
    this->y_real_max = y_real_max;
    this->y_ideal_max = y_ideal_max;
}

void AXS15231B_Touch::correctOffset(uint16_t *x, uint16_t *y) {
    *x = map(*x, x_real_min, x_real_max, 0, x_ideal_max);
    *y = map(*y, y_real_min, y_real_max, 0, y_ideal_max);
}

uint8_t AXS15231B_Touch::getLastError() const {
    return last_error;
}

const char* AXS15231B_Touch::getErrorString() const {
    switch (last_error) {
        case ERROR_NONE: return "No error";
        case ERROR_I2C_INIT: return "I2C initialization failed";
        case ERROR_DEVICE_NOT_FOUND: return "Device not found on I2C bus";
        case ERROR_I2C_COMMUNICATION: return "I2C communication error";
        case ERROR_INVALID_DATA: return "Invalid data received";
        case ERROR_INVALID_PIN: return "Invalid pin configuration";
        default: return "Unknown error";
    }
}

void AXS15231B_Touch::calibrate() {
    // Базовая реализация калибровки - можно расширить
    // Установка стандартных значений для 12-битного тачскрина
    setOffsets(100, 3900, 4095, 100, 3900, 4095);
    enOffsetCorrection(true);
}

bool AXS15231B_Touch::update() {
    // Атомарное чтение флага прерывания
    bool local_touch_int;
    #ifdef ESP32
    portENTER_CRITICAL(&mux);
    #endif
    local_touch_int = touch_int;
    touch_int = false;
    #ifdef ESP32
    portEXIT_CRITICAL(&mux);
    #endif
    
    if (!local_touch_int) {
        return false;
    }

    // Установка таймаута для I2C
    #ifdef ESP32
    Wire.setTimeout(50);
    #endif

    uint8_t tmp_buf[8] = {0};
    static const uint8_t read_touchpad_cmd[8] = {0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, 0x08};

    // Отправка команды
    Wire.beginTransmission(addr);
    Wire.write(read_touchpad_cmd, sizeof(read_touchpad_cmd));
    if (Wire.endTransmission() != 0) {
        last_error = ERROR_I2C_COMMUNICATION;
        return false;
    }

    delay(1); // Короткая задержка для обработки

    // Чтение ответа
    uint8_t bytes_read = Wire.requestFrom(addr, sizeof(tmp_buf));
    if (bytes_read != sizeof(tmp_buf)) {
        last_error = ERROR_I2C_COMMUNICATION;
        return false;
    }

    for(int i = 0; i < sizeof(tmp_buf) && Wire.available(); i++) {
        tmp_buf[i] = Wire.read();
    }

    // Извлечение координат
    uint16_t raw_X = AXS_GET_POINT_X(tmp_buf);
    uint16_t raw_Y = AXS_GET_POINT_Y(tmp_buf);

    // Валидация данных
    if (raw_X == 0 && raw_Y == 0) {
        last_error = ERROR_INVALID_DATA;
        return false;
    }

    // Ограничение значений
    raw_X = constrain(raw_X, x_real_min, x_real_max);
    raw_Y = constrain(raw_Y, y_real_min, y_real_max);

    // Коррекция смещения
    uint16_t x_max, y_max;
    if (en_offset_correction) {
        correctOffset(&raw_X, &raw_Y);
        x_max = x_ideal_max;
        y_max = y_ideal_max;
    } else {
        x_max = x_real_max;
        y_max = y_real_max;
    }

    // Применение вращения
    switch (rotation) {
        case 0:
            point_X = raw_X;
            point_Y = raw_Y;
            break;
        case 1:
            point_X = raw_Y;
            point_Y = x_max - raw_X;
            break;
        case 2:
            point_X = x_max - raw_X;
            point_Y = y_max - raw_Y;
            break;
        case 3:
            point_X = y_max - raw_Y;
            point_Y = raw_X;
            break;
        default:
            point_X = raw_X;
            point_Y = raw_Y;
            break;
    }

    last_error = ERROR_NONE;
    return true;
}