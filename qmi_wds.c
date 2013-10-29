#include <stdio.h>
#include <assert.h>

#include "qmi_wds.h"
#include "qmi_device.h"
#include "qmi_shared.h"
#include "qmi_dialer.h"
#include "qmi_hdrs.h"

static inline ssize_t qmi_wds_write(struct qmi_device *qmid, uint8_t *buf,
        ssize_t len){
    //TODO: Only do this if request is sucessful?
    qmid->wds_transaction_id = (qmid->wds_transaction_id + 1) % UINT8_MAX;

    //According to spec, transaction id must be non-zero
    if(!qmid->wds_transaction_id)
        qmid->wds_transaction_id = 1;

    if(qmi_verbose_logging){
        fprintf(stderr, "Will send (WDS):\n");
        parse_qmi(buf);
    }

    //+1 is to include marker
    return qmi_helpers_write(qmid->qmi_fd, buf, len + 1);
}

static uint8_t qmi_wds_send_set_event_report(struct qmi_device *qmid){
    uint8_t buf[QMI_DEFAULT_BUF_SIZE];
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) buf;
    uint8_t enable = 1;

    create_qmi_request(buf, QMI_SERVICE_WDS, qmid->nas_id,
            qmid->nas_transaction_id, QMI_WDS_SET_EVENT_REPORT);
    add_tlv(buf, QMI_WDS_TLV_ER_CUR_DATA_BEARER_IND, sizeof(uint8_t), &enable);

    return qmi_wds_write(qmid, buf, qmux_hdr->length);;
}

uint8_t qmi_wds_send(struct qmi_device *qmid){
    uint8_t retval = QMI_MSG_IGNORE;

    switch(qmid->wds_state){
        case WDS_GOT_CID:
        case WDS_IND_REQ:
            //Failed sends are not that interesting. It will just take longer
            //before the indications will be set up (timeout)
            qmi_wds_send_set_event_report(qmid);
            break;
        default:
            fprintf(stderr, "Unknown wds state (send())\n");
            assert(0);
    }

    return retval;
}

uint8_t qmi_wds_handle_msg(struct qmi_device *qmid){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) qmid->buf;
    qmi_hdr_gen_t *qmi_hdr = (qmi_hdr_gen_t*) (qmux_hdr + 1);
    uint8_t retval = QMI_MSG_IGNORE;
#if 0
    switch(qmi_hdr->message_id){
        case QMI_NAS_INDICATION_REGISTER:
            retval = qmi_nas_handle_ind_req_reply(qmid);
            break;
        case QMI_NAS_GET_SYS_INFO:
        case QMI_NAS_SYS_INFO_IND:
            qmi_nas_handle_sys_info(qmid);
            break;
        default:
            fprintf(stderr, "Unknown NAS message\n");
            break;
    }
#endif
    return retval;
}
