#ifndef QMI_DEVICE_H
#define QMI_DEVICE_H

#include "qmi_shared.h"

//Different sates for each service type
enum{
    CTL_NOT_SYNCED = 0,
    CTL_SYNCED,
};

//NAS state machine
enum{
    NAS_INIT = 0,
    //Got CID (this is the starting point)
    NAS_GOT_CID,
    //Indication request is sent
    NAS_IND_REQ,
    //Indication request received, so query system information to check for
    //attached state
    NAS_SYS_INFO_QUERY,
    //NAS is done (only new messages to send will be specified by a timeout)
    NAS_IDLE,
};

enum{
    WDS_INIT = 0,
    WDS_GOT_CID,
};

enum{
    DMS_INIT = 0,
    DMS_GOT_CID,
};

typedef uint8_t ctl_state_t;
typedef uint8_t nas_state_t;
typedef uint8_t wds_state_t;
typedef uint8_t dms_state_t;

struct qmi_device{
    int32_t qmi_fd;

    //Buffer has to be persistent accross calls to recv
    uint16_t qmux_progress;
    uint16_t cur_qmux_length;
    uint8_t buf[QMI_DEFAULT_BUF_SIZE];

    //Values independent for each service
    //According to the documentation (QMI architecture), a control point must
    //increment transaction id for each message it sends.
    uint8_t ctl_transaction_id;
    ctl_state_t ctl_state;

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
