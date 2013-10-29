#ifndef QMI_NAS_H
#define QMI_NAS_H

#include <stdint.h>

struct qmi_device;

//Handle a ctl message. Returns false if something went wrong
uint8_t qmi_nas_handle_msg(struct qmi_device *qmid);

//Send message based on state in state machine
uint8_t qmi_nas_sendmsg(struct qmi_device *qmid);
#endif
