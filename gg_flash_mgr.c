/**
* @file gg_flash_mgr.c
* @brief ESP32 External Flash Memory Manager Implementation
*/

#include "gg_flash_mgr.h"
#include "gg_flash_mgr_config.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_littlefs.h"
#include "driver/spi_common.h"
#include "esp_flash.h"
#include "esp_flash_spi_init.h"
#include "esp_partition.h"
#include "hal/spi_flash_types.h"
#include "hal/spi_types.h"
#include "esp_timer.h"

static const char *TAG = FLASH_MGR_LOG_TAG;

// =============================================================================
// INTERNAL DATA STRUCTURES
// =============================================================================

/**
* @brief Internal metadata structure for file management
*/
typedef struct __attribute__((packed)) {
    uint32_t total_entries;      ///< Total entries written
    uint32_t active_entries;     ///< Currently active entries
    uint32_t next_id;           ///< Next entry ID
    uint32_t deleted_from_start; ///< How many entries deleted from start
    uint32_t magic;             ///< Magic number for validation
} flash_mgr_metadata_t;

#define FLASH_MGR_METADATA_MAGIC 0xFEEDC0DE

/**
* @brief Internal state structure
*/
typedef struct {
    flash_mgr_config_t config;
    flash_mgr_metadata_t meta;
    esp_flash_t *ext_flash;
    bool initialized;
} flash_mgr_state_t;

// =============================================================================
// GLOBAL STATE
// =============================================================================

static flash_mgr_state_t g_state = {0};

// =============================================================================
// INTERNAL FUNCTION DECLARATIONS
// =============================================================================

static esp_err_t init_external_flash(void);
static esp_err_t init_littlefs(void);
static esp_err_t load_metadata(void);
static esp_err_t save_metadata(void);
static uint32_t calculate_max_entries(void);
static esp_err_t perform_auto_cleanup(void);
static uint32_t get_current_timestamp(void);

// =============================================================================
// PUBLIC API IMPLEMENTATION
// =============================================================================

flash_mgr_config_t flash_mgr_get_default_config(void) {
    flash_mgr_config_t config = {
        // Hardware Configuration
        .mosi_pin = FLASH_MGR_DEFAULT_MOSI_PIN,
        .miso_pin = FLASH_MGR_DEFAULT_MISO_PIN,
        .sclk_pin = FLASH_MGR_DEFAULT_SCLK_PIN,
        .cs_pin = FLASH_MGR_DEFAULT_CS_PIN,
        .spi_host = FLASH_MGR_DEFAULT_SPI_HOST,
        .freq_mhz = FLASH_MGR_DEFAULT_FREQ_MHZ,
        
        // Storage Configuration
        .mount_point = FLASH_MGR_DEFAULT_MOUNT_POINT,
        .data_file = FLASH_MGR_DEFAULT_DATA_FILE,
        .meta_file = FLASH_MGR_DEFAULT_META_FILE,
        .partition_label = FLASH_MGR_DEFAULT_PARTITION_LABEL,
        
        // Memory Limits
        .max_data_size = FLASH_MGR_DEFAULT_MAX_DATA_SIZE,
        .chunk_buffer_size = FLASH_MGR_DEFAULT_CHUNK_BUFFER_SIZE,
        
        // Behavior Configuration
        .format_on_init = FLASH_MGR_DEFAULT_FORMAT_ON_INIT,
        .auto_cleanup = FLASH_MGR_DEFAULT_AUTO_CLEANUP,
        .cleanup_threshold = FLASH_MGR_DEFAULT_CLEANUP_THRESHOLD,
        .cleanup_target = FLASH_MGR_DEFAULT_CLEANUP_TARGET
    };
    return config;
}

