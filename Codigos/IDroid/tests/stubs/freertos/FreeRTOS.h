#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
typedef unsigned TickType_t;
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(lock) ((void)(lock))
#define portEXIT_CRITICAL(lock) ((void)(lock))
#define pdMS_TO_TICKS(ms) ((ms)/10)
#define pdTRUE 1
#define pdPASS 1
