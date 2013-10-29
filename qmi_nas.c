#include <stdio.h>
#include <assert.h>

#include "qmi_nas.h"
#include "qmi_device.h"
#include "qmi_shared.h"
#include "qmi_hdrs.h"
#include "qmi_dialer.h"

static inline ssize_t qmi_ctl_write(struct qmi_device *qmid, uint8_t *buf,
        ssize_t len){
    //TODO: Only do this if request is sucessful?
    qmid->nas_transaction_id = (qmid->nas_transaction_id + 1) % UINT8_MAX;

    //According to spec, transaction id must be non-zero
    if(!qmid->nas_transaction_id)
        qmid->nas_transaction_id = 1;

    if(qmi_verbose_logging){
        fprintf(stderr, "Will send:\n");
        parse_qmi(buf);
    }

    //+1 is to include marker
    return qmi_helpers_write(qmid->qmi_fd, buf, len + 1);
}

static uint8_t qmi_nas_send_indication_request(struct qmi_device *qmid){
    uint8_t buf[QMI_DEFAULT_BUF_SIZE];
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) buf;
    uint8_t enable = 1;

    create_qmi_request(buf, QMI_SERVICE_NAS, qmid->nas_id,
            qmid->nas_transaction_id, QMI_NAS_INDICATION_REGISTER);
    add_tlv(buf, QMI_NAS_TLV_IND_SYS_INFO, sizeof(uint8_t), &enable);

    return qmi_ctl_write(qmid, buf, qmux_hdr->length);;
}

//Send message based on state in state machine
uint8_t qmi_nas_send(struct qmi_device *qmid){
    uint8_t retval = QMI_MSG_IGNORE;

    switch(qmid->nas_state){
        case NAS_GOT_CID:
        case NAS_IND_REQ:
            retval = qmi_nas_send_indication_request(qmid);
            break;
        default:
            fprintf(stderr, "Unknown state");
            assert(0);
            break;
    }

    return retval;
}

