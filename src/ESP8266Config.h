#ifndef ESP8266_CONFIG_H
#define ESP8266_CONFIG_H

#include <Arduino.h>
#include <EEPROM.h>
#include <vector>
#include "ESP8266DataTypes.h"

#define MAX_POLLING_PARAMS 10

struct WiFiConfig
{
    char ssid[32];
    char password[64];
    char hostname[32];
};

struct APIConfig
{
    char api_key[128];
    char read_url[128];
    char write_url[128];
    char upload_url[128];
    char config_url[128];
    uint16_t timeout_ms;
};

struct DeviceConfig
{
    uint8_t slave_address;
    uint16_t poll_interval_ms;
    uint16_t upload_interval_ms;
    uint8_t buffer_size;
    ParameterType enabled_params[MAX_POLLING_PARAMS];
    uint8_t num_enabled_params;
};

struct SecurityConfig
{
    char psk[65]; // Pre-Shared Key (e.g., 64 hex characters for SHA-256)
    uint32_t nonce; // Anti-replay counter
};

struct BootStatusConfig
{
    bool ota_reboot_pending;     // Flag set before OTA reboot
    bool boot_success_reported;  // Flag to track if boot success was reported
    char last_boot_status[16];   // "success", "failure", "rebooting"
    char boot_error_message[64]; // Error details if any
};

struct ESP8266Config
{
    WiFiConfig wifi;
    APIConfig api;
    DeviceConfig device;
    SecurityConfig security;
    BootStatusConfig boot_status;
    char firmware_version[16]; // Firmware version string (e.g., "1.0.0")
    uint32_t magic; // For EEPROM validation
    uint32_t config_version; // Configuration version - increment when defaults change
};

class ConfigManager
{
public:
    ConfigManager();

    bool begin();
    bool loadConfig();
    bool saveConfig();
    void loadDefaults();

    // Getters
    const WiFiConfig &getWiFiConfig() const { return config_.wifi; }
    const APIConfig &getAPIConfig() const { return config_.api; }
    const DeviceConfig &getDeviceConfig() const { return config_.device; }
    const SecurityConfig &getSecurityConfig() const { return config_.security; }
    const BootStatusConfig &getBootStatusConfig() const { return config_.boot_status; }
    const char *getFirmwareVersion() const { return config_.firmware_version; }

    // Setters
    void setWiFiConfig(const char *ssid, const char *password, const char *hostname = "bitbots-ecoWatt");
    void setAPIConfig(const char *api_key, const char *read_url, const char *write_url, const char *upload_url = NULL, const char *config_url = NULL, uint16_t timeout_ms = 5000);
    void setDeviceConfig(uint8_t slave_addr, uint16_t poll_interval, uint16_t upload_interval, uint8_t buffer_size);
    void setFirmwareVersion(const char *version);
    void updatePollingConfig(uint16_t new_interval, const std::vector<ParameterType> &new_params);

    // Boot status management
    void setOTARebootFlag(bool pending);
    void setBootStatus(const char *status, const char *error_message = "");
    void markBootSuccessReported();
    bool needsBootStatusReport() const;

    uint32_t getNextNonce(); // Increments the nonce, saves it, and returns the new value

private:
    ESP8266Config config_;
    static const uint32_t CONFIG_MAGIC = 0xBEEFCAFE;
    static const int EEPROM_SIZE = sizeof(ESP8266Config);

    bool isConfigValid() const;
    uint32_t calculateDefaultsHash() const; // Calculate hash of current defaults
    void populateDefaults(ESP8266Config &cfg) const; // Helper to populate defaults
};

extern ConfigManager configManager;

#endif // ESP8266_CONFIG_H