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

    // Calculate hash of current defaults in code
    uint32_t current_defaults_hash = calculateDefaultsHash();

    // Check if config is invalid OR if defaults have changed
    if (!isConfigValid())
    {
        Serial.println("[CONFIG] Invalid config in EEPROM, loading defaults");
        loadDefaults();
        saveConfig(); // Save the new defaults
        return false;
    }
    
    if (config_.config_version != current_defaults_hash)
    {
        Serial.println("[CONFIG] Configuration defaults have changed");
        Serial.print("[CONFIG] Stored hash: 0x");
        Serial.print(config_.config_version, HEX);
        Serial.print(", Current hash: 0x");
        Serial.println(current_defaults_hash, HEX);
        Serial.println("[CONFIG] Loading new defaults and updating EEPROM");
        loadDefaults();
        saveConfig(); // Save the new defaults
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

// Helper function to populate default values
// THIS IS THE SINGLE SOURCE OF TRUTH FOR ALL DEFAULT VALUES!
// Change values here, and both loadDefaults() and hash calculation use them.
void ConfigManager::populateDefaults(ESP8266Config &cfg) const
{
    // ============================================
    // WiFi defaults
    // ============================================
    strcpy(cfg.wifi.ssid, "PrimeAlphA");
    strcpy(cfg.wifi.password, "AlphaDBR11");
    strcpy(cfg.wifi.hostname, "bitbots-ecoWatt");

    // ============================================
    // API defaults - CHANGE THESE WHEN NEEDED
    // ============================================
    strcpy(cfg.api.api_key, "NjhhZWIwNDU1ZDdmMzg3MzNiMTQ5YTFjOjY4YWViMDQ1NWQ3ZjM4NzMzYjE0OWExMg==");
    strcpy(cfg.api.read_url, "http://20.15.114.131:8080/api/inverter/read");
    strcpy(cfg.api.write_url, "http://20.15.114.131:8080/api/inverter/write");
    strcpy(cfg.api.upload_url, "http://10.23.168.124:5001/upload");
    strcpy(cfg.api.config_url, "http://10.23.168.124:5001/config");
    cfg.api.timeout_ms = 5000;

    // ============================================
    // Security defaults
    // ============================================
    strcpy(cfg.security.psk, "E5A3C8B2F0D9E8A1C5B3A2D8F0E9C4B2A1D8E5C3B0A9F8E2D1C0B7A6F5E4D3C2");
    cfg.security.nonce = 0; // The counter always starts at 0

    // ============================================
    // Boot status defaults
    // ============================================
    cfg.boot_status.ota_reboot_pending = false;
    cfg.boot_status.boot_success_reported = false;
    strcpy(cfg.boot_status.last_boot_status, "success");
    strcpy(cfg.boot_status.boot_error_message, "");

    // ============================================
    // Firmware version default
    // ============================================
    strcpy(cfg.firmware_version, "1.0.0");

    // ============================================
    // Device defaults
    // ============================================
    cfg.device.slave_address = 0x11;
    cfg.device.poll_interval_ms = 5000;
    cfg.device.upload_interval_ms = 15000;
    cfg.device.buffer_size = 10;

    // ============================================
    // Initialize default polling parameters
    // ============================================
    cfg.device.num_enabled_params = 5;
    cfg.device.enabled_params[0] = ParameterType::AC_VOLTAGE;
    cfg.device.enabled_params[1] = ParameterType::AC_CURRENT;
    cfg.device.enabled_params[2] = ParameterType::AC_FREQUENCY;
    cfg.device.enabled_params[3] = ParameterType::TEMPERATURE;
    cfg.device.enabled_params[4] = ParameterType::OUTPUT_POWER;

    cfg.magic = CONFIG_MAGIC;
    cfg.config_version = 0; // Will be set by caller
}

void ConfigManager::loadDefaults()
{
    // Use the single source of truth for all defaults
    populateDefaults(config_);
    
    // Set the config version based on these defaults
    config_.config_version = calculateDefaultsHash();
}

// Calculate a hash of the default configuration values
// This automatically changes when you modify any default value in populateDefaults()
uint32_t ConfigManager::calculateDefaultsHash() const
{
    // Create a temporary config with default values
    ESP8266Config temp_config;
    
    // Load defaults into temp config (single source of truth)
    populateDefaults(temp_config);
    
    // Now calculate hash from the temp config structure
    uint32_t hash = 0x811C9DC5; // FNV-1a initial value
    
    // Helper lambda to hash a string
    auto hashString = [&hash](const char* str) {
        while (*str) {
            hash ^= (uint8_t)*str++;
            hash *= 0x01000193; // FNV-1a prime
        }
    };
    
    // Helper lambda to hash a numeric value
    auto hashValue = [&hash](uint32_t value) {
        hash ^= value;
        hash *= 0x01000193;
    };
    
    // Hash all the values from temp_config
    // WiFi
    hashString(temp_config.wifi.ssid);
    hashString(temp_config.wifi.password);
    hashString(temp_config.wifi.hostname);
    
    // API
    hashString(temp_config.api.api_key);
    hashString(temp_config.api.read_url);
    hashString(temp_config.api.write_url);
    hashString(temp_config.api.upload_url);
    hashString(temp_config.api.config_url);
    hashValue(temp_config.api.timeout_ms);
    
    // Security
    hashString(temp_config.security.psk);
    
    // Firmware version
    hashString(temp_config.firmware_version);
    
    // Device
    hashValue(temp_config.device.slave_address);
    hashValue(temp_config.device.poll_interval_ms);
    hashValue(temp_config.device.upload_interval_ms);
    hashValue(temp_config.device.buffer_size);
    
    // Enabled parameters
    hashValue(temp_config.device.num_enabled_params);
    for (uint8_t i = 0; i < temp_config.device.num_enabled_params; ++i) {
        hashValue((uint32_t)temp_config.device.enabled_params[i]);
    }
    
    return hash;
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
