#ifndef QMI_PACKETS_H
#define QMI_PACKETS_H

#include <stdint.h>

void create_qmi_request(uint8_t *buf, uint8_t service, uint8_t client_id, 
        uint16_t transaction_id, uint16_t message_id);
void add_tlv(uint8_t *buf, uint8_t type, uint16_t length, void *value);
void parse_qmi(uint8_t *buf);
#endif
