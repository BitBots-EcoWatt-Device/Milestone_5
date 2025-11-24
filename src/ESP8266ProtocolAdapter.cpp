#include "ESP8266ProtocolAdapter.h"
#include "ESP8266ErrorLogger.h"

ESP8266ProtocolAdapter::ESP8266ProtocolAdapter() : lastConnectionCheck_(0)
{
}

bool ESP8266ProtocolAdapter::begin()
{
    return connectWiFi();
}

bool ESP8266ProtocolAdapter::connectWiFi()
{
    const WiFiConfig &wifiConfig = configManager.getWiFiConfig();

    Serial.print("[WiFi] Connecting to ");
    Serial.println(wifiConfig.ssid);

    WiFi.hostname(wifiConfig.hostname);
    WiFi.begin(wifiConfig.ssid, wifiConfig.password);

    // Wait for connection with timeout
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000)
    {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println();
        Serial.print("[WiFi] Connected! IP address: ");
        Serial.println(WiFi.localIP());
        return true;
    }
    else
    {
        Serial.println();
        Serial.println("[WiFi] Connection failed!");
        return false;
    }
}

bool ESP8266ProtocolAdapter::sendReadRequest(const String &frameHex, String &outFrameHex)
{
    // Check connection periodically
    if (millis() - lastConnectionCheck_ > CONNECTION_CHECK_INTERVAL)
    {
        if (!isConnected())
        {
            Serial.println("[WiFi] Connection lost, attempting reconnect...");
            connectWiFi();
        }
        lastConnectionCheck_ = millis();
    }

    if (!isConnected())
    {
        Serial.println("[HTTP] WiFi not connected");
        return false;
    }

    const APIConfig &apiConfig = configManager.getAPIConfig();
    return postJSON(apiConfig.read_url, frameHex, outFrameHex);
}

bool ESP8266ProtocolAdapter::sendWriteRequest(const String &frameHex, String &outFrameHex)
{
    if (!isConnected())
    {
        Serial.println("[HTTP] WiFi not connected");
        return false;
    }

    const APIConfig &apiConfig = configManager.getAPIConfig();
    return postJSON(apiConfig.write_url, frameHex, outFrameHex);
}

bool ESP8266ProtocolAdapter::postJSON(const String &url, const String &frameHex, String &outFrameHex)
{
    const APIConfig &apiConfig = configManager.getAPIConfig();

    httpClient_.begin(wifiClient_, url);
    setupHTTPHeaders(httpClient_);
    httpClient_.setTimeout(apiConfig.timeout_ms);

    // Create JSON payload
    StaticJsonDocument<512> jsonDoc;
    jsonDoc["frame"] = frameHex;

    String payload;
    serializeJson(jsonDoc, payload);

    Serial.print("[HTTP] POST to: ");
    Serial.println(url);
    Serial.print("[HTTP] Payload: ");
    Serial.println(payload);

    int httpResponseCode = httpClient_.POST(payload);

    if (httpResponseCode > 0)
    {
        String response = httpClient_.getString();
        Serial.print("[HTTP] Response code: ");
        Serial.println(httpResponseCode);
        Serial.print("[HTTP] Response: ");
        Serial.println(response);

        if (httpResponseCode == HTTP_CODE_OK)
        {
            // Parse JSON response
            StaticJsonDocument<512> responseDoc;
            DeserializationError error = deserializeJson(responseDoc, response);

            if (error)
            {
                Serial.print("[HTTP] JSON parsing failed: ");
                Serial.println(error.c_str());
                errorLogger.logCorruptResponse("JSON parsing failed: " + String(error.c_str()));
                httpClient_.end();
                return false;
            }

            if (responseDoc.containsKey("frame"))
            {
                outFrameHex = responseDoc["frame"].as<String>();
                httpClient_.end();
                return true;
            }
            else
            {
                Serial.println("[HTTP] Response missing 'frame' field");
                errorLogger.logCorruptResponse("HTTP response missing 'frame' field");
            }
        }
    }
    else
    {
        Serial.print("[HTTP] Error: ");
        Serial.println(httpClient_.errorToString(httpResponseCode));
        
        // Check if it's a timeout error (code -1 or -11 typically indicates timeout)
        if (httpResponseCode == -1 || httpResponseCode == HTTPC_ERROR_READ_TIMEOUT) {
            errorLogger.logTimeout("HTTP POST request");
        } else {
            errorLogger.logPacketDrop("HTTP error: " + httpClient_.errorToString(httpResponseCode));
        }
    }

    httpClient_.end();
    return false;
}

void ESP8266ProtocolAdapter::setupHTTPHeaders(HTTPClient &client)
{
    const APIConfig &apiConfig = configManager.getAPIConfig();

    client.addHeader("Content-Type", "application/json");
    client.addHeader("Accept", "application/json");

    if (strlen(apiConfig.api_key) > 0)
    {
        String authHeader = "Bearer " + String(apiConfig.api_key);
        client.addHeader("Authorization", authHeader);
    }
}