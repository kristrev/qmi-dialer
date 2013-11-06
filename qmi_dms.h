#ifndef QMI_DMS_H
#define QMI_DMS_H

#include <stdint.h>
#include "qmi_shared.h"

#define QMI_DMS_RESET                       0x0000
#define QMI_DMS_VERIFY_PIN                  0x0028
#define QMI_DMS_SET_OPERATING_MODE          0x002E

//Verify PIN TLV
#define QMI_DMS_TLV_VP_VERIFY_PIN           0x01

//Set operating mode TLV
#define QMI_DMS_TLV_OPERATING_MODE          0x01

//This is a generic error code, but only used here. Modem returns NoEffect when
//there is no PIN code
#define QMI_ERR_NO_EFFECT                   0x001A

struct qmi_dms_verify_pin{
    uint8_t pin_id;
    uint8_t pin_len;
    char pin_value[QMID_MAX_LENGTH_PIN]; //Assumes native char is one byte
} __attribute__((packed));

typedef struct qmi_dms_verify_pin qmi_dms_verify_pin_t;

struct qmi_device;

uint8_t qmi_dms_send(struct qmi_device *qmid);
uint8_t qmi_dms_handle_msg(struct qmi_device *qmid);

#endif
