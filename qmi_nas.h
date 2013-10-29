#ifndef QMI_NAS_H
#define QMI_NAS_H

#include <stdint.h>

//NAS message types 
#define QMI_NAS_INDICATION_REGISTER     0x0003

//TLVs
#define QMI_NAS_TLV_IND_SYS_INFO        0x18

struct qmi_device;

//Handle a ctl message. Returns false if something went wrong
uint8_t qmi_nas_handle_msg(struct qmi_device *qmid);

//Send message based on state in state machine
uint8_t qmi_nas_send(struct qmi_device *qmid);
#endif
