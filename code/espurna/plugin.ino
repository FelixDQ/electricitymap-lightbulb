#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Ticker.h>
#include <WiFiClientSecureBearSSL.h>

void syncFromElectricitymap()
{
    BearSSL::WiFiClientSecure secure;
    secure.setInsecure();
    HTTPClient http;
    http.begin(secure, "api.electricitymap.org", 443, "/v3/power-consumption-breakdown/latest", false);
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

    int fossilFreePercentage = root["fossilFreePercentage"];
    String zone = root["zone"];

    DEBUG_MSG_P(PSTR("[GreenLight] zone: %s\n"), zone.c_str());
    DEBUG_MSG_P(PSTR("[GreenLight] fossilFreePercentage: %i\n"), fossilFreePercentage);
    http.end();
    secure.stop();

    if (fossilFreePercentage > 90)
    {
        lightColor("100,100,100", false);
        lightUpdate(true, true);
    }
    else if (fossilFreePercentage > 85)
    {
        lightColor("50,100,100", false);
        lightUpdate(true, true);
    }
    else if (fossilFreePercentage > 80)
    {
        lightColor("25,100,100", false);
        lightUpdate(true, true);
    }
    else if (fossilFreePercentage > 70)
    {
        lightColor("15,100,100", false);
        lightUpdate(true, true);
    }
    else
    {
        lightColor("0,100,100", false);
        lightUpdate(true, true);
    }
}

Ticker _timer;

void noConnection()
{
    lightColor("270,100,100", false);
    lightUpdate(true, true);
    relayStatus(0, true, false, false);
    _timer.attach(1, []() {
        lightColor("270,100,100", false);
        lightUpdate(true, true);
        relayStatus(0, !relayStatus(0), false, false);
    });
}

void extraSetup()
{
    espurnaRegisterLoop([]() {
        static auto last = millis();
        if (millis() - last > 10000)
        {
            last = millis();
            DEBUG_MSG_P(PSTR("[GreenLight] Ping\n"));
        }
    });
    
    noConnection();

    wifiRegister([](justwifi_messages_t message, char *parameter) {
        if (MESSAGE_CONNECTED == message)
        {
            _timer.detach();
            lightColor("300,100,100", false);
            lightUpdate(true, true);
            relayStatus(0, true, false, false);

            DEBUG_MSG_P(PSTR("[GreenLight] - wifi is connected\n"));
            syncFromElectricitymap();

            espurnaRegisterLoop([]() {
                static auto last = millis();
                if (millis() - last > 300000)
                {
                    last = millis();
                    syncFromElectricitymap();
                }
            });
        } else {
            noConnection();
        }
    });
}