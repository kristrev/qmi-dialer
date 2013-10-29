#ifndef QMI_WDS_H
#define QMI_WDS_H

#include <stdint.h>

//Message types
#define QMI_WDS_SET_EVENT_REPORT            0x0001
#define QMI_WDS_EVENT_REPORT_IND            QMI_WDS_SET_EVENT_REPORT

//Event report TLVs
#define QMI_WDS_TLV_ER_CUR_DATA_BEARER_IND  0x15

struct qmi_device;

//Handle a WDS message. Returns false if something went wrong
uint8_t qmi_wds_handle_msg(struct qmi_device *qmid);

//Send message based on state in state machine
uint8_t qmi_wds_send(struct qmi_device *qmid);

#endif