esp_err_t flash_mgr_init(const flash_mgr_config_t* config) {
    if (g_state.initialized) {
        ESP_LOGW(TAG, "Flash manager already initialized");
        return ESP_OK;
    }
    
    if (!config) {
        ESP_LOGE(TAG, "Configuration cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate configuration
    if (config->max_data_size < FLASH_MGR_MIN_DATA_SIZE || 
        config->max_data_size > FLASH_MGR_MAX_DATA_SIZE) {
        ESP_LOGE(TAG, "Invalid max_data_size: %u (must be %u-%u)", 
                config->max_data_size, FLASH_MGR_MIN_DATA_SIZE, FLASH_MGR_MAX_DATA_SIZE);
        return ESP_ERR_INVALID_ARG;
    }
    
if (config->chunk_buffer_size < FLASH_MGR_MIN_CHUNK_BUFFER_SIZE ||
    config->chunk_buffer_size > FLASH_MGR_MAX_CHUNK_BUFFER_SIZE) {
    ESP_LOGE(TAG, "Invalid chunk_buffer_size: %u (must be %u-%u)",
                config->chunk_buffer_size, FLASH_MGR_MIN_CHUNK_BUFFER_SIZE, FLASH_MGR_MAX_CHUNK_BUFFER_SIZE);
    return ESP_ERR_INVALID_ARG;
}

// Validate cleanup thresholds
if (config->cleanup_threshold <= config->cleanup_target) {
    ESP_LOGE(TAG, "Invalid cleanup configuration: threshold (%.2f) must be > target (%.2f)",
                config->cleanup_threshold, config->cleanup_target);
    return ESP_ERR_INVALID_ARG;
}

if (config->cleanup_threshold > 1.0f || config->cleanup_target > 1.0f ||
    config->cleanup_threshold < 0.0f || config->cleanup_target < 0.0f) {
    ESP_LOGE(TAG, "Invalid cleanup ratios: must be between 0.0 and 1.0");
    return ESP_ERR_INVALID_ARG;
}
    
    // Copy configuration
    memcpy(&g_state.config, config, sizeof(flash_mgr_config_t));
    
    ESP_LOGI(TAG, "Initializing Flash Manager");
    ESP_LOGI(TAG, "  Max data size: %u bytes (%.1f MB)", 
            config->max_data_size, config->max_data_size / (1024.0 * 1024.0));
    ESP_LOGI(TAG, "  Chunk buffer: %u bytes", config->chunk_buffer_size);
    ESP_LOGI(TAG, "  Auto cleanup: %s", config->auto_cleanup ? "enabled" : "disabled");
    
    esp_err_t ret = init_external_flash();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "External flash initialization failed");
        return ret;
    }
    
    ret = init_littlefs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LittleFS initialization failed");
        return ret;
    }
    
    ret = load_metadata();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Metadata loading failed");
        return ret;
    }
    
    g_state.initialized = true;
    
    ESP_LOGI(TAG, "Flash manager initialized successfully");
    ESP_LOGI(TAG, "  Max entries: %u", calculate_max_entries());
    ESP_LOGI(TAG, "  Current entries: %u", g_state.meta.active_entries);
    
    return ESP_OK;
}

esp_err_t flash_mgr_deinit(void) {
    if (!g_state.initialized) {
        return ESP_OK;
    }
    
    // Save metadata before deinitializing
    save_metadata();
    
    // Unmount filesystem
    esp_vfs_littlefs_unregister(g_state.config.partition_label);
    
    // Reset state
    memset(&g_state, 0, sizeof(g_state));
    
    ESP_LOGI(TAG, "Flash manager deinitialized");
    return ESP_OK;
}

bool flash_mgr_is_initialized(void) {
    return g_state.initialized;
}

esp_err_t flash_mgr_append(uint8_t type, uint8_t unit, int32_t value_x1000) {
    return flash_mgr_append_with_timestamp(get_current_timestamp(), type, unit, value_x1000);
}

