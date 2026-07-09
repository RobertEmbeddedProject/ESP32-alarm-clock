#pragma once
#include <stdbool.h>

// Start the isolated UART reader task (creates its own task internally).
void radar_snooze_init(void);

// Optional: turn raw frame dump on/off (hex) for debugging.
void radar_debug_enable_raw_dump(bool on);

// Read current flags (atomic/volatile-safe accessors).
bool radar_get_motion(void);
bool radar_get_stationary(void);
