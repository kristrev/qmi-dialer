#ifndef QMI_DEVICE_H
#define QMI_DEVICE_H

#include "qmi_shared.h"

//Different sates for each service type
enum{
    NAS_INIT,
    NAS_GOT_CID,
};

typedef uint8_t nas_state_t;
typedef uint8_t wds_state_t;
typedef uint8_t dms_state_t;

struct qmi_device{
    int32_t qmi_fd;

    //Buffer has to be persistent accross calls to recv
    uint16_t recv_progress;
    uint16_t recv_length;
    uint8_t buf[QMI_DEFAULT_BUF_SIZE];

    //Values independent for each service
    //According to the documentation (QMI architecture), a control point must
    //increment transaction id for each message it sends.
    uint8_t ctl_transaction_id;

    uint8_t nas_id;
    nas_state_t nas_state;
    uint16_t nas_transaction_id;

    uint8_t wds_id;
    wds_state_t wds_state;
    uint16_t wds_transaction_id;

    uint8_t dms_id;
    dms_state_t dms_state;
    uint16_t dms_transaction_id;
};

#endif
