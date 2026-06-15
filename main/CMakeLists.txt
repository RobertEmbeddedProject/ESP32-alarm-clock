idf_component_register(
  SRCS
    "src/main.c"
    "src/wifi.c"
    "src/ssd1309.c"
    "src/rotary.c"
    "src/songs.c"
    "src/mp3.c"
    "src/graphics_smallfonts.c"
    "src/graphics_bigfonts.c"
    "src/graphics_bitmaps.c"
  INCLUDE_DIRS
    "."
    "src"
    "inc"
  REQUIRES
    driver log esp_netif lwip esp_event esp_driver_pcnt esp_wifi esp_timer nvs_flash bt 
)
