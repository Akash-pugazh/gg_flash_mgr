/**
* @file gg_flash_mgr.h
* @brief ESP32 External Flash Memory Manager Component
* 
* This component provides efficient management of external SPI flash memory
* with automatic circular buffer behavior, RAM-efficient operations, and
* configurable storage limits.
* 
* Features:
* - External SPI flash support (W25Q128, etc.)
* - LittleFS filesystem integration
* - Circular buffer with automatic cleanup
* - RAM-efficient chunked operations
* - Configurable storage limits
* - Power-safe atomic operations
* 
* @date 2025
*/

#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
* @brief Flash manager configuration structure
*/
typedef struct {
    // SPI Flash Hardware Configuration
    int mosi_pin;           ///< MOSI pin number
    int miso_pin;           ///< MISO pin number  
    int sclk_pin;           ///< SCLK pin number
    int cs_pin;             ///< CS pin number
    int spi_host;           ///< SPI host (VSPI_HOST, HSPI_HOST)
    int freq_mhz;           ///< SPI frequency in MHz (default: 40)
    
    // Storage Configuration
    const char* mount_point;    ///< Mount point path (default: "/ext")
    const char* data_file;      ///< Data file name (default: "data.bin")
    const char* meta_file;      ///< Metadata file name (default: "meta.bin")
    const char* partition_label; ///< LittleFS partition label
    
    // Memory Limits
    uint32_t max_data_size;     ///< Maximum data storage size in bytes
    uint32_t chunk_buffer_size; ///< Chunk buffer size for operations (default: 4096)
    
    // Behavior Configuration
    bool format_on_init;        ///< Format filesystem on first initialization
    bool auto_cleanup;          ///< Enable automatic cleanup when storage is full
    float cleanup_threshold;    ///< Cleanup when storage exceeds this ratio (0.0-1.0)
    float cleanup_target;       ///< Target storage ratio after cleanup (0.0-1.0)
} flash_mgr_config_t;

/**
* @brief Data entry structure
*/
typedef struct __attribute__((packed)) {
    uint32_t timestamp;     ///< Entry timestamp
    uint32_t id;           ///< Unique entry ID
    uint8_t type;          ///< Data type identifier
    uint8_t unit;          ///< Data unit identifier
    int32_t value_x1000;   ///< Value multiplied by 1000 for precision
    uint8_t reserved[2];   ///< Reserved bytes for alignment
} flash_mgr_entry_t;

/**
* @brief Flash manager status information
*/
typedef struct {
    uint32_t total_entries;     ///< Total entries ever written
    uint32_t active_entries;    ///< Currently active entries
    uint32_t deleted_entries;   ///< Total entries deleted
    uint32_t free_space_bytes;  ///< Available storage space in bytes
    uint32_t used_space_bytes;  ///< Used storage space in bytes
    bool initialized;           ///< Whether manager is initialized
} flash_mgr_status_t;

/**
* @brief Get default configuration
* 
* @return Default configuration structure
*/
flash_mgr_config_t flash_mgr_get_default_config(void);

/**
* @brief Initialize flash manager with configuration
* 
* @param config Configuration structure
* @return ESP_OK on success, error code otherwise
*/
esp_err_t flash_mgr_init(const flash_mgr_config_t* config);

/**
* @brief Deinitialize flash manager
* 
* @return ESP_OK on success, error code otherwise
*/
esp_err_t flash_mgr_deinit(void);

/**
* @brief Check if flash manager is initialized
* 
* @return true if initialized, false otherwise
*/
bool flash_mgr_is_initialized(void);

/**
* @brief Append data entry to flash storage
* 
* @param type Data type identifier
* @param unit Data unit identifier  
* @param value_x1000 Value multiplied by 1000
* @return ESP_OK on success, error code otherwise
*/
esp_err_t flash_mgr_append(uint8_t type, uint8_t unit, int32_t value_x1000);

/**
* @brief Append data entry with custom timestamp
* 
* @param timestamp Custom timestamp
* @param type Data type identifier
* @param unit Data unit identifier
* @param value_x1000 Value multiplied by 1000
* @return ESP_OK on success, error code otherwise
*/
esp_err_t flash_mgr_append_with_timestamp(uint32_t timestamp, uint8_t type, uint8_t unit, int32_t value_x1000);

/**
* @brief Read entries in chunks (oldest first)
* 
* @param buffer Buffer to store read entries
* @param max_entries Maximum number of entries to read
* @param entries_read[out] Number of entries actually read
* @return ESP_OK on success, error code otherwise
*/
esp_err_t flash_mgr_read_chunk(flash_mgr_entry_t* buffer, uint32_t max_entries, uint32_t* entries_read);

/**
* @brief Delete processed entries from storage
* 
* This function actually removes data from the file to free up space.
* Uses RAM-efficient chunked operations to handle large files.
* 
* @param count Number of entries to delete (from oldest)
* @return ESP_OK on success, error code otherwise
*/
esp_err_t flash_mgr_delete_processed(uint32_t count);

/**
* @brief Get current storage status
* 
* @param status[out] Status information structure
* @return ESP_OK on success, error code otherwise
*/
esp_err_t flash_mgr_get_status(flash_mgr_status_t* status);

/**
* @brief Force cleanup of old entries
* 
* @param target_entries Target number of entries to keep
* @return ESP_OK on success, error code otherwise
*/
esp_err_t flash_mgr_cleanup(uint32_t target_entries);

/**
* @brief Format the storage (WARNING: Deletes all data)
* 
* @return ESP_OK on success, error code otherwise
*/
esp_err_t flash_mgr_format(void);

/**
* @brief Get filesystem information
* 
* @param total_bytes[out] Total filesystem size
* @param used_bytes[out] Used filesystem space
* @return ESP_OK on success, error code otherwise
*/
esp_err_t flash_mgr_get_fs_info(size_t* total_bytes, size_t* used_bytes);

#ifdef __cplusplus
}
#endif
