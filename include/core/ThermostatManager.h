// include/core/ThermostatManager.h
#pragma once

#include <Arduino.h>
#include "io/SensorsController.h"
#include "program/ProgramExecutor.h"

class Config;

/**
 * @brief Менеджер термостата.
 * Отслеживает температуру по заданному датчику и запускает программы при достижении порогов.
 *
 * Логика:
 * - два порога (lower/upper) и два действия (program_id_lower/program_id_upper);
 * - срабатывание только на фронте (false -> true), чтобы не перезапускать программу каждый цикл.
 */
class ThermostatManager {
public:
    // Конструктор с зависимостями
    ThermostatManager(Config& config, SensorsController& sensors, ProgramExecutor& executor);

    // Инициализация runtime-флага из конфига
    void begin();

    // Проверка условий (вызывается периодически)
    void update();

    // Включить/выключить термостат в рантайме
    void setEnabled(bool en) { _runtimeEnabled = en; }

    // Проверить, включён ли термостат в рантайме
    bool isEnabled() const { return _runtimeEnabled; }

private:
    Config& _config;               ///< ссылка на конфигурацию
    SensorsController& _sensors;   ///< ссылка на контроллер сенсоров
    ProgramExecutor& _executor;     ///< ссылка на исполнитель программ

    bool _runtimeEnabled;           ///< включён ли термостат (рантайм)
    bool _lastStateLower;           ///< предыдущее состояние нижнего порога (для детекции фронта)
    bool _lastStateUpper;           ///< предыдущее состояние верхнего порога
};

