#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Ticker.h>

#ifndef EMAP_TOKEN
#define EMAP_TOKEN "INSERT-TOKEN-HERE"
#endif
String EMAP_ZONE = "DK-DK2";

void _changeColor(const char *color)
{
    lightColor(color, false);
    lightUpdate(true, true);
}

void callElectricitymap()
{
    DEBUG_MSG_P(PSTR("[GreenLight] Calling electricityMap...\n"));
    HTTPClient http;

    if (http.begin("http://api.co2signal.com/v1/latest?countryCode=" + EMAP_ZONE))
    {
        http.addHeader("auth-token", EMAP_TOKEN);

        int statusCode = http.GET();
        if (statusCode > 0)
        {
            DEBUG_MSG_P(PSTR("[GreenLight] Status Code: %i\n"), statusCode);

            String payload = http.getString();
            DynamicJsonBuffer jsonBuffer(1000);
            JsonObject &root = jsonBuffer.parseObject(payload);
            if (!root.success())
            {
                DEBUG_MSG_P(PSTR("[GreenLight] Couldn't parse response: %s\n"), payload.c_str());
                return;
            }

            float fossilFuelPercentage = root["data"]["fossilFuelPercentage"];
            String zone = root["countryCode"];

            DEBUG_MSG_P(PSTR("[GreenLight] zone: %s\n"), zone.c_str());
            DEBUG_MSG_P(PSTR("[GreenLight] fossilFuelPercentage: %s\n"), String(fossilFuelPercentage).c_str());

            if (fossilFuelPercentage < 10)
                _changeColor("100,100,100");
            else if (fossilFuelPercentage < 15)
                _changeColor("50,100,100");
            else if (fossilFuelPercentage < 20)
                _changeColor("25,100,100");
            else if (fossilFuelPercentage < 30)
                _changeColor("15,100,100");
            else
                _changeColor("0,100,100");
        }
        else
        {
            DEBUG_MSG_P(PSTR("[GreenLight] GET failed, error: %s\n"), http.errorToString(statusCode).c_str());
        }

        http.end();
    }
    else
    {
        DEBUG_MSG_P(PSTR("[GreenLight] Unable to connect\n"));
    }
}

Ticker _connectionTimer;
Ticker _restartTimer;

void noConnection()
{
    _changeColor("270,100,100");
    relayStatus(0, true, false, false);
    _connectionTimer.attach(1, []() {
        _changeColor("270,100,100");
        relayStatus(0, !relayStatus(0), false, false);
    });
}

bool _isConnectedToWifi;

// Is called almost immediately after boot. Hasn't connected to wifi yet
void extraSetup()
{
    noConnection();

    wifiRegister([](justwifi_messages_t message, char *parameter) {
        if (MESSAGE_CONNECTED == message)
        {
            _isConnectedToWifi = true;
            _restartTimer.detach();
            _connectionTimer.detach();
            _changeColor("300,100,100");
            relayStatus(0, true, false, false);

            DEBUG_MSG_P(PSTR("[GreenLight] - wifi is connected\n"));
            callElectricitymap();
        }
        else
        {
            _isConnectedToWifi = false;
            DEBUG_MSG_P(PSTR("[GreenLight] - lost connection\n"));
            noConnection();
            _restartTimer.attach(300, []() {
                DEBUG_MSG_P(PSTR("[GreenLight] - Rebooting...\n"));
                deferredReset(100, CUSTOM_RESET_TERMINAL);
            });
        }
    });

    espurnaRegisterLoop([]() {
        static auto last = millis();
        if (millis() - last > 1000 * 60 * 5 || millis() < last) // millis() < last is when millis overflow (after ~50 days)
        {
            last = millis();

            if (_isConnectedToWifi)
            {
                callElectricitymap();
            }
        }
    });

    terminalRegisterCommand(F("callemap"), [](Embedis *e) {
        DEBUG_MSG_P(PSTR("[GreenLight] Manually called electricityMap\n"));
        callElectricitymap();
    });
}
