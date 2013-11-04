#ifndef QMI_DEVICE_H
#define QMI_DEVICE_H

#include <stdint.h>
#include <time.h>

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
    //Reset NAS before droing any configuration
    NAS_RESET,
    //Lock to a given mode (optional state)
    NAS_SET_SYSTEM,
    //Indication request is sent
    NAS_IND_REQ,
    //Indication request received, so query system information to check for
    //attached state
    NAS_SYS_INFO_QUERY,
    //NAS is done (only new messages to send will be specified by a timeout)
    NAS_IDLE,
};

//WDS state machine
enum{
    WDS_INIT = 0,
    WDS_GOT_CID,
    WDS_RESET,
    WDS_IND_REQ,
    //Ready to start connection
    WDS_DISCONNECTED,
    //Connection
    WDS_CONNECTING,
    //Successful connect. If connection fails, state will jump back to
    //DISCONNECTED
    WDS_CONNECTED,
    WDS_DISCONNECTING,
};

//DMS state machine
enum{
    DMS_INIT = 0,
    DMS_GOT_CID,
    DMS_RESET,
    DMS_VERIFY_PIN,
    DMS_IDLE
};

//The different values for the current service
enum{
    NO_SERVICE = 0,
    SERVICE_GSM,
    SERVICE_UMTS,
    SERVICE_LTE,
};

typedef uint8_t ctl_state_t;
typedef uint8_t nas_state_t;
typedef uint8_t wds_state_t;
typedef uint8_t dms_state_t;
typedef uint8_t cur_service_t;
typedef uint8_t cur_subservice_t;

struct qmi_device{
    char *dev_path;
    char *apn_name;
    char *pin_code;

    int32_t qmi_fd;

    //Buffer has to be persistent accross calls to recv
    uint16_t qmux_progress;
    uint16_t cur_qmux_length;
    uint8_t buf[QMI_DEFAULT_BUF_SIZE];

    uint8_t pin_unlocked;

    //Service is main service (GSM, UMTS, LTE)
    //Subservice is the type of connection, will only really matter for UMTS
    //(HSDPA, HSUPA +++). Even though the rat-mask is defined as a int, the
    //flags I need to store is in the lowest byte
    cur_service_t cur_service;
    cur_subservice_t cur_subservice;

    //Values independent for each service
    //According to the documentation (QMI architecture), a control point must
    //increment transaction id for each message it sends.
    uint8_t ctl_num_cids;
    uint8_t ctl_transaction_id;
    ctl_state_t ctl_state;

    uint8_t nas_id;
    nas_state_t nas_state;
    uint16_t nas_transaction_id;
    time_t nas_sent_time;

    uint8_t wds_id;
    wds_state_t wds_state;
    uint16_t wds_transaction_id;
    time_t wds_sent_time;

    uint8_t dms_id;
    dms_state_t dms_state;
    uint16_t dms_transaction_id;
    time_t dms_sent_time;

    //Handle used to stop connection
    uint32_t pkt_data_handle;
};

#endif
