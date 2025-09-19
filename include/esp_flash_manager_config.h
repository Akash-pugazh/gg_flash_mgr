/**
 * @file gg_flash_mgr_config.h
 * @brief Configuration definitions for GG Flash Manager
 * 
 * This file contains default configuration values and compile-time settings.
 * Users can override these in their project's sdkconfig or by defining
 * macros before including the header.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// DEFAULT HARDWARE CONFIGURATION
// =============================================================================

#ifndef FLASH_MGR_DEFAULT_MOSI_PIN
#define FLASH_MGR_DEFAULT_MOSI_PIN      23      ///< Default MOSI pin
#endif

#ifndef FLASH_MGR_DEFAULT_MISO_PIN  
#define FLASH_MGR_DEFAULT_MISO_PIN      19      ///< Default MISO pin
#endif

#ifndef FLASH_MGR_DEFAULT_SCLK_PIN
#define FLASH_MGR_DEFAULT_SCLK_PIN      18      ///< Default SCLK pin
#endif

#ifndef FLASH_MGR_DEFAULT_CS_PIN
#define FLASH_MGR_DEFAULT_CS_PIN        5       ///< Default CS pin
#endif

#ifndef FLASH_MGR_DEFAULT_SPI_HOST
#define FLASH_MGR_DEFAULT_SPI_HOST      2       ///< Default SPI host (VSPI_HOST)
#endif

#ifndef FLASH_MGR_DEFAULT_FREQ_MHZ
#define FLASH_MGR_DEFAULT_FREQ_MHZ      40      ///< Default SPI frequency
#endif

// =============================================================================
// DEFAULT STORAGE CONFIGURATION  
// =============================================================================

#ifndef FLASH_MGR_DEFAULT_MOUNT_POINT
#define FLASH_MGR_DEFAULT_MOUNT_POINT   "/ext"  ///< Default mount point
#endif

#ifndef FLASH_MGR_DEFAULT_DATA_FILE
#define FLASH_MGR_DEFAULT_DATA_FILE     "/ext/data.bin"    ///< Default data file
#endif

#ifndef FLASH_MGR_DEFAULT_META_FILE
#define FLASH_MGR_DEFAULT_META_FILE     "/ext/meta.bin"    ///< Default metadata file
#endif

#ifndef FLASH_MGR_DEFAULT_PARTITION_LABEL
#define FLASH_MGR_DEFAULT_PARTITION_LABEL "littlefs_storage"   ///< Default partition label
#endif

// =============================================================================
// DEFAULT MEMORY LIMITS
// =============================================================================

#ifndef FLASH_MGR_DEFAULT_MAX_DATA_SIZE
#define FLASH_MGR_DEFAULT_MAX_DATA_SIZE (12 * 1024 * 1024) ///< Default 12MB storage limit
#endif

#ifndef FLASH_MGR_DEFAULT_CHUNK_BUFFER_SIZE
#define FLASH_MGR_DEFAULT_CHUNK_BUFFER_SIZE 4096            ///< Default 4KB chunk buffer
#endif

// =============================================================================
// DEFAULT BEHAVIOR CONFIGURATION
// =============================================================================

#ifndef FLASH_MGR_DEFAULT_FORMAT_ON_INIT
#define FLASH_MGR_DEFAULT_FORMAT_ON_INIT    false           ///< Don't format by default
#endif

#ifndef FLASH_MGR_DEFAULT_AUTO_CLEANUP
#define FLASH_MGR_DEFAULT_AUTO_CLEANUP      true            ///< Enable auto cleanup
#endif

#ifndef FLASH_MGR_DEFAULT_CLEANUP_THRESHOLD
#define FLASH_MGR_DEFAULT_CLEANUP_THRESHOLD 0.95f           ///< Cleanup at 95% full
#endif

#ifndef FLASH_MGR_DEFAULT_CLEANUP_TARGET
#define FLASH_MGR_DEFAULT_CLEANUP_TARGET    0.75f           ///< Target 75% after cleanup
#endif

// =============================================================================
// COMPILE-TIME LIMITS AND VALIDATION
// =============================================================================

#ifndef FLASH_MGR_MAX_CHUNK_BUFFER_SIZE
#define FLASH_MGR_MAX_CHUNK_BUFFER_SIZE (64 * 1024)         ///< Maximum chunk buffer size
#endif

#ifndef FLASH_MGR_MIN_CHUNK_BUFFER_SIZE  
#define FLASH_MGR_MIN_CHUNK_BUFFER_SIZE 1024                ///< Minimum chunk buffer size
#endif

#ifndef FLASH_MGR_MAX_DATA_SIZE
#define FLASH_MGR_MAX_DATA_SIZE         (16 * 1024 * 1024)  ///< Maximum data size (16MB)
#endif

#ifndef FLASH_MGR_MIN_DATA_SIZE
#define FLASH_MGR_MIN_DATA_SIZE         1024                ///< Minimum data size (1KB)
#endif

// =============================================================================
// LOGGING CONFIGURATION
// =============================================================================

#ifndef FLASH_MGR_LOG_TAG
#define FLASH_MGR_LOG_TAG               "flash_mgr"         ///< Log tag
#endif

#ifndef FLASH_MGR_ENABLE_DEBUG_LOGS
#define FLASH_MGR_ENABLE_DEBUG_LOGS     0                   ///< Enable debug logging
#endif

#ifndef FLASH_MGR_PROGRESS_LOG_INTERVAL
#define FLASH_MGR_PROGRESS_LOG_INTERVAL (10 * 4096)         ///< Progress log every 40KB
#endif

// =============================================================================
// ENTRY STRUCTURE CONFIGURATION
// =============================================================================

#ifndef FLASH_MGR_ENTRY_SIZE
#define FLASH_MGR_ENTRY_SIZE            18                  ///< Size of data entry in bytes
#endif

// =============================================================================
// VALIDATION MACROS
// =============================================================================

#if FLASH_MGR_DEFAULT_CHUNK_BUFFER_SIZE > FLASH_MGR_MAX_CHUNK_BUFFER_SIZE
#error "Default chunk buffer size exceeds maximum allowed"
#endif

#if FLASH_MGR_DEFAULT_CHUNK_BUFFER_SIZE < FLASH_MGR_MIN_CHUNK_BUFFER_SIZE
#error "Default chunk buffer size is below minimum required"
#endif

#if FLASH_MGR_DEFAULT_MAX_DATA_SIZE > FLASH_MGR_MAX_DATA_SIZE
#error "Default data size exceeds maximum allowed"
#endif

#if FLASH_MGR_DEFAULT_MAX_DATA_SIZE < FLASH_MGR_MIN_DATA_SIZE
#error "Default data size is below minimum required"
#endif

#if FLASH_MGR_DEFAULT_CLEANUP_THRESHOLD <= FLASH_MGR_DEFAULT_CLEANUP_TARGET
#error "Cleanup threshold must be greater than cleanup target"
#endif

#ifdef __cplusplus
}
#endif
