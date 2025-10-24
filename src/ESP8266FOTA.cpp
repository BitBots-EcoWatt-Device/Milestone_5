#include "ESP8266FOTA.h"
#include "ESP8266Config.h"
#include "ESP8266Security.h"
#include <SHA256.h>

// FOTA can trigger an immediate config request
extern volatile bool configRequestPending;
// Signal that final ACK should be sent by main loop
extern volatile bool finalAckPending;

ESP8266FOTA::ESP8266FOTA()
{
    reset();
}

void ESP8266FOTA::begin()
{
    Serial.println("[FOTA] Initializing FOTA system...");
    cleanupPreviousFOTA();
    reset();
    Serial.println("[FOTA] FOTA system initialized");
}

void ESP8266FOTA::reset()
{
    manifest_.reset();
    last_chunk_received_ = 0;
    chunk_verified_ = false;
    update_in_progress_ = false;
    update_just_started_ = false;
    manifest_ack_sent_ = false;
    manifest_received_ = false;
    total_chunks_received_ = 0;
    memset(chunks_received_bitmap_, 0, sizeof(chunks_received_bitmap_));
    update_session_initialized_ = false;
    bytes_written_ = 0;
    hash_type_ = HashType::NONE;
    hash_initialized_ = false;
    streaming_sha256_.reset();
}

float ESP8266FOTA::getProgress() const
{
    if (!manifest_.valid || manifest_.total_chunks == 0)
        return 0.0f;

    return (float)total_chunks_received_ / (float)manifest_.total_chunks * 100.0f;
}

bool ESP8266FOTA::isComplete() const
{
    return manifest_.valid &&
           total_chunks_received_ == manifest_.total_chunks &&
           manifest_.total_chunks > 0;
}

unsigned long ESP8266FOTA::getRecommendedPollingInterval() const
{
    if (update_in_progress_ && !isComplete())
    {
        return 500; // Very fast polling for chunks
    }
    else
    {
        // Normal polling when no FOTA is active
        return 5000;
    }
}

bool ESP8266FOTA::justStartedUpdate() const
{
    return update_just_started_;
}

bool ESP8266FOTA::processSecureFOTAResponse(const String &secureResponse)
{
    // Parse the secure wrapper
    StaticJsonDocument<2048> secureDoc;
    DeserializationError error = deserializeJson(secureDoc, secureResponse);

    if (error)
    {
        Serial.print("[FOTA] Error parsing secure response: ");
        Serial.println(error.c_str());
        return false;
    }

    // Check if this is a secure wrapper with nonce, payload, and mac
    if (!secureDoc.containsKey("nonce") || !secureDoc.containsKey("payload") || !secureDoc.containsKey("mac"))
    {
        // Assume it's plain JSON, try to process directly
        if (secureDoc.containsKey("fota"))
        {
            return processPlainFOTAResponse(secureDoc["fota"]);
        }
        Serial.println("[FOTA] No secure wrapper or FOTA data found");
        return false;
    }

    // Extract secure wrapper components
    uint32_t nonce = secureDoc["nonce"];
    String encodedPayload = secureDoc["payload"];
    String receivedMac = secureDoc["mac"];

    // Verify MAC first
    const char *psk = configManager.getSecurityConfig().psk;
    String calculatedMac = ESP8266Security::calculateHMAC(psk, nonce, encodedPayload);

    if (calculatedMac != receivedMac)
    {
        Serial.println("[FOTA] Error: MAC verification failed for secure FOTA response");
        return false;
    }

    // Decode base64 payload
    unsigned int decodedLength = ESP8266Security::getBase64DecodedLength(encodedPayload);
    unsigned char *decodedBuffer = new unsigned char[decodedLength + 1];

    ESP8266Security::decodeBase64(encodedPayload, decodedBuffer);
    decodedBuffer[decodedLength] = '\0';

    String decodedPayload = String((char *)decodedBuffer);
    delete[] decodedBuffer;

    // Parse the decoded payload
    StaticJsonDocument<2048> payloadDoc;
    error = deserializeJson(payloadDoc, decodedPayload);

    if (error)
    {
        Serial.print("[FOTA] Error parsing decoded payload: ");
        Serial.println(error.c_str());
        return false;
    }

    // Process FOTA data if present
    if (payloadDoc.containsKey("fota"))
    {
        return processPlainFOTAResponse(payloadDoc["fota"]);
    }

    // No FOTA data in this response
    return true;
}

