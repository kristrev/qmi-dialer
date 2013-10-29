#include <stdio.h>
#include <endian.h>
#include <stdint.h>
#include <stdbool.h>

#include "qmi_dialer.h"
#include "qmi_ctl.h"
#include "qmi_hdrs.h"
#include "qmi_shared.h"
#include "qmi_helpers.h"
#include "qmi_device.h"

static inline ssize_t qmi_ctl_write(struct qmi_device *qmid, uint8_t *buf,
        ssize_t len){
    //TODO: Only do this if request is sucessful?
    qmid->ctl_transaction_id = (qmid->ctl_transaction_id + 1) % UINT8_MAX;

    //According to spec, transaction id must be non-zero
    if(!qmid->ctl_transaction_id)
        qmid->ctl_transaction_id = 1;

    //+1 is to include marker
    return qmi_helpers_write(qmid->qmi_fd, buf, len + 1);
}

ssize_t qmi_ctl_update_cid(struct qmi_device *qmid, uint8_t service,
        bool release, uint8_t cid){
    uint8_t buf[QMI_DEFAULT_BUF_SIZE];
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) buf;
    uint16_t message_id = release ? QMI_CTL_RELEASE_CID : QMI_CTL_GET_CID;
    //TODO: Perhaps make this nicer, sinceit is only used in one case
    uint16_t tlv_value = htole16((cid << 8) | service);
    ssize_t retval = 0;

    create_qmi_request(buf, QMI_SERVICE_CTL, 0, qmid->ctl_transaction_id,
            message_id);

    if(release)
        add_tlv(buf, QMI_CTL_TLV_ALLOC_INFO, sizeof(uint16_t), &tlv_value);
    else
        add_tlv(buf, QMI_CTL_TLV_ALLOC_INFO, sizeof(uint8_t), &service);

    if(qmi_verbose_logging){
        fprintf(stderr, "Will send (update cid):\n");
        parse_qmi(buf);
    }

    retval = qmi_ctl_write(qmid, buf, qmux_hdr->length);

    if(retval <= 0)
        fprintf(stderr, "Failed to send request for CID for %x\n", service);


    printf("Retval %zd\n", retval);
    return retval;
}

ssize_t qmi_ctl_send_sync(struct qmi_device *qmid){
    uint8_t buf[QMI_DEFAULT_BUF_SIZE];
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) buf;

    create_qmi_request(buf, QMI_SERVICE_CTL, 0, qmid->ctl_transaction_id,
            QMI_CTL_SYNC);

    if(qmi_verbose_logging){
        fprintf(stderr, "Will send (send sync):\n");
        parse_qmi(buf);
    }

    return qmi_ctl_write(qmid, buf, qmux_hdr->length);
}

//Return false is something went wrong (typically no available CID)
static bool qmi_ctl_handle_cid_reply(struct qmi_device *qmid){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) qmid->buf;
    qmi_hdr_ctl_t *qmi_hdr = (qmi_hdr_ctl_t*) (qmux_hdr + 1);
    qmi_tlv_t *tlv = (qmi_tlv_t*) (qmi_hdr + 1);
    uint16_t *result = NULL;
    uint8_t service = 0, cid = 0;

    //A CID reply has two TLVs. First is always the result of the operation
    result = (uint16_t*) (tlv+1);

    //TODO: Improve logic so that I know which service this is?
    if(le16toh(*result) == QMI_RESULT_FAILURE){
        fprintf(stderr, "Failed to get a CID, aborting\n");
        return false;
    }

    //Get the CID
    tlv = (qmi_tlv_t*) (((uint8_t*) (tlv+1)) + tlv->length);
    service = *((uint8_t*) (tlv+1));
    cid = *(((uint8_t*) (tlv+1)) + 1);

    if(qmi_verbose_logging)
        fprintf(stderr, "Service %x got cid %u\n", service, cid);

    switch(service){
        case QMI_SERVICE_DMS:
            qmid->dms_id = cid;
            break;
        case QMI_SERVICE_WDS:
            qmid->wds_id = cid;
            break;
        case QMI_SERVICE_NAS:
            qmid->nas_id = cid;
            break;
        default:
            fprintf(stderr, "CID for service not handled by qmid\n");
            break;
    }

    return true;
}

static uint8_t qmi_ctl_handle_sync_reply(struct qmi_device *qmid){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) qmid->buf;
    qmi_hdr_ctl_t *qmi_hdr = (qmi_hdr_ctl_t*) (qmux_hdr + 1);
    qmi_tlv_t *tlv = (qmi_tlv_t*) (qmi_hdr + 1);
    uint16_t result = *((uint16_t*) (tlv+1));

    //All the "rouge" SYNC messages seem to have transaction_id == 0. Use that
    //for now, see if it is consistent or not. I know that I only send one sync
    //message with ID 1, so ignore all SYNC messages that does not have this ID
    if(qmi_hdr->transaction_id != 1 || qmid->ctl_state == CTL_SYNCED){
        if(qmi_verbose_logging)
            fprintf(stderr, "Ignoring sync pacet from modem. %u %u\n",
                    qmi_hdr->transaction_id, qmid->ctl_state);
        return QMI_MSG_IGNORE;
    }

    if(result == QMI_RESULT_FAILURE){
        fprintf(stderr, "Sync operation failed\n");
        return QMI_MSG_FAILURE;
    } else{
        qmid->ctl_state = CTL_SYNCED;
        return QMI_MSG_SUCCESS;
    }
}

static bool qmi_ctl_request_cid(struct qmi_device *qmid){
    if(qmi_ctl_update_cid(qmid, QMI_SERVICE_NAS, false, 0) <= 0)
        return false;

    if(qmi_ctl_update_cid(qmid, QMI_SERVICE_WDS, false, 0) <= 0)
        return false;

    if(qmi_ctl_update_cid(qmid, QMI_SERVICE_DMS, false, 0) <= 0)
        return false;

    return true;
}

uint8_t qmi_ctl_handle_msg(struct qmi_device *qmid){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) qmid->buf;
    qmi_hdr_ctl_t *qmi_hdr = (qmi_hdr_ctl_t*) (qmux_hdr + 1);
    uint8_t retval;

    switch(qmi_hdr->message_id){
        case QMI_CTL_GET_CID:
        case QMI_CTL_RELEASE_CID:
            //Do not set any values unless I am synced
            if(qmid->ctl_state == CTL_NOT_SYNCED){
                retval = QMI_MSG_IGNORE;
                break;
            }

            if(qmi_verbose_logging)
                fprintf(stdout, "CTL: CID get/release reply\n");

            retval = qmi_ctl_handle_cid_reply(qmid);
            break;
        case QMI_CTL_SYNC:
            //TODO: I suspected some of these packages are sent by the modem
            //every now and then. Check up on that and perhaps have a check on
            //state
            if(qmi_verbose_logging)
                fprintf(stdout, "CTL: SYNC reply\n");

            retval = qmi_ctl_handle_sync_reply(qmid);

            //if(retval == QMI_MSG_SUCCESS)
                //retval = qmi_ctl_request_cid(qmid);
            break;
        default:
            fprintf(stderr, "No handle for message of type %x\n",
                    qmi_hdr->message_id);
            retval = QMI_MSG_IGNORE;
            break;
    }

    return retval;
}