esp_err_t flash_mgr_append_with_timestamp(uint32_t timestamp, uint8_t type, uint8_t unit, int32_t value_x1000) {
    if (!g_state.initialized) {
        ESP_LOGE(TAG, "Flash manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    flash_mgr_entry_t entry = {
        .timestamp = timestamp,
        .id = g_state.meta.next_id++,
        .type = type,
        .unit = unit,
        .value_x1000 = value_x1000,
        .reserved = {0}
    };
    
#if FLASH_MGR_ENABLE_DEBUG_LOGS
    ESP_LOGD(TAG, "Appending entry ID %u", entry.id);
#endif
    
    // Append to end of file
    FILE *f = fopen(g_state.config.data_file, "ab");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open data file for append");
        return ESP_FAIL;
    }
    
    size_t written = fwrite(&entry, sizeof(flash_mgr_entry_t), 1, f);
    fclose(f);
    
    if (written != 1) {
        ESP_LOGE(TAG, "Failed to write entry");
        return ESP_FAIL;
    }
    
    // Update metadata
    g_state.meta.total_entries++;
    g_state.meta.active_entries++;
    
    // Check for auto cleanup
    if (g_state.config.auto_cleanup) {
        uint32_t current_size = g_state.meta.active_entries * sizeof(flash_mgr_entry_t);
        float usage_ratio = (float)current_size / g_state.config.max_data_size;
        
        if (usage_ratio >= g_state.config.cleanup_threshold) {
            ESP_LOGW(TAG, "Storage %.1f%% full, triggering auto cleanup", usage_ratio * 100);
            esp_err_t cleanup_ret = perform_auto_cleanup();
            if (cleanup_ret != ESP_OK) {
                ESP_LOGE(TAG, "Auto cleanup failed: %s", esp_err_to_name(cleanup_ret));
                // Continue anyway - don't fail the append operation
            }
        }
    }
    
    esp_err_t ret = save_metadata();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save metadata");
        return ret;
    }
    
#if FLASH_MGR_ENABLE_DEBUG_LOGS
    ESP_LOGD(TAG, "Entry appended successfully");
#endif
    
    return ESP_OK;
}

esp_err_t flash_mgr_read_chunk(flash_mgr_entry_t* buffer, uint32_t max_entries, uint32_t* entries_read) {
    if (!g_state.initialized || !buffer || !entries_read) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *entries_read = 0;
    
    if (g_state.meta.active_entries == 0) {
        return ESP_OK; // No data to read
    }
    
    FILE *f = fopen(g_state.config.data_file, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open data file for reading");
        return ESP_FAIL;
    }
    
    // Read from beginning of file (oldest entries first)
    uint32_t entries_to_read = (max_entries < g_state.meta.active_entries) ? 
                            max_entries : g_state.meta.active_entries;
    
    for (uint32_t i = 0; i < entries_to_read; i++) {
        size_t read = fread(&buffer[i], sizeof(flash_mgr_entry_t), 1, f);
        if (read != 1) {
            // End of file or read error
            break;
        }
        (*entries_read)++;
    }
    
    fclose(f);
    
#if FLASH_MGR_ENABLE_DEBUG_LOGS
    ESP_LOGD(TAG, "Read %u entries from start of file", *entries_read);
#endif
    
    return ESP_OK;
}

