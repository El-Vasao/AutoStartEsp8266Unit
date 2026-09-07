// include/core/ModeManager.h
#pragma once

#include <Arduino.h>
#include "common/Constants.h"

// Предварительные объявления
class WebServer;
class GSMController;
class MQTTClient;
class Config;
class OTAHandler;

/**
 * @brief Управление режимами работы системы.
 * Обеспечивает переключение между режимами и вызов соответствующих методов входа/выхода.
 *
 * Контракт:
 * - режимы должны включать/выключать только “внешние” подсистемы (AP, OTA, связь),
 *   а основная логика (IO, программы, триггеры) живёт в `Core::handle*`.
 *
 * Примечание: в заголовке когда-то был enter/exitBatterySave, но в `CoreMode` такого режима нет.
 * Либо возвращаем и доводим до рабочего состояния, либо окончательно убираем хвост.
 */
class ModeManager {
public:
    ModeManager();

    // Инициализация ссылками на подсистемы (вызывается из Core)
    void init(WebServer& webServer, GSMController& gsm, MQTTClient& mqtt,
              Config& config, OTAHandler& ota);

    // Переключиться в новый режим
    void switchMode(CoreMode newMode);

    // Текущий режим
    CoreMode getCurrentMode() const { return _currentMode; }

    // Имя текущего режима
    const char* getModeName() const;

    // Время входа в текущий режим (мс)
    uint32_t getModeEnterTime() const { return _modeEnterTime; }

    // Длительность текущего режима (мс)
    uint32_t getModeDuration() const { return millis() - _modeEnterTime; }

    // Является ли текущий режим точкой доступа
    bool isApMode() const {
        return _currentMode == CoreMode::EMERGENCY_AP || _currentMode == CoreMode::SETUP_AP;
    }

private:
    CoreMode _currentMode;          ///< текущий режим
    uint32_t _modeEnterTime;        ///< время входа в режим (мс)

    WebServer* _webServer;          ///< ссылка на веб-сервер
    GSMController* _gsm;            ///< ссылка на GSM-контроллер
    MQTTClient* _mqtt;              ///< ссылка на MQTT-клиент
    Config* _config;                ///< ссылка на конфигурацию
    OTAHandler* _ota;               ///< ссылка на обработчик OTA

    // Методы входа/выхода для каждого режима
    void enterBoot();
    void exitBoot();
    void enterEmergencyAP();
    void exitEmergencyAP();
    void enterSetupAP();
    void exitSetupAP();
    void enterNormal();
    void exitNormal();
    void enterNormalSilent();
    void exitNormalSilent();
    // Примечание: режим BatterySave был “заглушкой” и удалён, чтобы `CoreMode` и `ModeManager` не расходились.
    void enterOTAUpdate();
    void exitOTAUpdate();
};

