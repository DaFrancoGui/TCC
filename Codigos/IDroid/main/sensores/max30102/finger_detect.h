#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct { uint32_t baseline; uint8_t up, down; bool present; } finger_state_t;
void finger_init(finger_state_t *state);
bool finger_update(finger_state_t *state, uint32_t ir);