esp_err_t flash_mgr_delete(uint32_t count) {
    if (!g_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (count > g_state.meta.active_entries) {
        count = g_state.meta.active_entries;
    }
    
    if (count == 0) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Deleting %u entries", count);
    
    // Calculate remaining data
    uint32_t remaining_entries = g_state.meta.active_entries - count;
    uint32_t bytes_to_skip = count * sizeof(flash_mgr_entry_t);
    
    if (remaining_entries == 0) {
        // Simple case: delete entire file
        ESP_LOGI(TAG, "Deleting entire file (no remaining entries)");
        if (remove(g_state.config.data_file) != 0) {
            ESP_LOGW(TAG, "Failed to remove file, but continuing");
        }
        
        g_state.meta.active_entries = 0;
        g_state.meta.deleted_from_start += count;
        return save_metadata();
    }
    
    // Use configured chunk size for RAM-efficient operation
    uint8_t *chunk_buffer = malloc(g_state.config.chunk_buffer_size);
    if (!chunk_buffer) {
        ESP_LOGE(TAG, "Failed to allocate %u byte chunk buffer", g_state.config.chunk_buffer_size);
        return ESP_ERR_NO_MEM;
    }
    
    // Create temporary file for safe operation
    char temp_file[256];
    snprintf(temp_file, sizeof(temp_file), "%s_temp.bin", g_state.config.data_file);
    
    FILE *src = fopen(g_state.config.data_file, "rb");
    if (!src) {
        ESP_LOGE(TAG, "Failed to open source file");
        free(chunk_buffer);
        return ESP_FAIL;
    }
    
    FILE *dst = fopen(temp_file, "wb");
    if (!dst) {
        ESP_LOGE(TAG, "Failed to create temp file");
        fclose(src);
        free(chunk_buffer);
        return ESP_FAIL;
    }
    
    // Skip the entries to delete
    if (fseek(src, bytes_to_skip, SEEK_SET) != 0) {
        ESP_LOGE(TAG, "Failed to seek past deleted entries");
        fclose(src);
        fclose(dst);
        free(chunk_buffer);
        remove(temp_file);
        return ESP_FAIL;
    }
    
    // Copy remaining data in chunks
    uint32_t remaining_bytes = remaining_entries * sizeof(flash_mgr_entry_t);
    uint32_t bytes_copied = 0;
    
    ESP_LOGI(TAG, "Copying %u bytes in chunks of %u", remaining_bytes, g_state.config.chunk_buffer_size);
    
    while (bytes_copied < remaining_bytes) {
        uint32_t chunk_size = (remaining_bytes - bytes_copied > g_state.config.chunk_buffer_size) ? 
                            g_state.config.chunk_buffer_size : (remaining_bytes - bytes_copied);
        
        size_t read = fread(chunk_buffer, 1, chunk_size, src);
        if (read != chunk_size) {
            ESP_LOGE(TAG, "Read error: got %u, expected %u at offset %u", 
                    read, chunk_size, bytes_copied);
            break;
        }
        
        size_t written = fwrite(chunk_buffer, 1, chunk_size, dst);
        if (written != chunk_size) {
            ESP_LOGE(TAG, "Write error: wrote %u, expected %u", written, chunk_size);
            break;
        }
        
        bytes_copied += chunk_size;
        
        // Progress indicator for large operations
        if (bytes_copied % FLASH_MGR_PROGRESS_LOG_INTERVAL == 0) {
            ESP_LOGI(TAG, "Copied %u/%u bytes (%.1f%%)", 
                    bytes_copied, remaining_bytes, 100.0 * bytes_copied / remaining_bytes);
        }
    }
    
    fclose(src);
    fclose(dst);
    free(chunk_buffer);
    
    if (bytes_copied != remaining_bytes) {
        ESP_LOGE(TAG, "Copy failed: %u/%u bytes copied", bytes_copied, remaining_bytes);
        remove(temp_file);
        return ESP_FAIL;
    }
    
    // Atomically replace original file with temp file
    if (remove(g_state.config.data_file) != 0) {
        ESP_LOGE(TAG, "Failed to remove original file");
        remove(temp_file);
        return ESP_FAIL;
    }
    
    if (rename(temp_file, g_state.config.data_file) != 0) {
        ESP_LOGE(TAG, "Failed to rename temp file");
        return ESP_FAIL;
    }
    
    // Update metadata
    g_state.meta.active_entries -= count;
    g_state.meta.deleted_from_start += count;
    
    esp_err_t ret = save_metadata();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save metadata after deletion");
        return ret;
    }
    
    ESP_LOGI(TAG, "Successfully deleted %u entries. Active: %u, Total deleted: %u", 
            count, g_state.meta.active_entries, g_state.meta.deleted_from_start);
    
    return ESP_OK;
}

esp_err_t flash_mgr_get_status(flash_mgr_status_t* status) {
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_state.initialized) {
        memset(status, 0, sizeof(flash_mgr_status_t));
        status->initialized = false;
        return ESP_ERR_INVALID_STATE;
    }
    
    status->total_entries = g_state.meta.total_entries;
    status->active_entries = g_state.meta.active_entries;
    status->deleted_entries = g_state.meta.deleted_from_start;
    status->used_space_bytes = g_state.meta.active_entries * sizeof(flash_mgr_entry_t);
    status->free_space_bytes = g_state.config.max_data_size - status->used_space_bytes;
    status->initialized = true;
    
    return ESP_OK;
}

