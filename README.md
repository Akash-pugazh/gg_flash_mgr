# GG Flash Manager

ESP32 External Flash Memory Manager with LittleFS support for efficient sensor data storage and retrieval.

## Features

- **External SPI Flash Support**: Works with W25Q128, W25Q64, and similar SPI flash chips
- **LittleFS Integration**: Reliable filesystem with wear leveling and power-safe operations
- **Circular Buffer Behavior**: Automatic cleanup when storage is full
- **RAM-Efficient Operations**: Chunked read/write operations to minimize RAM usage
- **Configurable Storage Limits**: Set maximum data size and cleanup thresholds
- **High-Performance**: Optimized for high-frequency sensor data logging
- **Power-Safe**: Atomic operations and metadata management
- **Easy Integration**: Simple API with comprehensive examples

## Hardware Requirements

- ESP32, ESP32-S2, ESP32-S3, ESP32-C3, or ESP32-C6
- External SPI flash memory (W25Q series recommended)
- SPI connections (MOSI, MISO, SCLK, CS)

### Default Pin Configuration

| Signal | Default Pin | Configurable |
|--------|-------------|--------------|
| MOSI   | GPIO 23     | ✓            |
| MISO   | GPIO 19     | ✓            |
| SCLK   | GPIO 18     | ✓            |
| CS     | GPIO 5      | ✓            |

## Installation

### Method 1: ESP-IDF Component Registry (Recommended)

Add to your project's `idf_component.yml`:

```yaml
dependencies:
  your-team/gg_flash_mgr: "^1.0.0"
```

### Method 2: Local Component

1. Clone or copy this component to your project's `components` directory:
```bash
cd your_project/components
git clone <repository-url> gg_flash_mgr
```

2. The component will be automatically detected by ESP-IDF build system.

### Method 3: External Component Directory

1. Place the component in a shared location
2. Add to your main `CMakeLists.txt`:
```cmake
set(EXTRA_COMPONENT_DIRS "path/to/components")
```

## Quick Start

### Basic Usage

```c
#include "gg_flash_mgr.h"

void app_main(void)
{
    // Get default configuration
    flash_mgr_config_t config = flash_mgr_get_default_config();
    
    // Initialize flash manager
    esp_err_t ret = flash_mgr_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE("app", "Flash manager init failed: %s", esp_err_to_name(ret));
        return;
    }
    
    // Append sensor data (temperature: 25.5°C)
    ret = flash_mgr_append(1, 1, 25500);  // type=1, unit=1, value=25.5*1000
    if (ret != ESP_OK) {
        ESP_LOGE("app", "Failed to append data: %s", esp_err_to_name(ret));
    }
    
    // Read data
    flash_mgr_entry_t buffer[10];
    uint32_t entries_read;
    ret = flash_mgr_read_chunk(buffer, 10, &entries_read);
    if (ret == ESP_OK && entries_read > 0) {
        ESP_LOGI("app", "Read %u entries", entries_read);
        ESP_LOGI("app", "First entry: type=%u, value=%.3f", 
                 buffer[0].type, buffer[0].value_x1000 / 1000.0);
    }
    
    // Delete processed entries
    flash_mgr_delete_processed(entries_read);
    
    // Cleanup
    flash_mgr_deinit();
}
```

## Configuration

### Hardware Configuration

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

### Storage Configuration

```c
// Storage limits
config.max_data_size = 8 * 1024 * 1024;  // 8MB maximum
config.chunk_buffer_size = 4096;          // 4KB chunk buffer

// Cleanup behavior
config.auto_cleanup = true;               // Enable automatic cleanup
config.cleanup_threshold = 0.90f;         // Cleanup when 90% full
config.cleanup_target = 0.70f;            // Target 70% after cleanup
```

### File System Configuration

```c
// File paths
config.mount_point = "/ext";
config.data_file = "/ext/sensor_data.bin";
config.meta_file = "/ext/metadata.bin";
config.partition_label = "external_flash";

// Initialization behavior
config.format_on_init = false;  // Don't format existing data
```

## API Reference

### Initialization Functions

#### `flash_mgr_get_default_config()`
Returns default configuration structure.

#### `flash_mgr_init(const flash_mgr_config_t* config)`
Initialize the flash manager with given configuration.

#### `flash_mgr_deinit()`
Deinitialize and cleanup resources.

#### `flash_mgr_is_initialized()`
Check if flash manager is initialized.

### Data Operations

#### `flash_mgr_append(uint8_t type, uint8_t unit, int32_t value_x1000)`
Append data entry with current timestamp.

#### `flash_mgr_append_with_timestamp(uint32_t timestamp, uint8_t type, uint8_t unit, int32_t value_x1000)`
Append data entry with custom timestamp.

#### `flash_mgr_read_chunk(flash_mgr_entry_t* buffer, uint32_t max_entries, uint32_t* entries_read)`
Read entries in chunks (oldest first).

