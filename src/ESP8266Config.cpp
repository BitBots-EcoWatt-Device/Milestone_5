#include "ESP8266Config.h"

ConfigManager configManager;

ConfigManager::ConfigManager()
{
    loadDefaults();
}

bool ConfigManager::begin()
{
    EEPROM.begin(EEPROM_SIZE);
    return loadConfig();
}

bool ConfigManager::loadConfig()
{
    EEPROM.get(0, config_);

    if (!isConfigValid())
    {
        Serial.println("[CONFIG] Invalid config in EEPROM, loading defaults");
        loadDefaults();
        return false;
    }

    Serial.println("[CONFIG] Configuration loaded successfully");
    return true;
}

bool ConfigManager::saveConfig()
{
    config_.magic = CONFIG_MAGIC;
    EEPROM.put(0, config_);
    bool success = EEPROM.commit();

    if (success)
    {
        Serial.println("[CONFIG] Configuration saved successfully");
    }
    else
    {
        Serial.println("[CONFIG] Failed to save configuration");
    }

    return success;
}

void ConfigManager::loadDefaults()
{
    // WiFi defaults
    strcpy(config_.wifi.ssid, "PrimeAlphA");
    strcpy(config_.wifi.password, "AlphaDBR11");
    strcpy(config_.wifi.hostname, "bitbots-ecoWatt");

    // API defaults
    strcpy(config_.api.api_key, "NjhhZWIwNDU1ZDdmMzg3MzNiMTQ5YTFjOjY4YWViMDQ1NWQ3ZjM4NzMzYjE0OWExMg==");
    strcpy(config_.api.read_url, "http://20.15.114.131:8080/api/inverter/read");
    strcpy(config_.api.write_url, "http://20.15.114.131:8080/api/inverter/write");
    strcpy(config_.api.upload_url, "http://10.23.168.102:5000/upload");
    strcpy(config_.api.config_url, "http://10.23.168.102:5000/config");
    config_.api.timeout_ms = 5000;

    // Security defaults
    strcpy(config_.security.psk, "E5A3C8B2F0D9E8A1C5B3A2D8F0E9C4B2A1D8E5C3B0A9F8E2D1C0B7A6F5E4D3C2");
    config_.security.nonce = 0; // The counter always starts at 0

    // Boot status defaults
    config_.boot_status.ota_reboot_pending = false;
    config_.boot_status.boot_success_reported = false;
    strcpy(config_.boot_status.last_boot_status, "success");
    strcpy(config_.boot_status.boot_error_message, "");

    // Firmware version default
    strcpy(config_.firmware_version, "1.0.0");

    // Device defaults
    config_.device.slave_address = 0x11;
    config_.device.poll_interval_ms = 5000;
    config_.device.upload_interval_ms = 15000;
    config_.device.buffer_size = 10;

    // Initialize default polling parameters
    config_.device.num_enabled_params = 5;
    config_.device.enabled_params[0] = ParameterType::AC_VOLTAGE;
    config_.device.enabled_params[1] = ParameterType::AC_CURRENT;
    config_.device.enabled_params[2] = ParameterType::AC_FREQUENCY;
    config_.device.enabled_params[3] = ParameterType::TEMPERATURE;
    config_.device.enabled_params[4] = ParameterType::OUTPUT_POWER;

    config_.magic = CONFIG_MAGIC;
}

void ConfigManager::setWiFiConfig(const char *ssid, const char *password, const char *hostname)
{
    strncpy(config_.wifi.ssid, ssid, sizeof(config_.wifi.ssid) - 1);
    strncpy(config_.wifi.password, password, sizeof(config_.wifi.password) - 1);
    strncpy(config_.wifi.hostname, hostname, sizeof(config_.wifi.hostname) - 1);

    config_.wifi.ssid[sizeof(config_.wifi.ssid) - 1] = '\0';
    config_.wifi.password[sizeof(config_.wifi.password) - 1] = '\0';
    config_.wifi.hostname[sizeof(config_.wifi.hostname) - 1] = '\0';
}

