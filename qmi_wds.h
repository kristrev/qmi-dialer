#ifndef QMI_WDS_H
#define QMI_WDS_H

#include <stdint.h>

struct qmi_device;

//Handle a WDS message. Returns false if something went wrong
uint8_t qmi_wds_handle_msg(struct qmi_device *qmid);

//Send message based on state in state machine
uint8_t qmi_wds_send(struct qmi_device *qmid);

#endif
