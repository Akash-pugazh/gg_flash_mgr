idf_component_register(
    SRCS "gg_flash_mgr.c"
    INCLUDE_DIRS "include"
    REQUIRES "esp_flash" "esp_partition" "esp_timer" "driver"
)