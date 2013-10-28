#ifndef QMI_HDRS_H
#define QMI_HDRS_H

#include <stdint.h>

struct qmux_header{
    //This value (I/F type) is not really part of the qmux header, but keep it
    //here for convenience
    uint8_t type;
    uint16_t length;
    uint8_t control_flags;
    uint8_t service_type;
    uint8_t client_id;
} __attribute__((packed));

typedef struct qmux_header qmux_header_t;

#endif