esp_err_t flash_mgr_cleanup(uint32_t target_entries) {
    if (!g_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (target_entries >= g_state.meta.active_entries) {
        ESP_LOGW(TAG, "Target entries %u >= active entries %u, no cleanup needed", 
                target_entries, g_state.meta.active_entries);
        return ESP_OK;
    }
    
    uint32_t entries_to_remove = g_state.meta.active_entries - target_entries;
    ESP_LOGI(TAG, "Manual cleanup: removing %u entries (keeping %u)", 
            entries_to_remove, target_entries);
    
    return flash_mgr_delete(entries_to_remove);
}

esp_err_t flash_mgr_format(void) {
    if (!g_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGW(TAG, "Formatting storage - ALL DATA WILL BE LOST");
    
    // Remove data files
    remove(g_state.config.data_file);
    remove(g_state.config.meta_file);
    
    // Reset metadata
    memset(&g_state.meta, 0, sizeof(g_state.meta));
    g_state.meta.magic = FLASH_MGR_METADATA_MAGIC;
    
    esp_err_t ret = save_metadata();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save metadata after format");
        return ret;
    }
    
    ESP_LOGI(TAG, "Storage formatted successfully");
    return ESP_OK;
}

esp_err_t flash_mgr_get_fs_info(size_t* total_bytes, size_t* used_bytes) {
    if (!g_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return esp_littlefs_info(g_state.config.partition_label, total_bytes, used_bytes);
}

// =============================================================================
// INTERNAL FUNCTION IMPLEMENTATIONS
// =============================================================================

static esp_err_t init_external_flash(void) {
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = g_state.config.mosi_pin,
        .miso_io_num = g_state.config.miso_pin,
        .sclk_io_num = g_state.config.sclk_pin,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1
    };
    
    esp_err_t ret = spi_bus_initialize(g_state.config.spi_host, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    esp_flash_spi_device_config_t flash_cfg = {
        .host_id = g_state.config.spi_host,
        .cs_io_num = g_state.config.cs_pin,
        .cs_id = 0,
        .freq_mhz = g_state.config.freq_mhz,
        .io_mode = SPI_FLASH_FASTRD
    };
    
    ret = spi_bus_add_flash_device(&g_state.ext_flash, &flash_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Add flash device failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_flash_init(g_state.ext_flash);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Flash init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    uint32_t jedec_id;
    ret = esp_flash_read_id(g_state.ext_flash, &jedec_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "JEDEC read failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "External flash initialized - JEDEC ID: 0x%06X", jedec_id);
    return ESP_OK;
}

static esp_err_t init_littlefs(void) {
    static esp_partition_t ext_partition = {
        .type = ESP_PARTITION_TYPE_DATA,
        .subtype = ESP_PARTITION_SUBTYPE_DATA_LITTLEFS,
        .address = 0x0,
        .size = 16 * 1024 * 1024, // 16MB
        .encrypted = false,
        .readonly = false
    };
    
    // Set partition label and flash chip
    strncpy((char*)ext_partition.label, g_state.config.partition_label, sizeof(ext_partition.label) - 1);
    ext_partition.flash_chip = g_state.ext_flash;
    
    esp_vfs_littlefs_conf_t conf = {
        .base_path = g_state.config.mount_point,
        .partition = &ext_partition,
        .format_if_mount_failed = g_state.config.format_on_init,
        .dont_mount = false,
        .grow_on_mount = false
    };
    
    if (g_state.config.format_on_init) {
        ESP_LOGI(TAG, "Formatting external flash...");
        esp_err_t fmt_ret = esp_littlefs_format_partition(&ext_partition);
        if (fmt_ret != ESP_OK) {
            ESP_LOGE(TAG, "Format failed: %s", esp_err_to_name(fmt_ret));
            return fmt_ret;
        }
    }
    
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LittleFS mount failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "LittleFS mounted at %s", g_state.config.mount_point);
    return ESP_OK;
}

static esp_err_t load_metadata(void) {
    FILE *f = fopen(g_state.config.meta_file, "rb");
    if (!f) {
        // First boot - initialize metadata
        memset(&g_state.meta, 0, sizeof(g_state.meta));
        g_state.meta.magic = FLASH_MGR_METADATA_MAGIC;
        ESP_LOGI(TAG, "Initializing fresh metadata");
        return ESP_OK;
    }
    
    size_t read = fread(&g_state.meta, sizeof(flash_mgr_metadata_t), 1, f);
    fclose(f);
    
    if (read != 1) {
        ESP_LOGE(TAG, "Failed to read metadata");
        return ESP_FAIL;
    }
    
    if (g_state.meta.magic != FLASH_MGR_METADATA_MAGIC) {
        ESP_LOGW(TAG, "Invalid metadata magic, reinitializing");
        memset(&g_state.meta, 0, sizeof(g_state.meta));
        g_state.meta.magic = FLASH_MGR_METADATA_MAGIC;
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Loaded metadata - active: %u, total: %u, deleted: %u",
            g_state.meta.active_entries, g_state.meta.total_entries, g_state.meta.deleted_from_start);
    
    return ESP_OK;
}

static esp_err_t save_metadata(void) {
    FILE *f = fopen(g_state.config.meta_file, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open metadata file for writing");
        return ESP_FAIL;
    }
    
    size_t written = fwrite(&g_state.meta, sizeof(flash_mgr_metadata_t), 1, f);
    fclose(f);
    
    if (written != 1) {
        ESP_LOGE(TAG, "Failed to write metadata");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

static uint32_t calculate_max_entries(void) {
    return g_state.config.max_data_size / sizeof(flash_mgr_entry_t);
}

static esp_err_t perform_auto_cleanup(void) {
    uint32_t max_entries = calculate_max_entries();
    uint32_t target_entries = (uint32_t)(max_entries * g_state.config.cleanup_target);
    
    if (g_state.meta.active_entries <= target_entries) {
        return ESP_OK; // Already at target
    }
    
    uint32_t entries_to_remove = g_state.meta.active_entries - target_entries;
    ESP_LOGI(TAG, "Auto cleanup: removing %u entries (keeping %u)", 
            entries_to_remove, target_entries);
    
    return flash_mgr_delete(entries_to_remove);
}
 
static uint32_t get_current_timestamp(void) {
    // Try to get real timestamp, fallback to entry ID
    time_t now = time(NULL);
    if (now > 0) {
        return (uint32_t)now;
    } else {
        return g_state.meta.next_id;
    }
}

// =============================================================================
// UTILITY FUNCTIONS IMPLEMENTATION - STANDALONE FILE/DIRECTORY OPERATIONS
// =============================================================================

#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <fnmatch.h>

static const char *UTIL_TAG = "gg_flash_util";

// Helper function to create parent directories
static esp_err_t create_parent_dirs(const char* filepath) {
    char* path_copy = strdup(filepath);
    if (!path_copy) {
        return ESP_ERR_NO_MEM;
    }
    
    char* last_slash = strrchr(path_copy, '/');
    if (!last_slash || last_slash == path_copy) {
        free(path_copy);
        return ESP_OK; // No parent directory or root directory
    }
    
    *last_slash = '\0'; // Terminate at parent directory
    
    // Check if parent directory exists
    struct stat st;
    if (stat(path_copy, &st) == 0) {
        free(path_copy);
        return ESP_OK; // Parent directory exists
    }
    
    // Recursively create parent directories
    esp_err_t ret = create_parent_dirs(path_copy);
    if (ret != ESP_OK) {
        free(path_copy);
        return ret;
    }
    
    // Create this directory
    if (mkdir(path_copy, 0755) != 0) {
        ESP_LOGE(UTIL_TAG, "Failed to create directory: %s", path_copy);
        free(path_copy);
        return ESP_FAIL;
    }
    
    free(path_copy);
    return ESP_OK;
}

// Directory Operations
esp_err_t flash_mgr_util_mkdir(const char* path) {
    if (!path) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check if directory already exists
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return ESP_OK; // Directory already exists
        } else {
            return ESP_ERR_INVALID_STATE; // Path exists but is not a directory
        }
    }
    
    // Create parent directories first
    esp_err_t ret = create_parent_dirs(path);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Create the target directory
    if (mkdir(path, 0755) != 0) {
        ESP_LOGE(UTIL_TAG, "Failed to create directory: %s", path);
        return ESP_FAIL;
    }
    
    ESP_LOGI(UTIL_TAG, "Created directory: %s", path);
    return ESP_OK;
}

esp_err_t flash_mgr_util_rmdir(const char* path, bool recursive) {
    if (!path) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!recursive) {
        if (rmdir(path) != 0) {
            ESP_LOGE(UTIL_TAG, "Failed to remove directory: %s", path);
            return ESP_FAIL;
        }
        return ESP_OK;
    }
    
    // Recursive removal
    DIR* dir = opendir(path);
    if (!dir) {
        ESP_LOGE(UTIL_TAG, "Failed to open directory: %s", path);
        return ESP_FAIL;
    }
    
    struct dirent* entry;
    char full_path[256];
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                flash_mgr_util_rmdir(full_path, true);
            } else {
                unlink(full_path);
            }
        }
    }
    
    closedir(dir);
    
    if (rmdir(path) != 0) {
        ESP_LOGE(UTIL_TAG, "Failed to remove directory: %s", path);
        return ESP_FAIL;
    }
    
    ESP_LOGI(UTIL_TAG, "Removed directory: %s", path);
    return ESP_OK;
}

