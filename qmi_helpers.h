#ifndef QMI_PACKETS_H
#define QMI_PACKETS_H

#include <stdint.h>
#include <sys/types.h>

//transaction_id and message_id is assumed to be received in host byte order
void create_qmi_request(uint8_t *buf, uint8_t service, uint8_t client_id, 
        uint16_t transaction_id, uint16_t message_id);

//Remember that value has to been in little endian. Length is converted by this
//unction
void add_tlv(uint8_t *buf, uint8_t type, uint16_t length, void *value);
void parse_qmi(uint8_t *buf);
ssize_t qmi_helpers_write(int32_t qmi_fd, uint8_t *buf, ssize_t len);
int qmi_helpers_set_link(char *ifname, uint8_t up);
#endif