bool ESP8266FOTA::processPlainFOTAResponse(const JsonObject &fotaObj)
{
    Serial.print("[FOTA] Processing FOTA message: ");
    serializeJson(fotaObj, Serial);
    Serial.println();

    // Check if this is a manifest (first FOTA message)
    if (fotaObj.containsKey("manifest"))
    {
        if (processManifest(fotaObj))
        {
            Serial.println("[FOTA] Manifest processed successfully");
            return true;
        }
        else
        {
            Serial.println("[FOTA] Failed to process manifest");
            return false;
        }
    }
    // Check if this is a chunk message
    else if (fotaObj.containsKey("chunk_number"))
    {
        if (processChunk(fotaObj))
        {
            Serial.print("[FOTA] Chunk ");
            Serial.print(fotaObj["chunk_number"].as<int>());
            Serial.println(" processed successfully");
            return true;
        }
        else
        {
            Serial.print("[FOTA] Failed to process chunk ");
            Serial.println(fotaObj["chunk_number"].as<int>());
            return false;
        }
    }
    else
    {
        Serial.println("[FOTA] Unknown FOTA message format");
        return false;
    }
}

bool ESP8266FOTA::processManifest(const JsonObject &fota)
{
    if (!fota.containsKey("manifest"))
    {
        Serial.println("[FOTA] Error: No manifest in FOTA message");
        return false;
    }

    JsonObject manifest = fota["manifest"];

    // Parse manifest fields
    String version = manifest["version"] | "";
    uint32_t size = manifest["size"] | 0;
    String hash = manifest["hash"] | "";
    uint16_t chunk_size = manifest["chunk_size"] | 0;
    uint16_t total_chunks = manifest["total_chunks"] | 0;

    // Create temporary manifest for validation
    FOTAManifest tempManifest;
    tempManifest.version = version;
    tempManifest.size = size;
    tempManifest.hash = hash;
    tempManifest.chunk_size = chunk_size;
    tempManifest.total_chunks = total_chunks;
    tempManifest.valid = true;

    // Validate manifest
    if (!validateManifest(tempManifest))
    {
        Serial.println("[FOTA] Error: Manifest validation failed");
        return false;
    }

    // Check if this is a different version than current
    if (version.equals(configManager.getFirmwareVersion()))
    {
        Serial.println("[FOTA] Warning: Manifest version same as current firmware");
        return false;
    }

    // Determine hash type based on manifest hash length
    size_t hashLength = hash.length();
    if (hashLength == 32)
    {
        hash_type_ = HashType::MD5;
        Serial.println("[FOTA] Manifest hash algorithm: MD5");
    }
    else if (hashLength == 64)
    {
        hash_type_ = HashType::SHA256;
        Serial.println("[FOTA] Manifest hash algorithm: SHA256");
    }
    else
    {
        Serial.print("[FOTA] Error: Unsupported manifest hash length: ");
        Serial.println(hashLength);
        return false;
    }

    // Store manifest
    abortStreamingUpdate();
    manifest_ = tempManifest;
    manifest_received_ = true;
    update_in_progress_ = true;
    update_just_started_ = true; // Flag that update just started for immediate polling
    last_chunk_received_ = 0;
    chunk_verified_ = true;
    total_chunks_received_ = 0;
    memset(chunks_received_bitmap_, 0, sizeof(chunks_received_bitmap_));
    bytes_written_ = 0;
    hash_initialized_ = false;
    update_session_initialized_ = false;

    Serial.println("[FOTA] Manifest processed successfully:");
    Serial.print("  Version: ");
    Serial.println(version);
    Serial.print("  Size: ");
    Serial.println(size);
    Serial.print("  Hash: ");
    Serial.println(hash);
    Serial.print("  Chunk Size: ");
    Serial.println(chunk_size);
    Serial.print("  Total Chunks: ");
    Serial.println(total_chunks);

    return true;
}

