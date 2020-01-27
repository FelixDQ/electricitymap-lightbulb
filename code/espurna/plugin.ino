#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Ticker.h>
#include <WiFiClientSecureBearSSL.h>

void _changeColor(const char * color)
{
    lightColor(color, false);
    lightUpdate(true, true);
}

void callElectricitymap()
{
    DEBUG_MSG_P(PSTR("[GreenLight] Calling electricityMap...\n"));
    BearSSL::WiFiClientSecure secure;
    secure.setInsecure();
    HTTPClient http;
    http.begin(secure, "api.co2signal.com", 443, "/v1/latest?countryCode=DK-DK2", true);
    http.addHeader("auth-token", EMAP_TOKEN);
    int statusCode = http.GET();
    String payload = http.getString();
    DEBUG_MSG_P(PSTR("[GreenLight] Status Code: %i\n"), statusCode);

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
    DEBUG_MSG_P(PSTR("[GreenLight] fossilFuelPercentage: %f\n"), fossilFuelPercentage);
    http.end();

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

Ticker _connectionTimer;
Ticker _syncTimer;
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

void extraSetup()
{
    noConnection();

    wifiRegister([](justwifi_messages_t message, char *parameter) {
        if (MESSAGE_CONNECTED == message)
        {
            _restartTimer.detach();
            _connectionTimer.detach();
            _changeColor("300,100,100");
            relayStatus(0, true, false, false);

            DEBUG_MSG_P(PSTR("[GreenLight] - wifi is connected\n"));
            callElectricitymap();
            _syncTimer.attach(360, []() {
                callElectricitymap();
            });
        }
        else
        {
            _syncTimer.detach();
            DEBUG_MSG_P(PSTR("[GreenLight] - lost connection\n"));
            noConnection();
            _restartTimer.once(300, []() {
                DEBUG_MSG_P(PSTR("[GreenLight] - Rebooting...\n"));
                deferredReset(100, CUSTOM_RESET_TERMINAL);
            });
        }
    });
}