void ConfigManager::setAPIConfig(const char *api_key, const char *read_url, const char *write_url, const char *upload_url, const char *config_url, uint16_t timeout_ms)
{
    strncpy(config_.api.api_key, api_key, sizeof(config_.api.api_key) - 1);
    strncpy(config_.api.read_url, read_url, sizeof(config_.api.read_url) - 1);
    strncpy(config_.api.write_url, write_url, sizeof(config_.api.write_url) - 1);
    if (upload_url)
        strncpy(config_.api.upload_url, upload_url, sizeof(config_.api.upload_url) - 1);
    else
        config_.api.upload_url[0] = '\0';
    if (config_url)
        strncpy(config_.api.config_url, config_url, sizeof(config_.api.config_url) - 1);
    else
        config_.api.config_url[0] = '\0';

    config_.api.api_key[sizeof(config_.api.api_key) - 1] = '\0';
    config_.api.read_url[sizeof(config_.api.read_url) - 1] = '\0';
    config_.api.write_url[sizeof(config_.api.write_url) - 1] = '\0';
    config_.api.upload_url[sizeof(config_.api.upload_url) - 1] = '\0';
    config_.api.config_url[sizeof(config_.api.config_url) - 1] = '\0';
    config_.api.timeout_ms = timeout_ms;
}

void ConfigManager::setDeviceConfig(uint8_t slave_addr, uint16_t poll_interval, uint16_t upload_interval, uint8_t buffer_size)
{
    config_.device.slave_address = slave_addr;
    config_.device.poll_interval_ms = poll_interval;
    config_.device.upload_interval_ms = upload_interval;
    config_.device.buffer_size = buffer_size;
}

void ConfigManager::setFirmwareVersion(const char *version)
{
    strncpy(config_.firmware_version, version, sizeof(config_.firmware_version) - 1);
    config_.firmware_version[sizeof(config_.firmware_version) - 1] = '\0';
}

bool ConfigManager::isConfigValid() const
{
    return config_.magic == CONFIG_MAGIC &&
           strlen(config_.wifi.ssid) > 0 &&
           strlen(config_.api.api_key) > 0;
}

void ConfigManager::updatePollingConfig(uint16_t new_interval, const std::vector<ParameterType> &new_params)
{
    config_.device.poll_interval_ms = new_interval;
    config_.device.num_enabled_params = min((uint8_t)new_params.size(), (uint8_t)MAX_POLLING_PARAMS);
    for (uint8_t i = 0; i < config_.device.num_enabled_params; ++i)
    {
        config_.device.enabled_params[i] = new_params[i];
    }
}

uint32_t ConfigManager::getNextNonce()
{
    // Increment the nonce value in the configuration object
    config_.security.nonce++;

    // Immediately save the entire configuration to EEPROM to persist the new nonce
    saveConfig();

    // Return the new nonce that should be used for the current message
    return config_.security.nonce;
}

// Boot status management methods
void ConfigManager::setOTARebootFlag(bool pending)
{
    config_.boot_status.ota_reboot_pending = pending;
    config_.boot_status.boot_success_reported = false;
    if (pending)
    {
        strcpy(config_.boot_status.last_boot_status, "rebooting");
        strcpy(config_.boot_status.boot_error_message, "");
    }
    saveConfig();
}

void ConfigManager::setBootStatus(const char *status, const char *error_message)
{
    strncpy(config_.boot_status.last_boot_status, status, sizeof(config_.boot_status.last_boot_status) - 1);
    config_.boot_status.last_boot_status[sizeof(config_.boot_status.last_boot_status) - 1] = '\0';
    
    if (error_message)
    {
        strncpy(config_.boot_status.boot_error_message, error_message, sizeof(config_.boot_status.boot_error_message) - 1);
        config_.boot_status.boot_error_message[sizeof(config_.boot_status.boot_error_message) - 1] = '\0';
    }
    else
    {
        config_.boot_status.boot_error_message[0] = '\0';
    }
    
    config_.boot_status.boot_success_reported = false;
    saveConfig();
}

void ConfigManager::markBootSuccessReported()
{
    config_.boot_status.boot_success_reported = true;
    config_.boot_status.ota_reboot_pending = false;
    saveConfig();
}

bool ConfigManager::needsBootStatusReport() const
{
    // Report boot status if:
    // 1. OTA reboot was pending and we successfully booted (status changed to success)
    // 2. Boot success hasn't been reported yet
    return config_.boot_status.ota_reboot_pending || !config_.boot_status.boot_success_reported;
}