bool ESP8266FOTA::processChunk(const JsonObject &fota)
{
    if (!update_in_progress_ || !manifest_.valid)
    {
        Serial.println("[FOTA] Error: No FOTA update in progress or invalid manifest");
        return false;
    }

    // Parse chunk fields
    uint16_t chunk_number = fota["chunk_number"] | 0;
    String data = fota["data"] | "";
    String mac = fota["mac"] | "";
    uint16_t total_chunks = fota["total_chunks"] | 0;

    // Validate chunk
    if (!validateChunk(chunk_number, data, mac))
    {
        chunk_verified_ = false;
        return false;
    }

    // Verify total chunks matches manifest
    if (total_chunks != manifest_.total_chunks)
    {
        Serial.println("[FOTA] Error: Chunk total_chunks mismatch with manifest");
        chunk_verified_ = false;
        return false;
    }

    // Check if we already received this chunk
    if (isChunkReceived(chunk_number))
    {
        Serial.print("[FOTA] Warning: Chunk ");
        Serial.print(chunk_number);
        Serial.println(" already received, skipping");
        last_chunk_received_ = chunk_number;
        chunk_verified_ = true;
        return true;
    }

    // Verify chunk MAC
    if (!verifyChunkMAC(data, mac))
    {
        Serial.print("[FOTA] Error: Chunk ");
        Serial.print(chunk_number);
        Serial.println(" MAC verification failed");
        chunk_verified_ = false;
        return false;
    }

    // Ensure streaming update session is ready
    if (!beginStreamingUpdate())
    {
        Serial.println("[FOTA] Error: Failed to initialize OTA streaming session");
        chunk_verified_ = false;
        return false;
    }

    // Decode base64 data
    unsigned int decodedLength = ESP8266Security::getBase64DecodedLength(data);
    if (decodedLength == 0)
    {
        Serial.println("[FOTA] Error: Invalid base64 data length");
        abortStreamingUpdate();
        chunk_verified_ = false;
        return false;
    }

    unsigned char *decodedBuffer = new unsigned char[decodedLength];
    if (!decodedBuffer)
    {
        Serial.println("[FOTA] Error: Failed to allocate memory for decoded chunk");
        abortStreamingUpdate();
        chunk_verified_ = false;
        return false;
    }

    ESP8266Security::decodeBase64(data, decodedBuffer);

    if (hash_initialized_)
    {
        if (hash_type_ == HashType::SHA256)
        {
            streaming_sha256_.update(decodedBuffer, decodedLength);
        }
        else if (hash_type_ == HashType::MD5)
        {
            streaming_md5_.add(decodedBuffer, decodedLength);
        }
    }

    size_t written = Update.write(decodedBuffer, decodedLength);
    delete[] decodedBuffer;

    if (written != decodedLength)
    {
        Serial.print("[FOTA] Error: Failed to write chunk to flash. Expected ");
        Serial.print(decodedLength);
        Serial.print(" bytes, wrote ");
        Serial.println(written);
        Update.printError(Serial);
        abortStreamingUpdate();
        chunk_verified_ = false;
        return false;
    }

    bytes_written_ += written;
    if (bytes_written_ > manifest_.size)
    {
        Serial.println("[FOTA] Error: Received more data than manifest size allows");
        abortStreamingUpdate();
        chunk_verified_ = false;
        return false;
    }

    // Update status
    markChunkReceived(chunk_number);
    last_chunk_received_ = chunk_number;
    chunk_verified_ = true;

    Serial.print("[FOTA] Chunk ");
    Serial.print(chunk_number);
    Serial.print(" stored successfully (");
    Serial.print(total_chunks_received_);
    Serial.print("/");
    Serial.print(manifest_.total_chunks);
    Serial.println(" received)");

    // Trigger immediate config request to fetch next chunk
    configRequestPending = true;

    // Check if all chunks received
    if (isComplete())
    {
        Serial.println("[FOTA] All chunks received!");

        // Request the main loop to send the final ACK via the normal config request path.
        // Avoid performing HTTP or reboot directly inside FOTA processing.
        finalAckPending = true;
        configRequestPending = true;

        Serial.println("[FOTA] Final ACK requested via configRequestPending; will finalize after successful ACK.");
        // Do not finalize/reboot here — main loop will finalize after the server has received the ACK.

        if (finalizeStreamingUpdate())
        {
            Serial.println("[FOTA] Firmware validation successful - Ready for installation!");
            Serial.println("[FOTA] Rebooting to apply new firmware...");

            // Mark that we're about to reboot for OTA update
            configManager.setOTARebootFlag(true);

            delay(1000);   // Give time for serial output
            ESP.restart(); // Apply new firmware
        }
        else
        {
            Serial.println("[FOTA] Error: Firmware finalization failed");
        }
    }

    return true;
}

