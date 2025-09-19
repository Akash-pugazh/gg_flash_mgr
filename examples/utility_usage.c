/**
 * @file utility_usage.c
 * @brief Example usage of GG Flash Manager utility functions
 * 
 * This example demonstrates:
 * - Creating nested directories
 * - Writing and reading files
 * - Directory operations
 * - File management utilities
 */

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "gg_flash_mgr.h"

static const char *TAG = "utility_example";

// Callback function for directory listing
bool list_callback(const char* path, const flash_mgr_file_info_t* info, void* user_data) {
    uint32_t* count = (uint32_t*)user_data;
    (*count)++;
    
    ESP_LOGI(TAG, "📄 %s %s (%u bytes)", 
             info->is_directory ? "DIR " : "FILE", 
             path, 
             info->size);
    
    return true; // Continue listing
}

// Callback function for finding files
bool find_callback(const char* path, const flash_mgr_file_info_t* info, void* user_data) {
    ESP_LOGI(TAG, "🔍 Found: %s (%u bytes)", path, info->size);
    return true; // Continue searching
}

void utility_functions_example(void)
{
    ESP_LOGI(TAG, "🚀 Starting GG Flash Manager Utility Functions Example");
    
    // Initialize flash manager first
    flash_mgr_config_t config = flash_mgr_get_default_config();
    esp_err_t ret = flash_mgr_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Flash manager init failed: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(TAG, "✅ Flash manager initialized");
    
    // =================================================================
    // DIRECTORY OPERATIONS
    // =================================================================
    
    ESP_LOGI(TAG, "📁 Testing Directory Operations");
    
    // Create nested directories
    const char* nested_dir = "/ext/logs/sensors/temperature";
    ret = flash_mgr_util_mkdir(nested_dir);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Created nested directory: %s", nested_dir);
    }
    
    // Create more directories
    flash_mgr_util_mkdir("/ext/logs/sensors/humidity");
    flash_mgr_util_mkdir("/ext/logs/sensors/pressure");
    flash_mgr_util_mkdir("/ext/config");
    flash_mgr_util_mkdir("/ext/backup");
    
    // Check if directories exist
    if (flash_mgr_util_dir_exists("/ext/logs")) {
        ESP_LOGI(TAG, "✅ Directory /ext/logs exists");
    }
    
    // =================================================================
    // FILE OPERATIONS
    // =================================================================
    
    ESP_LOGI(TAG, "📝 Testing File Operations");
    
    // Write some configuration files
    const char* config_data = "{\n  \"sensor_interval\": 1000,\n  \"upload_interval\": 60000\n}";
    ret = flash_mgr_util_write_text("/ext/config/sensor_config.json", config_data, false);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Wrote config file");
    }
    
    // Write sensor data files
    char sensor_data[256];
    for (int i = 0; i < 5; i++) {
        snprintf(sensor_data, sizeof(sensor_data), 
                 "Timestamp: %d, Temperature: %.1f°C\n", 
                 i * 1000, 25.0 + i * 0.5);
        
        flash_mgr_util_write_text("/ext/logs/sensors/temperature/temp_log.txt", 
                                 sensor_data, true); // Append mode
    }
    
    // Write binary data
    uint8_t binary_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    flash_mgr_util_write_file("/ext/backup/calibration.bin", 
                             binary_data, sizeof(binary_data), false);
    
    // Read back the config file
    char* read_config;
    ret = flash_mgr_util_read_text("/ext/config/sensor_config.json", &read_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "📖 Read config file:\n%s", read_config);
        free(read_config);
    }
    
    // =================================================================
    // FILE INFORMATION
    // =================================================================
    
    ESP_LOGI(TAG, "ℹ️ Testing File Information");
    
    // Get file info
    flash_mgr_file_info_t file_info;
    ret = flash_mgr_util_get_file_info("/ext/logs/sensors/temperature/temp_log.txt", &file_info);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "📊 File info: size=%u bytes, modified=%ld", 
                 file_info.size, file_info.mtime);
    }
    
    // Calculate file checksum
    uint32_t checksum;
    ret = flash_mgr_util_file_checksum("/ext/config/sensor_config.json", &checksum);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "🔐 File checksum: 0x%08X", checksum);
    }
    
    // =================================================================
    // DIRECTORY LISTING
    // =================================================================
    
    ESP_LOGI(TAG, "📋 Testing Directory Listing");
    
    // List directory contents
    uint32_t file_count = 0;
    ESP_LOGI(TAG, "📁 Contents of /ext/logs/sensors:");
    ret = flash_mgr_util_list_dir("/ext/logs/sensors", list_callback, &file_count);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "📊 Total items found: %u", file_count);
    }
    
    // Get directory size
    size_t dir_size;
    uint32_t total_files;
    ret = flash_mgr_util_get_dir_size("/ext/logs", &dir_size, &total_files);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "📊 Directory /ext/logs: %u bytes, %u files", 
                 dir_size, total_files);
    }
    
    // =================================================================
    // FILE OPERATIONS
    // =================================================================
    
    ESP_LOGI(TAG, "🔄 Testing File Operations");
    
    // Copy a file
    ret = flash_mgr_util_copy_file("/ext/config/sensor_config.json", 
                                  "/ext/backup/sensor_config_backup.json");
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ File copied successfully");
    }
    
    // Check if files exist
    if (flash_mgr_util_file_exists("/ext/backup/sensor_config_backup.json")) {
        ESP_LOGI(TAG, "✅ Backup file exists");
    }
    
    // =================================================================
    // ADVANCED OPERATIONS
    // =================================================================
    
    ESP_LOGI(TAG, "🔍 Testing Advanced Operations");
    
    // Find all .json files
    ESP_LOGI(TAG, "🔍 Searching for *.json files:");
    ret = flash_mgr_util_find_files("/ext", "*.json", true, find_callback, NULL);
    
    // Find all .txt files
    ESP_LOGI(TAG, "🔍 Searching for *.txt files:");
    flash_mgr_util_find_files("/ext", "*.txt", true, find_callback, NULL);
    
    // =================================================================
    // CLEANUP DEMONSTRATION
    // =================================================================
    
    ESP_LOGI(TAG, "🧹 Testing Cleanup Operations");
    
    // Move a file
    ret = flash_mgr_util_move_file("/ext/backup/calibration.bin", 
                                  "/ext/config/calibration_moved.bin");
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ File moved successfully");
    }
    
    // Delete a file
    ret = flash_mgr_util_delete_file("/ext/backup/sensor_config_backup.json");
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ File deleted successfully");
    }
    
    // Remove empty directory (non-recursive)
    ret = flash_mgr_util_rmdir("/ext/backup", false);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Empty directory removed");
    }
    
    // =================================================================
    // FINAL STATUS
    // =================================================================
    
    ESP_LOGI(TAG, "📊 Final Status");
    
    // Get final directory size
    ret = flash_mgr_util_get_dir_size("/ext", &dir_size, &total_files);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "📊 Total storage used: %u bytes, %u files", 
                 dir_size, total_files);
    }
    
    // List final directory structure
    ESP_LOGI(TAG, "📁 Final directory structure:");
    file_count = 0;
    flash_mgr_util_list_dir("/ext", list_callback, &file_count);
    
    ESP_LOGI(TAG, "✅ Utility functions example completed successfully!");
    
    // Cleanup flash manager
    flash_mgr_deinit();
}

void app_main(void)
{
    // Run the utility functions example
    utility_functions_example();
    
    // Keep the task alive
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