bool flash_mgr_util_dir_exists(const char* path) {
    if (!path) {
        return false;
    }
    
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

esp_err_t flash_mgr_util_list_dir(const char* path, flash_mgr_dir_callback_t callback, void* user_data) {
    if (!path || !callback) {
        return ESP_ERR_INVALID_ARG;
    }
    
    DIR* dir = opendir(path);
    if (!dir) {
        ESP_LOGE(UTIL_TAG, "Failed to open directory: %s", path);
        return ESP_FAIL;
    }
    
    struct dirent* entry;
    char full_path[256];
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        flash_mgr_file_info_t info = {0};
        struct stat st;
        
        if (stat(full_path, &st) == 0) {
            info.size = st.st_size;
            info.mtime = st.st_mtime;
            info.is_directory = S_ISDIR(st.st_mode);
            
            if (!callback(full_path, &info, user_data)) {
                break; // Callback requested to stop
            }
        }
    }
    
    closedir(dir);
    return ESP_OK;
}

// File Operations
esp_err_t flash_mgr_util_write_file(const char* filepath, const void* data, size_t size, bool append) {
    if (!filepath || !data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Create parent directories if needed
    esp_err_t ret = create_parent_dirs(filepath);
    if (ret != ESP_OK) {
        return ret;
    }
    
    FILE* file = fopen(filepath, append ? "ab" : "wb");
    if (!file) {
        ESP_LOGE(UTIL_TAG, "Failed to open file: %s", filepath);
        return ESP_FAIL;
    }
    
    size_t written = fwrite(data, 1, size, file);
    fclose(file);
    
    if (written != size) {
        ESP_LOGE(UTIL_TAG, "Failed to write complete data to file: %s", filepath);
        return ESP_FAIL;
    }
    
    ESP_LOGI(UTIL_TAG, "Wrote %u bytes to file: %s", size, filepath);
    return ESP_OK;
}

esp_err_t flash_mgr_util_read_file(const char* filepath, void** buffer, size_t* size) {
    if (!filepath || !buffer || !size) {
        return ESP_ERR_INVALID_ARG;
    }
    
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        ESP_LOGE(UTIL_TAG, "Failed to open file: %s", filepath);
        return ESP_FAIL;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size < 0) {
        fclose(file);
        return ESP_FAIL;
    }
    
    // Allocate buffer
    *buffer = malloc(file_size + 1); // +1 for null terminator if needed
    if (!*buffer) {
        fclose(file);
        return ESP_ERR_NO_MEM;
    }
    
    // Read file
    size_t read_size = fread(*buffer, 1, file_size, file);
    fclose(file);
    
    if (read_size != file_size) {
        free(*buffer);
        *buffer = NULL;
        return ESP_FAIL;
    }
    
    *size = file_size;
    ((char*)*buffer)[file_size] = '\0'; // Null terminate for text files
    
    ESP_LOGI(UTIL_TAG, "Read %u bytes from file: %s", *size, filepath);
    return ESP_OK;
}