void ESP8266FOTA::markChunkReceived(uint16_t chunk_num)
{
    if (chunk_num < 512) // Max supported chunks
    {
        uint8_t byte_index = chunk_num / 32;
        uint8_t bit_index = chunk_num % 32;
        if (!(chunks_received_bitmap_[byte_index] & (1UL << bit_index)))
        {
            chunks_received_bitmap_[byte_index] |= (1UL << bit_index);
            total_chunks_received_++;
        }
    }
}

bool ESP8266FOTA::isChunkReceived(uint16_t chunk_num) const
{
    if (chunk_num >= 512)
        return false;
    uint8_t byte_index = chunk_num / 32;
    uint8_t bit_index = chunk_num % 32;
    return (chunks_received_bitmap_[byte_index] & (1UL << bit_index)) != 0;
}

uint16_t ESP8266FOTA::getNextMissingChunk() const
{
    if (!manifest_.valid || manifest_.total_chunks == 0)
        return 0;

    // Find the first missing chunk (1-indexed)
    for (uint16_t chunk = 1; chunk <= manifest_.total_chunks; chunk++)
    {
        if (!isChunkReceived(chunk))
        {
            return chunk;
        }
    }

    return 0; // All chunks received
}

void ESP8266FOTA::addStatusToConfigRequest(JsonObject &requestObj)
{
    Serial.print("[DEBUG] addStatusToConfigRequest called - update_in_progress_: ");
    Serial.print(update_in_progress_ ? "true" : "false");
    Serial.print(", manifest_ack_sent_: ");
    Serial.print(manifest_ack_sent_ ? "true" : "false");
    Serial.print(", last_chunk_received_: ");
    Serial.println(last_chunk_received_);

    // Add FOTA status if there's an ongoing update
    if (update_in_progress_)
    {
        if (manifest_received_ && !manifest_ack_sent_)
        {
            // Step 1: Just received manifest, send ONLY acknowledgment
            // This tells server to change status from "manifest_sent" to "active"
            JsonObject fotaStatusObj = requestObj.createNestedObject("fota_status");
            fotaStatusObj["manifest_ack"] = true;
            manifest_ack_sent_ = true; // Mark that we've sent the ACK
            Serial.println("[FOTA] Sending manifest acknowledgment (server will change status to 'active')");
        }
        else if (total_chunks_received_ > 0)
        {
            // Step 2+: Normal chunk acknowledgment after server status is "active"
            JsonObject fotaStatusObj = requestObj.createNestedObject("fota_status");

            // Correct format: chunk_received (0-indexed) and verified
            fotaStatusObj["chunk_received"] = last_chunk_received_;
            fotaStatusObj["verified"] = chunk_verified_;

            Serial.print("[FOTA] Sending chunk_received: ");
            Serial.print(last_chunk_received_);
            Serial.print(", verified: ");
            Serial.println(chunk_verified_ ? "true" : "false");
        }
        else
        {
            Serial.println("[DEBUG] FOTA in progress but no manifest ACK or chunk to send");
        }
        // Note: Server automatically sends next chunk after receiving acknowledgment
        // No explicit chunk requests needed - server manages the flow
    }
    else
    {
        Serial.println("[DEBUG] No FOTA in progress - no FOTA status added to request");
    }
}

