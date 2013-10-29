#ifndef QMI_NAS_H
#define QMI_NAS_H

#include <stdint.h>

//NAS message types 
#define QMI_NAS_INDICATION_REGISTER     0x0003
#define QMI_NAS_GET_SYS_INFO            0x004D
#define QMI_NAS_SYS_INFO_IND            0x004E

//TLVs
#define QMI_NAS_TLV_IND_SYS_INFO        0x18

//Sys info TLV
#define QMI_NAS_TLV_SI_GSM_SS           0x12
#define QMI_NAS_TLV_SI_WCDMA_SS         0x13
#define QMI_NAS_TLV_SI_LTE_SS           0x14

struct qmi_device;

//Handle a ctl message. Returns false if something went wrong
uint8_t qmi_nas_handle_msg(struct qmi_device *qmid);

//Send message based on state in state machine
uint8_t qmi_nas_send(struct qmi_device *qmid);
#endif
