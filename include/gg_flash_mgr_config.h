/**
* @file gg_flash_mgr.h
* @brief ESP32 External Flash Memory Manager Component
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
    // SPI Flash Pin Configuration
    int mosi_pin;
    int miso_pin;
    int sclk_pin;
    int cs_pin;
    int spi_host;
    int freq_mhz;
    
    // Storage Configuration
    const char* mount_point;
    const char* partition_label;
    const char* data_file;
    const char* meta_file;

    // Memory Limits
    uint32_t max_data_size; // How much data storage in the data file in bytes
    uint32_t chunk_buffer_size; // Max buffer in ram for holding data from flash (default: 4096)
    
    // Behavior Configuration
    bool format_on_init;        // Format filesystem on first initialization
    bool auto_cleanup;          // Enable automatic cleanup when storage is full
    float cleanup_threshold;    // Cleanup when storage exceeds this ratio (0.0-1.0)
    float cleanup_target;       // Target storage ratio after cleanup (0.0-1.0)
} flash_mgr_config_t;

/**
* @brief Data entry structure to stored under the data file
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
* @param count Number of entries to delete (from oldest probably start of file)
* @return ESP_OK on success, error code otherwise
*/
esp_err_t flash_mgr_delete(uint32_t count);

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
* @param target_entries Target number of entries to keep (probably end of file)
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

// =============================================================================
// UTILITY FUNCTIONS - STANDALONE FILE/DIRECTORY OPERATIONS
// =============================================================================

/**
 * @brief File information structure
 */
typedef struct {
    size_t size;        ///< File size in bytes
    time_t mtime;       ///< Last modification time
    bool is_directory;  ///< True if this is a directory
} flash_mgr_file_info_t;

/**
 * @brief Directory listing callback function
 * @param path Full path of the file/directory
 * @param info File information
 * @param user_data User-provided data pointer
 * @return true to continue listing, false to stop
 */
typedef bool (*flash_mgr_dir_callback_t)(const char* path, const flash_mgr_file_info_t* info, void* user_data);

// Directory Operations
/**
 * @brief Create a directory (supports nested directories)
 * @param path Directory path (e.g., "/ext/logs/sensors")
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t flash_mgr_util_mkdir(const char* path);

/**
 * @brief Remove a directory (recursive removal supported)
 * @param path Directory path to remove
 * @param recursive If true, removes directory and all contents
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t flash_mgr_util_rmdir(const char* path, bool recursive);

/**
 * @brief Check if directory exists
 * @param path Directory path to check
 * @return true if directory exists, false otherwise
 */
bool flash_mgr_util_dir_exists(const char* path);

/**
 * @brief List directory contents
 * @param path Directory path to list
 * @param callback Callback function called for each entry
 * @param user_data User data passed to callback
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t flash_mgr_util_list_dir(const char* path, flash_mgr_dir_callback_t callback, void* user_data);

// File Operations
/**
 * @brief Write data to file (creates directories if needed)
 * @param filepath Full file path
 * @param data Data to write
 * @param size Size of data in bytes
 * @param append If true, append to file; if false, overwrite
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t flash_mgr_util_write_file(const char* filepath, const void* data, size_t size, bool append);

/**
 * @brief Read entire file into buffer
 * @param filepath Full file path
 * @param buffer[out] Buffer to store data (allocated by function)
 * @param size[out] Size of data read
 * @return ESP_OK on success, error code otherwise
 * @note Caller must free the returned buffer
 */
esp_err_t flash_mgr_util_read_file(const char* filepath, void** buffer, size_t* size);

/**
 * @brief Write text string to file
 * @param filepath Full file path
 * @param text Text string to write
 * @param append If true, append to file; if false, overwrite
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t flash_mgr_util_write_text(const char* filepath, const char* text, bool append);

/**
 * @brief Read text file into string
 * @param filepath Full file path
 * @param text[out] Text string (allocated by function)
 * @return ESP_OK on success, error code otherwise
 * @note Caller must free the returned string
 */
esp_err_t flash_mgr_util_read_text(const char* filepath, char** text);

/**
 * @brief Delete a file
 * @param filepath Full file path
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t flash_mgr_util_delete_file(const char* filepath);

/**
 * @brief Check if file exists
 * @param filepath File path to check
 * @return true if file exists, false otherwise
 */
bool flash_mgr_util_file_exists(const char* filepath);

/**
 * @brief Get file information
 * @param filepath Full file path
 * @param info[out] File information structure
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t flash_mgr_util_get_file_info(const char* filepath, flash_mgr_file_info_t* info);

/**
 * @brief Copy a file
 * @param src_path Source file path
 * @param dst_path Destination file path
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t flash_mgr_util_copy_file(const char* src_path, const char* dst_path);

/**
 * @brief Move/rename a file
 * @param old_path Current file path
 * @param new_path New file path
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t flash_mgr_util_move_file(const char* old_path, const char* new_path);

// Advanced File Operations
/**
 * @brief Calculate file checksum (CRC32)
 * @param filepath Full file path
 * @param checksum[out] Calculated CRC32 checksum
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t flash_mgr_util_file_checksum(const char* filepath, uint32_t* checksum);

/**
 * @brief Get directory size (recursive)
 * @param path Directory path
 * @param total_size[out] Total size in bytes
 * @param file_count[out] Number of files (optional, can be NULL)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t flash_mgr_util_get_dir_size(const char* path, size_t* total_size, uint32_t* file_count);

/**
 * @brief Find files matching pattern
 * @param base_path Base directory to search
 * @param pattern File pattern (supports * and ? wildcards)
 * @param recursive Search subdirectories
 * @param callback Callback for each matching file
 * @param user_data User data for callback
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t flash_mgr_util_find_files(const char* base_path, const char* pattern, bool recursive, 
                                   flash_mgr_dir_callback_t callback, void* user_data);

#ifdef __cplusplus
}
#endif
