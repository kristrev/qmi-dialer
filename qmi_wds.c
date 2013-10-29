#include <stdio.h>
#include <assert.h>
#include <endian.h>
#include <string.h>

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


static uint8_t qmi_wds_connect(struct qmi_device *qmid){
    uint8_t buf[QMI_DEFAULT_BUF_SIZE];
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) buf;
    char *apn = "internet";

    create_qmi_request(buf, QMI_SERVICE_WDS, qmid->wds_id,
            qmid->wds_transaction_id, QMI_WDS_START_NETWORK_INTERFACE);
    add_tlv(buf, QMI_WDS_TLV_SNI_APN_NAME, strlen(apn), apn);

    //This is so far the only critical write I have. However, I will not do
    //anything right now, the next connect will be controlled by a timeout
    if(qmi_wds_write(qmid, buf, qmux_hdr->length) == qmux_hdr->length + 1){
        qmid->wds_state = WDS_CONNECTING;
        return QMI_MSG_SUCCESS;
    } else
        return QMI_MSG_FAILURE;
}

uint8_t qmi_wds_update_connect(struct qmi_device *qmid){
    //Establish a new connection if I have service and a connection is not
    //already established, or in progress
    //TODO: Add a check for PIN
    if(qmid->has_service && qmid->wds_state == WDS_DISCONNECTED){
        qmi_wds_connect(qmid);
    } else if((qmid->wds_state == WDS_CONNECTED ||
                qmid->wds_state == WDS_CONNECTING) && !qmid->has_service){
        //Lost service, might disconnect
        //TODO: Check if this one is really needed
    }
    return 0;
}

static uint8_t qmi_wds_send_set_event_report(struct qmi_device *qmid){
    uint8_t buf[QMI_DEFAULT_BUF_SIZE];
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) buf;
    uint8_t enable = 1;

    create_qmi_request(buf, QMI_SERVICE_WDS, qmid->wds_id,
            qmid->wds_transaction_id, QMI_WDS_SET_EVENT_REPORT);
    add_tlv(buf, QMI_WDS_TLV_ER_CUR_DATA_BEARER_IND, sizeof(uint8_t), &enable);
    qmid->wds_state = WDS_IND_REQ;

    return qmi_wds_write(qmid, buf, qmux_hdr->length);
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
        case WDS_DISCONNECTED:
            //wds_send also needs to support disconnect, but this function
            //should only be called from within wds state machine (and only when
            //disconnected is actually set)
            printf("qmi_wds_connect will be called\n");
            break;
        default:
            fprintf(stderr, "Nothing to send for WDS\n");
            break;
    }

    return retval;
}

static uint8_t qmi_wds_handle_event_report(struct qmi_device *qmid){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) qmid->buf;
    qmi_hdr_gen_t *qmi_hdr = (qmi_hdr_gen_t*) (qmux_hdr + 1);
    qmi_tlv_t *tlv = (qmi_tlv_t*) (qmi_hdr + 1);
    uint16_t tlv_length = le16toh(qmi_hdr->length), i = 0;
    uint16_t result = le16toh(*((uint16_t*) (tlv+1)));
    uint8_t has_service = 0;
    uint8_t retval = QMI_MSG_IGNORE;

    if(result == QMI_RESULT_FAILURE)
        return QMI_MSG_FAILURE;

    //WDS is configured and ready to connect
    //EventReport is both used as the indication AND the reply for the intial
    //request. I need to make sure I dont mess up the state, so only set it for
    //the request (WDS can only move into >= PKT_SRVC_QUERY from here)
    if(qmid->wds_state < WDS_DISCONNECTED){
        qmid->wds_state = WDS_DISCONNECTED;
        retval = QMI_MSG_SUCCESS;
    }

    //Remove first tlv
    tlv_length = tlv_length - sizeof(qmi_tlv_t) - tlv->length;
    tlv = (qmi_tlv_t*) (((uint8_t*) (tlv+1)) + tlv->length);

    while(i<tlv_length){
        printf("TLV type: %x\n", tlv->type);

        i += sizeof(qmi_tlv_t) + tlv->length;

        if(i==tlv_length)
            break;
        else
            tlv = (qmi_tlv_t*) (((uint8_t*) (tlv+1)) + tlv->length);
    }

    return retval;
}

uint8_t qmi_wds_handle_msg(struct qmi_device *qmid){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) qmid->buf;
    qmi_hdr_gen_t *qmi_hdr = (qmi_hdr_gen_t*) (qmux_hdr + 1);
    uint8_t retval = QMI_MSG_IGNORE;
    
    switch(qmi_hdr->message_id){
        //This one also covers the reply to the set event report
        case QMI_WDS_EVENT_REPORT_IND:
            retval = qmi_wds_handle_event_report(qmid);
            if(retval == QMI_MSG_SUCCESS)
                qmi_wds_send(qmid);
            break;
        default:
            fprintf(stderr, "Unknown WDS message\n");
            break;
    }
    
    return retval;
}