esp_err_t flash_mgr_util_write_text(const char* filepath, const char* text, bool append) {
    if (!text) {
        return ESP_ERR_INVALID_ARG;
    }
    
    return flash_mgr_util_write_file(filepath, text, strlen(text), append);
}

esp_err_t flash_mgr_util_read_text(const char* filepath, char** text) {
    size_t size;
    return flash_mgr_util_read_file(filepath, (void**)text, &size);
}

esp_err_t flash_mgr_util_delete_file(const char* filepath) {
    if (!filepath) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (unlink(filepath) != 0) {
        ESP_LOGE(UTIL_TAG, "Failed to delete file: %s", filepath);
        return ESP_FAIL;
    }
    
    ESP_LOGI(UTIL_TAG, "Deleted file: %s", filepath);
    return ESP_OK;
}

bool flash_mgr_util_file_exists(const char* filepath) {
    if (!filepath) {
        return false;
    }
    
    struct stat st;
    return (stat(filepath, &st) == 0 && S_ISREG(st.st_mode));
}

esp_err_t flash_mgr_util_get_file_info(const char* filepath, flash_mgr_file_info_t* info) {
    if (!filepath || !info) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct stat st;
    if (stat(filepath, &st) != 0) {
        return ESP_FAIL;
    }
    
    info->size = st.st_size;
    info->mtime = st.st_mtime;
    info->is_directory = S_ISDIR(st.st_mode);
    
    return ESP_OK;
}

