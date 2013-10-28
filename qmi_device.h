#ifndef QMI_DEVICE_H
#define QMI_DEVICE_H

//Different sates for each service type
enum{
    NAS_INIT,
    NAS_GOT_CID,
};

typedef uint8_t nas_state_t;
typedef uint8_t wds_state_t;
typedef uint8_t dms_state_t;

struct qmi_device{
    uint8_t nas_id;
    nas_state_t nas_state;

    uint8_t wds_id;
    wds_state_t wds_state;

    uint8_t dms_id;
    dms_state_t dms_state;
};

#endif