#### `flash_mgr_delete_processed(uint32_t count)`
Delete processed entries from storage (frees space).

### Management Functions

#### `flash_mgr_get_status(flash_mgr_status_t* status)`
Get current storage status and statistics.

#### `flash_mgr_cleanup(uint32_t target_entries)`
Force cleanup to target number of entries.

#### `flash_mgr_format()`
Format storage (WARNING: deletes all data).

#### `flash_mgr_get_fs_info(size_t* total_bytes, size_t* used_bytes)`
Get filesystem information.

## Data Structure

### Entry Structure

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

### Status Structure

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

## Examples

The `examples/` directory contains comprehensive usage examples:

### Basic Usage (`examples/basic_usage.c`)
- Simple sensor data logging
- Reading and processing data
- Status monitoring
- Basic error handling

### Advanced Usage (`examples/advanced_usage.c`)
- High-frequency data logging (100Hz)
- Multi-task architecture
- Performance monitoring
- Custom hardware configuration
- Error recovery strategies

### Integration Examples

Copy the desired example to your project's `main/main.c` and modify as needed.

## Performance Characteristics

### Write Performance
- **Typical**: 1000-5000 entries/second
- **Factors**: SPI frequency, flash chip speed, data size
- **Optimization**: Use higher SPI frequencies for better performance

### Read Performance
- **Chunked reads**: 10,000-50,000 entries/second
- **Single reads**: 500-2000 entries/second
- **Optimization**: Use larger chunk sizes for bulk operations

### Memory Usage
- **RAM**: ~1KB base + chunk buffer size (configurable)
- **Flash**: Entry size is 18 bytes + filesystem overhead
- **Optimization**: Adjust chunk buffer size based on available RAM

## Troubleshooting

### Common Issues

#### Flash Manager Initialization Fails
```
E (123) flash_mgr: External flash initialization failed
```
**Solutions:**
- Check SPI pin connections
- Verify flash chip compatibility
- Try lower SPI frequency
- Check power supply stability

#### LittleFS Mount Failed
```
E (456) flash_mgr: LittleFS mount failed: ESP_FAIL
```
**Solutions:**
- Set `format_on_init = true` for first use
- Check flash chip size and addressing
- Verify partition table configuration

#### Write Operations Fail
```
E (789) flash_mgr: Failed to write entry
```
**Solutions:**
- Check available storage space
- Enable auto-cleanup
- Verify filesystem is not corrupted
- Check for hardware issues

### Debug Configuration

Enable debug logging in `gg_flash_mgr_config.h`:
```c
#define FLASH_MGR_ENABLE_DEBUG_LOGS 1
```

### Recovery Procedures

#### Format and Reinitialize
```c
flash_mgr_config_t config = flash_mgr_get_default_config();
config.format_on_init = true;
esp_err_t ret = flash_mgr_init(&config);
```

#### Manual Cleanup
```c
flash_mgr_status_t status;
flash_mgr_get_status(&status);
flash_mgr_cleanup(status.active_entries / 2);  // Keep half the data
```

## Configuration Options

### Compile-Time Configuration

Edit `include/gg_flash_mgr_config.h` to customize default values:

```c
// Hardware defaults
#define FLASH_MGR_DEFAULT_MOSI_PIN      23
#define FLASH_MGR_DEFAULT_MISO_PIN      19
#define FLASH_MGR_DEFAULT_SCLK_PIN      18
#define FLASH_MGR_DEFAULT_CS_PIN        5

// Storage defaults
#define FLASH_MGR_DEFAULT_MAX_DATA_SIZE (12 * 1024 * 1024)  // 12MB
#define FLASH_MGR_DEFAULT_CHUNK_BUFFER_SIZE 4096             // 4KB

// Behavior defaults
#define FLASH_MGR_DEFAULT_AUTO_CLEANUP      true
#define FLASH_MGR_DEFAULT_CLEANUP_THRESHOLD 0.95f
#define FLASH_MGR_DEFAULT_CLEANUP_TARGET    0.75f
```

### Runtime Configuration

Override defaults at runtime:
```c
flash_mgr_config_t config = flash_mgr_get_default_config();
config.max_data_size = 16 * 1024 * 1024;  // 16MB
config.cleanup_threshold = 0.80f;          // Cleanup at 80%
```

## Dependencies

- **ESP-IDF**: >= 4.1.0
- **LittleFS**: joltwallet/littlefs ^1.20.1 (automatically managed)

## License

MIT License - see LICENSE file for details.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## Support

For issues and questions:
1. Check the troubleshooting section
2. Review existing issues in the repository
3. Create a new issue with detailed information

## Changelog

### v1.0.0
- Initial release
- Basic flash management functionality
- LittleFS integration
- Circular buffer behavior
- Comprehensive examples and documentation