bool ESP8266FOTA::validateManifest(const FOTAManifest &manifest) const
{
    // Check basic fields
    if (manifest.version.isEmpty() ||
        manifest.size == 0 ||
        manifest.hash.isEmpty() ||
        manifest.chunk_size == 0 ||
        manifest.total_chunks == 0)
    {
        Serial.println("[FOTA] Error: Invalid manifest data - missing required fields");
        return false;
    }

    // Check reasonable size limits (e.g., max 4MB firmware)
    if (manifest.size > 4 * 1024 * 1024)
    {
        Serial.println("[FOTA] Error: Firmware size too large");
        return false;
    }

    // Check chunk size is reasonable (e.g., 512-4096 bytes)
    if (manifest.chunk_size < 512 || manifest.chunk_size > 4096)
    {
        Serial.println("[FOTA] Error: Invalid chunk size");
        return false;
    }

    // Check total chunks limit (max 512 supported by bitmap)
    if (manifest.total_chunks > 512)
    {
        Serial.println("[FOTA] Error: Too many chunks (max 512 supported)");
        return false;
    }

    // Verify size/chunk calculation
    uint32_t expectedSize = (manifest.total_chunks - 1) * manifest.chunk_size +
                            (manifest.size % manifest.chunk_size == 0 ? manifest.chunk_size : manifest.size % manifest.chunk_size);
    if (abs((int32_t)(expectedSize - manifest.size)) > (int32_t)manifest.chunk_size)
    {
        Serial.println("[FOTA] Error: Size/chunk calculation mismatch");
        return false;
    }

    return true;
}

bool ESP8266FOTA::validateChunk(uint16_t chunk_number, const String &data, const String &mac) const
{
    // Validate chunk data
    if (data.isEmpty() || mac.isEmpty())
    {
        Serial.println("[FOTA] Error: Invalid chunk data - missing data or MAC");
        return false;
    }

    // Check if chunk is within valid range
    if (chunk_number >= manifest_.total_chunks)
    {
        Serial.println("[FOTA] Error: Chunk number out of range");
        return false;
    }

    return true;
}

String ESP8266FOTA::calculateChunkHMAC(const char *psk, const String &base64Data)
{
    // Calculate HMAC exactly like the server:
    // hmac.new(device_psk.encode('utf-8'), chunk_data.encode('utf-8'), hashlib.sha256).hexdigest()
    //
    // This means: HMAC-SHA256(key=psk, message=base64Data) without any nonce prefix

    SHA256 sha256;

    // Reset HMAC with the PSK key
    sha256.resetHMAC(psk, strlen(psk));

    // Update HMAC with the base64 chunk data directly (as UTF-8 bytes)
    sha256.update(base64Data.c_str(), base64Data.length());

    // Finalize HMAC calculation
    uint8_t mac_result[SHA256::HASH_SIZE];
    sha256.finalizeHMAC(psk, strlen(psk), mac_result, sizeof(mac_result));

    // Convert to hex string
    String mac_hex = "";
    mac_hex.reserve(sizeof(mac_result) * 2 + 1);

    for (int i = 0; i < sizeof(mac_result); i++)
    {
        char hex_buf[3];
        sprintf(hex_buf, "%02x", mac_result[i]);
        mac_hex += hex_buf;
    }

    return mac_hex;
}

