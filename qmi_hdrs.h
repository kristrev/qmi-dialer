#ifndef QMI_HDRS_H
#define QMI_HDRS_H

#include <stdint.h>

//qmux header
struct qmux_header{
    //This value (I/F type) is not really part of the qmux header, but keep it
    //here for convenience
    uint8_t type;
    uint16_t length;
    uint8_t control_flags;
    uint8_t service_type;
    uint8_t client_id;
} __attribute__((packed));

//The two different types of QMI headers I have seen. According to the
//specification, the size of the header is implementation-dependant (depending
//on service)
struct qmi_header_ctl{
    uint8_t control_flags;
    uint8_t transaction_id;
    uint16_t message_id;
    uint16_t length;
} __attribute__((packed));

struct qmi_header_gen{
    uint8_t control_flags;
    uint16_t transaction_id;
    uint16_t message_id;
    uint16_t length;
} __attribute__((packed));

struct qmi_tlv{
    uint8_t type;
    uint16_t length;
} __attribute__((packed));

typedef struct qmux_header qmux_hdr_t;
typedef struct qmi_header_ctl qmi_hdr_ctl_t;
typedef struct qmi_header_gen qmi_hdr_gen_t;
typedef struct qmi_tlv qmi_tlv_t;

#endif
