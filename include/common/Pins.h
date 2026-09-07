// include/common/Pins.h
#pragma once

#include <Arduino.h>

/**
 * @file Pins.h
 * @brief Аппаратная конфигурация устройства.
 * 
 * Здесь определяются пины для всех периферийных устройств,
 * а также имена по умолчанию для входов (используются в логах/отладке).
 *
 * Количество элементов в массивах автоматически определяет лимиты
 * (MAX_RELAYS, MAX_INPUTS) в Constants.h. Для датчиков температуры лимит задаётся
 * отдельно (MAX_SENSORS), т.к. количество найденных на шине 1-Wire динамическое.
 *
 * Важно для ESP8266 (bootstrapping pins):
 * - GPIO0 и GPIO2 влияют на режим загрузки. Проверьте подтяжки и состояния на старте.
 * - GPIO15 должен быть LOW при старте (обычно подтянут к GND на модуле), используйте его аккуратно.
 */

namespace Pin {

    // GSM (пины UART0; общие с Serial)
    constexpr uint8_t GSM_TX = 1;      // TXD0 -> SIM800 RXD
    constexpr uint8_t GSM_RX = 3;      // RXD0 <- SIM800 TXD

    // 1-Wire Bus для датчиков температуры (GPIO0, должен быть подтянут к VCC)
    constexpr uint8_t ONEWIRE = 0;

    // Кнопка на PCB (при старте должен быть HIGH!)
    constexpr uint8_t BUTTON = 2;

    // Цифровые входы (список пинов)
    constexpr uint8_t INPUT_PINS[] = {
        13,  // IN1
        12,  // IN2
        BUTTON  // IN3 (кнопка)
    };

    // ID входов (привязаны к INPUT_PINS по индексу)
    // Диапазон: 1001.. (не 0)
    constexpr uint16_t INPUT_IDS[] = {
        1001, // IN1
        1002, // IN2
        1003  // IN3 (кнопка)
    };

    // Реле (активный HIGH) — список пинов
    constexpr uint8_t RELAY_PINS[] = {
        14,  // RELAY1
        5,   // RELAY2
        4,   // RELAY3
        15,  // RELAY4
        16   // RELAY5 (только OUTPUT!)
    };

    // ID реле (привязаны к RELAY_PINS по индексу)
    // Диапазон: 2001.. (не 0)
    constexpr uint16_t RELAY_IDS[] = {
        2001, // RELAY1
        2002, // RELAY2
        2003, // RELAY3
        2004, // RELAY4
        2005  // RELAY5
    };

    static_assert(sizeof(INPUT_PINS) / sizeof(INPUT_PINS[0]) == sizeof(INPUT_IDS) / sizeof(INPUT_IDS[0]),
                  "INPUT_PINS and INPUT_IDS must have same length");
    static_assert(sizeof(RELAY_PINS) / sizeof(RELAY_PINS[0]) == sizeof(RELAY_IDS) / sizeof(RELAY_IDS[0]),
                  "RELAY_PINS and RELAY_IDS must have same length");

    // Аналоговый вход для измерения напряжения (0-1V, внешний делитель 1:15)
    constexpr uint8_t VBAT = A0;

    // Максимальное количество датчиков температуры (можно менять под свою плату)
    constexpr uint8_t MAX_SENSORS = 3;

} // namespace Pin