bool ESP8266FOTA::verifyChunkMAC(const String &data, const String &mac)
{
    if (mac.isEmpty())
    {
        Serial.println("[FOTA] Error: Empty MAC");
        return false;
    }

    // Calculate HMAC of the chunk data using PSK from configuration
    const char *psk = configManager.getSecurityConfig().psk;
    if (strlen(psk) == 0)
    {
        Serial.println("[FOTA] Error: No PSK configured for MAC verification");
        return false;
    }

    // Server calculates HMAC directly on base64 chunk data (as UTF-8 bytes)
    // Server: hmac.new(device_psk.encode('utf-8'), chunk_data.encode('utf-8'), hashlib.sha256).hexdigest()
    // We need to calculate HMAC directly on the base64 string without nonce prefix
    String calculatedMac = calculateChunkHMAC(psk, data);

    Serial.print("[FOTA] Expected MAC: ");
    Serial.println(mac);
    Serial.print("[FOTA] Calculated MAC: ");
    Serial.println(calculatedMac);

    bool macValid = calculatedMac.equalsIgnoreCase(mac);
    if (!macValid)
    {
        Serial.println("[FOTA] Error: MAC verification failed");
        return false;
    }

    Serial.println("[FOTA] MAC verification successful");
    return true;
}

void ESP8266FOTA::printStatus() const
{
    if (update_in_progress_)
    {
        Serial.print("FOTA Update: IN PROGRESS (");
        if (manifest_.valid)
        {
            Serial.print(total_chunks_received_);
            Serial.print("/");
            Serial.print(manifest_.total_chunks);
            Serial.print(" chunks, v");
            Serial.print(manifest_.version);
        }
        Serial.println(")");
    }
    else
    {
        Serial.println("FOTA Update: IDLE");
    }
}

void ESP8266FOTA::printDetailedStatus() const
{
    Serial.println("[FOTA] FOTA Status:");
    Serial.print("  Update in progress: ");
    Serial.println(update_in_progress_ ? "Yes" : "No");
    Serial.print("  Manifest received: ");
    Serial.println(manifest_received_ ? "Yes" : "No");

    if (manifest_.valid)
    {
        Serial.print("  Target version: ");
        Serial.println(manifest_.version);
        Serial.print("  Firmware size: ");
        Serial.println(manifest_.size);
        Serial.print("  Total chunks: ");
        Serial.println(manifest_.total_chunks);
        Serial.print("  Chunks received: ");
        Serial.print(total_chunks_received_);
        Serial.print("/");
        Serial.println(manifest_.total_chunks);

        if (total_chunks_received_ > 0)
        {
            Serial.print("  Progress: ");
            Serial.print(getProgress(), 1);
            Serial.println("%");
        }

        if (isComplete())
        {
            Serial.println("  Status: COMPLETE - Ready for installation");
        }
    }

    Serial.print("  Last chunk received: ");
    Serial.println(last_chunk_received_);
    Serial.print("  Last chunk verified: ");
    Serial.println(chunk_verified_ ? "Yes" : "No");
}

void ESP8266FOTA::cleanupPreviousFOTA()
{
    Serial.println("[FOTA] Resetting previous FOTA state...");

    // ESP8266 Update class doesn't have abort() method
    // Just reset our tracking state
    update_session_initialized_ = false;
    bytes_written_ = 0;
    hash_initialized_ = false;
    streaming_sha256_.reset();
    streaming_md5_.begin();
}

