#ifndef _AS608_DRIVER_H_
#define _AS608_DRIVER_H_

#include <stdint.h>
#include <stdbool.h>

bool as608_init(void);
bool as608_enroll_fingerprint(uint16_t storage_position);
bool as608_verify_fingerprint(uint16_t *matched_id, uint16_t *score);

#endif