esp_err_t flash_mgr_util_copy_file(const char* src_path, const char* dst_path) {
    if (!src_path || !dst_path) {
        return ESP_ERR_INVALID_ARG;
    }
    
    void* buffer;
    size_t size;
    
    esp_err_t ret = flash_mgr_util_read_file(src_path, &buffer, &size);
    if (ret != ESP_OK) {
        return ret;
    }
    
    ret = flash_mgr_util_write_file(dst_path, buffer, size, false);
    free(buffer);
    
    if (ret == ESP_OK) {
        ESP_LOGI(UTIL_TAG, "Copied file: %s -> %s", src_path, dst_path);
    }
    
    return ret;
}

esp_err_t flash_mgr_util_move_file(const char* old_path, const char* new_path) {
    if (!old_path || !new_path) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Create parent directories for new path
    esp_err_t ret = create_parent_dirs(new_path);
    if (ret != ESP_OK) {
        return ret;
    }
    
    if (rename(old_path, new_path) != 0) {
        ESP_LOGE(UTIL_TAG, "Failed to move file: %s -> %s", old_path, new_path);
        return ESP_FAIL;
    }
    
    ESP_LOGI(UTIL_TAG, "Moved file: %s -> %s", old_path, new_path);
    return ESP_OK;
}

// Advanced File Operations
esp_err_t flash_mgr_util_file_checksum(const char* filepath, uint32_t* checksum) {
    if (!filepath || !checksum) {
        return ESP_ERR_INVALID_ARG;
    }
    
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        return ESP_FAIL;
    }
    
    uint32_t crc = 0xFFFFFFFF;
    uint8_t buffer[1024];
    size_t bytes_read;
    
    // Simple CRC32 calculation
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            crc ^= buffer[i];
            for (int j = 0; j < 8; j++) {
                if (crc & 1) {
                    crc = (crc >> 1) ^ 0xEDB88320;
                } else {
                    crc >>= 1;
                }
            }
        }
    }
    
    fclose(file);
    *checksum = crc ^ 0xFFFFFFFF;
    
    return ESP_OK;
}

esp_err_t flash_mgr_util_get_dir_size(const char* path, size_t* total_size, uint32_t* file_count) {
    if (!path || !total_size) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *total_size = 0;
    if (file_count) {
        *file_count = 0;
    }
    
    DIR* dir = opendir(path);
    if (!dir) {
        return ESP_FAIL;
    }
    
    struct dirent* entry;
    char full_path[256];
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                size_t subdir_size;
                uint32_t subdir_files;
                if (flash_mgr_util_get_dir_size(full_path, &subdir_size, &subdir_files) == ESP_OK) {
                    *total_size += subdir_size;
                    if (file_count) {
                        *file_count += subdir_files;
                    }
                }
            } else {
                *total_size += st.st_size;
                if (file_count) {
                    (*file_count)++;
                }
            }
        }
    }
    
    closedir(dir);
    return ESP_OK;
}

esp_err_t flash_mgr_util_find_files(const char* base_path, const char* pattern, bool recursive, 
                                   flash_mgr_dir_callback_t callback, void* user_data) {
    if (!base_path || !pattern || !callback) {
        return ESP_ERR_INVALID_ARG;
    }
    
    DIR* dir = opendir(base_path);
    if (!dir) {
        return ESP_FAIL;
    }
    
    struct dirent* entry;
    char full_path[256];
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);
        
        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                if (recursive) {
                    flash_mgr_util_find_files(full_path, pattern, recursive, callback, user_data);
                }
            } else {
                // Check if filename matches pattern
                if (fnmatch(pattern, entry->d_name, 0) == 0) {
                    flash_mgr_file_info_t info = {
                        .size = st.st_size,
                        .mtime = st.st_mtime,
                        .is_directory = false
                    };
                    
                    if (!callback(full_path, &info, user_data)) {
                        break; // Callback requested to stop
                    }
                }
            }
        }
    }
    
    closedir(dir);
    return ESP_OK;
}
 