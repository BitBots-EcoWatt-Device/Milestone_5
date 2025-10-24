#ifndef ESP8266_FOTA_H
#define ESP8266_FOTA_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Hash.h>
#include <SHA256.h>
#include <Updater.h>
struct FOTAManifest
{
    String version;
    uint32_t size;
    String hash;
    uint16_t chunk_size;
    uint16_t total_chunks;
    bool valid;
    
    void reset()
    {
        version = "";
        size = 0;
        hash = "";
        chunk_size = 0;
        total_chunks = 0;
        valid = false;
    }
};

struct FOTAChunk
{
    uint16_t chunk_number;
    String data;           // Base64 encoded chunk data
    String mac;            // HMAC of chunk data
    uint16_t total_chunks;
    bool valid;
    
    void reset()
    {
        chunk_number = 0;
        data = "";
        mac = "";
        total_chunks = 0;
        valid = false;
    }
};

class ESP8266FOTA
{
public:
    ESP8266FOTA();
    
    // Initialization
    void begin();
    void reset();
    
    // Status queries
    bool isUpdateInProgress() const { return update_in_progress_; }
    bool isManifestReceived() const { return manifest_received_; }
    bool hasLastChunkInfo() const { return total_chunks_received_ > 0; }
    uint16_t getLastChunkReceived() const { return last_chunk_received_; }
    bool isLastChunkVerified() const { return chunk_verified_; }
    uint16_t getTotalChunksReceived() const { return total_chunks_received_; }
    const FOTAManifest& getManifest() const { return manifest_; }
    
    // Progress information
    float getProgress() const;
    bool isComplete() const;
    
    // Polling optimization
    unsigned long getRecommendedPollingInterval() const;
    bool needsFastPolling() const { return update_in_progress_ && !isComplete(); }
    bool justStartedUpdate() const;
    void clearJustStartedFlag() { update_just_started_ = false; }
    void markManifestAckSent() { manifest_ack_sent_ = true; }
    
    // FOTA processing - works with secure wrapped JSON
    bool processSecureFOTAResponse(const String &secureResponse);
    bool processPlainFOTAResponse(const JsonObject &fotaObj);
    
    // Add FOTA status to config request
    void addStatusToConfigRequest(JsonObject &requestObj);
    
    // Status display
    void printStatus() const;
    void printDetailedStatus() const;

private:
    enum class HashType
    {
        NONE,
        MD5,
        SHA256
    };

    // State variables
    FOTAManifest manifest_;
    uint16_t last_chunk_received_;
    bool chunk_verified_;
    bool update_in_progress_;
    bool manifest_received_;
    bool update_just_started_;
    bool manifest_ack_sent_;
    uint32_t chunks_received_bitmap_[16]; // Bitmap to track received chunks (supports up to 512 chunks)
    uint16_t total_chunks_received_;
    bool update_session_initialized_;
    uint32_t bytes_written_;
    HashType hash_type_;
    SHA256 streaming_sha256_;
    MD5Builder streaming_md5_;
    bool hash_initialized_;
    
    // Internal processing
    bool processManifest(const JsonObject &fota);
    bool processChunk(const JsonObject &fota);
    
    // Chunk management
    void markChunkReceived(uint16_t chunk_num);
    bool isChunkReceived(uint16_t chunk_num) const;
    uint16_t getNextMissingChunk() const;
    
    // Storage and verification
    bool verifyChunkMAC(const String &data, const String &mac);
    String calculateChunkHMAC(const char *psk, const String &base64Data);
    bool beginStreamingUpdate();
    bool finalizeStreamingUpdate();
    void abortStreamingUpdate();
    
    // Validation
    bool validateManifest(const FOTAManifest &manifest) const;
    bool validateChunk(uint16_t chunk_number, const String &data, const String &mac) const;
    
    // File system management
    void cleanupPreviousFOTA();
};

#endif // ESP8266_FOTA_H