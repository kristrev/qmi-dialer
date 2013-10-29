#include <stdio.h>
#include <assert.h>
#include <endian.h>

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
    //TODO: Could be that I do not need any more indications (except signal
    //strength). WDS gives me current technology

    return qmi_ctl_write(qmid, buf, qmux_hdr->length);;
}

static uint8_t qmi_nas_req_sys_info(struct qmi_device *qmid){
    uint8_t buf[QMI_DEFAULT_BUF_SIZE];
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) buf;
 
    create_qmi_request(buf, QMI_SERVICE_NAS, qmid->nas_id,
            qmid->nas_transaction_id, QMI_NAS_GET_SYS_INFO);

    return qmi_ctl_write(qmid, buf, qmux_hdr->length);
}

//Send message based on state in state machine
uint8_t qmi_nas_send(struct qmi_device *qmid){
    uint8_t retval = QMI_MSG_IGNORE;

    switch(qmid->nas_state){
        case NAS_GOT_CID:
        case NAS_IND_REQ:
            retval = qmi_nas_send_indication_request(qmid);
            break;
        case NAS_SYS_INFO_QUERY:
            retval = qmi_nas_req_sys_info(qmid);
            break;
        default:
            fprintf(stderr, "Unknown state");
            assert(0);
            break;
    }

    return retval;
}

static uint8_t qmi_nas_handle_ind_req_reply(struct qmi_device *qmid){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) qmid->buf;
    qmi_hdr_ctl_t *qmi_hdr = (qmi_hdr_ctl_t*) (qmux_hdr + 1);
    qmi_tlv_t *tlv = (qmi_tlv_t*) (qmi_hdr + 1);
    uint16_t result = le16toh(*((uint16_t*) (tlv+1)));

    if(result == QMI_RESULT_FAILURE){
        fprintf(stderr, "Could not register indications\n");
        return QMI_MSG_FAILURE;
    } else {
        qmid->nas_state = NAS_SYS_INFO_QUERY;
        //I don't care about the return value. If something fails, a timeout
        //will make sure the message is resent
        qmi_nas_send(qmid);
        return QMI_MSG_SUCCESS;
    }
}

//No return value needed, as no action will be taken if this message is not
//correct
static void qmi_nas_handle_sys_info(struct qmi_device *qmid){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) qmid->buf;
    qmi_hdr_gen_t *qmi_hdr = (qmi_hdr_gen_t*) (qmux_hdr + 1);
    qmi_tlv_t *tlv = (qmi_tlv_t*) (qmi_hdr + 1);
    uint16_t tlv_length = le16toh(qmi_hdr->length), i = 0;
    uint16_t result = le16toh(*((uint16_t*) (tlv+1)));
    qmi_nas_service_info_t *qsi = NULL;
    uint8_t has_service = 0;

    if(result == QMI_RESULT_FAILURE)
        return;

    //Remove first tlv
    tlv_length = tlv_length - sizeof(qmi_tlv_t) - tlv->length;
    tlv = (qmi_tlv_t*) (((uint8_t*) (tlv+1)) + tlv->length);

    //The goal right now is just to check if one is attached (srv_status !=
    //NO_SERVICE). If so and not connected, start connect. Then I need to figure
    //out how to only get statistics
    while(i<tlv_length){
        if(tlv->type == QMI_NAS_TLV_SI_GSM_SS ||
                tlv->type == QMI_NAS_TLV_SI_WCDMA_SS ||
                tlv->type == QMI_NAS_TLV_SI_LTE_SS){
            qsi = (qmi_nas_service_info_t*) (tlv+1);

            //I only care if one technology gives me service
            if(qsi->srv_status){
                if(qmi_verbose_logging)
                    fprintf(stderr, "Technology %x has service\n", tlv->type);
                has_service = 1;
            }
        }

        i += sizeof(qmi_tlv_t) + tlv->length;

        if(i==tlv_length)
            break;
        else
            tlv = (qmi_tlv_t*) (((uint8_t*) (tlv+1)) + tlv->length);
    }

    //if
    //if service && !connected && !connecting
    //connect
    //else if(!service && connected || connecting)
    //disconnect (maybe not needed)
}

uint8_t qmi_nas_handle_msg(struct qmi_device *qmid){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) qmid->buf;
    qmi_hdr_gen_t *qmi_hdr = (qmi_hdr_gen_t*) (qmux_hdr + 1);
    uint8_t retval = QMI_MSG_IGNORE;

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

    return retval;
}
