#ifndef AI_STUB_H
#define AI_STUB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool AI_deploy(uint32_t accelerator_addr, uint64_t accelerator_id);
void AI_predict(int8_t *inputs, size_t num_inputs, int8_t *result, size_t num_results);

#endif
