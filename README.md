# 🚀 GG Flash Manager

**The ESP32 External Flash Memory Manager** 🔥

[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-4.1+-blue.svg)](https://github.com/espressif/esp-idf)
[![Platform](https://img.shields.io/badge/Platform-ESP32-green.svg)](https://www.espressif.com/en/products/socs/esp32)
[![Version](https://img.shields.io/badge/Version-1.0.0-orange.svg)](https://github.com/your-team/gg_flash_mgr)

GG Flash Manager is for managing external SPI flash memory on ESP32 devices.

### 🎯 Core Features

- **🛡️ Data Integrity**: Built-in corruption detection and automatic recovery
- **💾 Smart Storage**: Automatic circular buffer with intelligent cleanup
- **🔄 Wear Leveling**: LittleFS integration spreads writes across flash blocks
- **🚀 RAM Efficient**: Chunked operations minimize memory usage

## 🛠️ Hardware Requirements

### Required Hardware
- **ESP32 Family**: ESP32, ESP32-S2, ESP32-S3, ESP32-C3, or ESP32-C6
- **External SPI Flash**: W25Q128 or compatible (16MB+ recommended)
- **SPI Connections**: Standard 4-wire SPI interface

### 📍 Default Pin Configuration

| Signal | GPIO Pin | Description | Configurable |
|--------|----------|-------------|--------------|
| **MOSI** | `23` | Master Out Slave In | ✅ |
| **MISO** | `19` | Master In Slave Out | ✅ |
| **SCLK** | `18` | Serial Clock | ✅ |
| **CS** | `5` | Chip Select | ✅ |

> **💡 Pro Tip**: Customize pins if your board layout needs it!

## 📦 Installation

### Method 1: ESP-IDF Component Registry (The Cool Way)

Add to your project's `idf_component.yml`:

```yaml
dependencies:
  <namespace>/gg_flash_mgr: "^1.0.0"
```

## 🚀 Quick Start

### ⚡ Basic Usage (5 Minutes Setup)

```c
#include "gg_flash_mgr.h"

void app_main(void)
{
    // 🔥 One-liner setup with defaults
    flash_mgr_config_t config = flash_mgr_get_default_config();

    // 🚀 Initialize (handles all the complex flash setup)
    esp_err_t ret = flash_mgr_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE("app", "❌ Flash init failed: %s", esp_err_to_name(ret));
        return;
    }

    // 📝 Log some sensor data (temperature: 25.5°C)
    ret = flash_mgr_append(1, 1, 25500);  // type=1, unit=1, value=25.5*1000
    if (ret != ESP_OK) {
        ESP_LOGE("app", "❌ Data logging failed: %s", esp_err_to_name(ret));
    }

    // 📖 Read your data back
    flash_mgr_entry_t buffer[10];
    uint32_t entries_read;
    ret = flash_mgr_read_chunk(buffer, 10, &entries_read);

    if (ret == ESP_OK && entries_read > 0) {
        ESP_LOGI("app", "📊 Got %u entries!", entries_read);
        ESP_LOGI("app", "🌡️ Temperature: %.1f°C",
                 buffer[0].value_x1000 / 1000.0);
    }

    // 🗑️ Clean up processed data (frees flash space)
    flash_mgr_delete(entries_read);

    // 🔄 Shutdown properly
    flash_mgr_deinit();
}
```

## ⚙️ Configuration

### 🛠️ Hardware Configuration

```c
flash_mgr_config_t config = flash_mgr_get_default_config();

// Custom SPI pins
config.mosi_pin = 13;
config.miso_pin = 12;
config.sclk_pin = 14;
config.cs_pin = 15;
config.spi_host = HSPI_HOST;
config.freq_mhz = 40;
```

### 💾 Storage Configuration

```c
// Storage limits
config.max_data_size = 8 * 1024 * 1024;  // 8MB maximum
config.chunk_buffer_size = 4096;          // 4KB chunk buffer

// Cleanup behavior
config.auto_cleanup = true;               // Enable automatic cleanup
config.cleanup_threshold = 0.90f;         // Cleanup when 90% full
config.cleanup_target = 0.70f;            // Target 70% after cleanup
```

### 🗂️ File System Configuration

```c
// File paths
config.mount_point = "/ext";
config.data_file = "/ext/data.bin";
config.meta_file = "/ext/meta.bin";
config.partition_label = "gg_flash_storage";

// Initialization behavior
config.format_on_init = false;  // Don't format existing data else you are dead 💀
```

## 🏗️ Data Structure

### 📝 Entry Structure (The Heart of Your Data)

```c
typedef struct {
    uint32_t timestamp;     // Entry timestamp (Unix time or sequence)
    uint32_t id;           // Unique entry ID
    uint8_t type;          // Data type identifier (user-defined)
    uint8_t unit;          // Data unit identifier (user-defined)
    int32_t value_x1000;   // Value multiplied by 1000 for precision
    uint8_t reserved[2];   // Reserved for alignment
} flash_mgr_entry_t;
```

### 📊 Status Structure (Know Your Storage State)

```c
typedef struct {
    uint32_t total_entries;     // Total entries ever written
    uint32_t active_entries;    // Currently active entries
    uint32_t deleted_entries;   // Total entries deleted
    uint32_t free_space_bytes;  // Available storage space
    uint32_t used_space_bytes;  // Used storage space
    bool initialized;           // Initialization status
} flash_mgr_status_t;
```

## 🔗 Dependencies

- **ESP-IDF**: >= 4.1.0
- **LittleFS**: joltwallet/littlefs ^1.20.1 (automatically managed)


## 📈 Changelog

### v1.0.0 - **The Genesis Release** 🔥
- Initial release
- LittleFS integration
- Circular buffer behavior

