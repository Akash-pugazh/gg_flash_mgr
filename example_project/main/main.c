/**
 * @file main.c
 * @brief Example project demonstrating GG Flash Manager usage
 */

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "gg_flash_mgr.h"

static const char *TAG = "example";

void app_main(void)
{
    ESP_LOGI(TAG, "GG Flash Manager Example Starting");
    
    // Get default configuration
    flash_mgr_config_t config = flash_mgr_get_default_config();
    
    // Initialize flash manager
    esp_err_t ret = flash_mgr_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Flash manager initialization failed: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(TAG, "Flash manager initialized successfully");
    
    // Example: Log some sensor data
    for (int i = 0; i < 10; i++) {
        // Simulate temperature sensor (25.0°C + i*0.1)
        int32_t temperature = 25000 + (i * 100);  // 25.0°C to 25.9°C
        ret = flash_mgr_append(1, 1, temperature);  // type=1 (temp), unit=1 (celsius)
        
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Logged temperature: %.1f°C", temperature / 1000.0);
        } else {
            ESP_LOGE(TAG, "Failed to log temperature: %s", esp_err_to_name(ret));
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));  // 1 second delay
    }
    
    // Read back the data
    ESP_LOGI(TAG, "Reading back logged data:");
    
    flash_mgr_entry_t buffer[10];
    uint32_t entries_read;
    
    ret = flash_mgr_read_chunk(buffer, 10, &entries_read);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Read %u entries:", entries_read);
        
        for (uint32_t i = 0; i < entries_read; i++) {
            ESP_LOGI(TAG, "  Entry %u: ID=%u, Type=%u, Value=%.1f", 
                     i, buffer[i].id, buffer[i].type, buffer[i].value_x1000 / 1000.0);
        }
        
        // Delete the processed entries
        ret = flash_mgr_delete_processed(entries_read);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Deleted %u processed entries", entries_read);
        }
    } else {
        ESP_LOGE(TAG, "Failed to read data: %s", esp_err_to_name(ret));
    }
    
    // Get final status
    flash_mgr_status_t status;
    ret = flash_mgr_get_status(&status);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Final status:");
        ESP_LOGI(TAG, "  Active entries: %u", status.active_entries);
        ESP_LOGI(TAG, "  Total entries: %u", status.total_entries);
        ESP_LOGI(TAG, "  Deleted entries: %u", status.deleted_entries);
    }
    
    ESP_LOGI(TAG, "Example completed successfully!");
    
    // Keep running
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