bool ESP8266FOTA::beginStreamingUpdate()
{
    if (update_session_initialized_)
    {
        return true;
    }

    if (!manifest_.valid || manifest_.size == 0)
    {
        Serial.println("[FOTA] Error: Cannot start OTA - invalid manifest");
        return false;
    }

    if (!Update.begin(manifest_.size))
    {
        Serial.println("[FOTA] Error: Update.begin failed");
        Update.printError(Serial);
        return false;
    }

    update_session_initialized_ = true;
    bytes_written_ = 0;

    if (!hash_initialized_)
    {
        if (hash_type_ == HashType::SHA256)
        {
            streaming_sha256_.reset();
            hash_initialized_ = true;
        }
        else if (hash_type_ == HashType::MD5)
        {
            streaming_md5_.begin();
            hash_initialized_ = true;
        }
        else
        {
            hash_initialized_ = false;
        }
    }

    Serial.println("[FOTA] OTA streaming session started");
    return true;
}

bool ESP8266FOTA::finalizeStreamingUpdate()
{
    Serial.println("[FOTA] DEBUG: Entering finalizeStreamingUpdate()");

    if (!update_session_initialized_)
    {
        Serial.println("[FOTA] Error: No OTA session to finalize");
        return false;
    }

    Serial.print("[FOTA] DEBUG: Checking bytes - written: ");
    Serial.print(bytes_written_);
    Serial.print(", expected: ");
    Serial.println(manifest_.size);

    if (bytes_written_ != manifest_.size)
    {
        Serial.print("[FOTA] Error: Written bytes (");
        Serial.print(bytes_written_);
        Serial.print(") do not match manifest size (");
        Serial.print(manifest_.size);
        Serial.println(")");
        // Cannot abort on ESP8266, just reset state
        update_session_initialized_ = false;
        hash_initialized_ = false;
        streaming_sha256_.reset();
        streaming_md5_.begin();
        return false;
    }

    String calculatedHash = "";

    if (hash_type_ == HashType::SHA256)
    {
        uint8_t hash[SHA256::HASH_SIZE];
        streaming_sha256_.finalize(hash, sizeof(hash));
        hash_initialized_ = false;

        calculatedHash.reserve(SHA256::HASH_SIZE * 2);
        for (int i = 0; i < SHA256::HASH_SIZE; i++)
        {
            char hex[3];
            sprintf(hex, "%02x", hash[i]);
            calculatedHash += hex;
        }
    }
    else if (hash_type_ == HashType::MD5)
    {
        streaming_md5_.calculate();
        hash_initialized_ = false;
        calculatedHash = streaming_md5_.toString();
    }
    else
    {
        Serial.println("[FOTA] Warning: No hash algorithm selected; skipping validation");
        hash_initialized_ = false;
    }

    Serial.print("[FOTA] Expected hash: ");
    Serial.println(manifest_.hash);
    Serial.print("[FOTA] Calculated hash: ");
    Serial.println(calculatedHash);

    if (calculatedHash.length() > 0 && !calculatedHash.equalsIgnoreCase(manifest_.hash))
    {
        Serial.println("[FOTA] Error: Firmware hash validation failed");
        // Cannot abort on ESP8266, just reset state
        update_session_initialized_ = false;
        streaming_sha256_.reset();
        streaming_md5_.begin();
        return false;
    }

    if (!Update.end())
    {
        Serial.println("[FOTA] Error: Update.end failed");
        Update.printError(Serial);
        // Cannot abort on ESP8266, just reset state
        update_session_initialized_ = false;
        streaming_sha256_.reset();
        streaming_md5_.begin();
        return false;
    }

    if (!Update.isFinished())
    {
        Serial.println("[FOTA] Error: Update not finished after end");
        // Cannot abort on ESP8266, just reset state
        update_session_initialized_ = false;
        streaming_sha256_.reset();
        streaming_md5_.begin();
        return false;
    }

    Serial.println("[FOTA] OTA streaming session finalized successfully");
    update_session_initialized_ = false;
    return true;
}

void ESP8266FOTA::abortStreamingUpdate()
{
    // ESP8266 Update class doesn't have abort() method
    // Just reset our tracking state
    update_session_initialized_ = false;
    bytes_written_ = 0;
    hash_initialized_ = false;
    streaming_sha256_.reset();
    streaming_md5_.begin();
}