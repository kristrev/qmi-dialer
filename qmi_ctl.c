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
#include "qmi_nas.h"
#include "qmi_wds.h"

static inline ssize_t qmi_ctl_write(struct qmi_device *qmid, uint8_t *buf,
        ssize_t len){
    //TODO: Only do this if request is sucessful?
    qmid->ctl_transaction_id = (qmid->ctl_transaction_id + 1) % UINT8_MAX;

    //According to spec, transaction id must be non-zero
    if(!qmid->ctl_transaction_id)
        qmid->ctl_transaction_id = 1;

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_3){
        QMID_DEBUG_PRINT(stderr, "Will send (CTL):\n");
        parse_qmi(buf);
    }

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

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
        if(release)
            QMID_DEBUG_PRINT(stderr, "Releasing CID %x for service %x\n",
                    cid, service);
        else
            QMID_DEBUG_PRINT(stderr, "Requesting CID for service %x\n",
                    service);

    if(release)
        add_tlv(buf, QMI_CTL_TLV_ALLOC_INFO, sizeof(uint16_t), &tlv_value);
    else
        add_tlv(buf, QMI_CTL_TLV_ALLOC_INFO, sizeof(uint8_t), &service);

    retval = qmi_ctl_write(qmid, buf, qmux_hdr->length);

    if(retval <= 0)
        if(qmid_verbose_logging >= QMID_LOG_LEVEL_1)
            QMID_DEBUG_PRINT(stderr, "Failed to send request for CID for %x\n",
                    service);

    return retval;
}

ssize_t qmi_ctl_send_sync(struct qmi_device *qmid){
    uint8_t buf[QMI_DEFAULT_BUF_SIZE];
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) buf;

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
        QMID_DEBUG_PRINT(stderr, "Seding sync request\n");

    create_qmi_request(buf, QMI_SERVICE_CTL, 0, qmid->ctl_transaction_id,
            QMI_CTL_SYNC);

    return qmi_ctl_write(qmid, buf, qmux_hdr->length);
}

//Return false is something went wrong (typically no available CID)
static uint8_t qmi_ctl_handle_cid_reply(struct qmi_device *qmid){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) qmid->buf;
    qmi_hdr_ctl_t *qmi_hdr = (qmi_hdr_ctl_t*) (qmux_hdr + 1);
    qmi_tlv_t *tlv = (qmi_tlv_t*) (qmi_hdr + 1);
    uint16_t *result = NULL;
    uint8_t service = 0, cid = 0;

    //A CID reply has two TLVs. First is always the result of the operation
    result = (uint16_t*) (tlv+1);

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
        QMID_DEBUG_PRINT(stderr, "Received CID get/release reply\n");

    //TODO: Improve logic so that I know which service this is?
    if(le16toh(*result) == QMI_RESULT_FAILURE){
        if(qmid_verbose_logging >= QMID_LOG_LEVEL_1)
            QMID_DEBUG_PRINT(stderr, "Failed to get a CID for service %x\n",
                    service);
        return QMI_MSG_FAILURE;
    }

    //Get the CID
    tlv = (qmi_tlv_t*) (((uint8_t*) (tlv+1)) + tlv->length);
    service = *((uint8_t*) (tlv+1));
    cid = *(((uint8_t*) (tlv+1)) + 1);

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_1)
        QMID_DEBUG_PRINT(stderr, "Service %x got cid %u\n", service, cid);

    switch(service){
        case QMI_SERVICE_DMS:
            qmid->dms_id = cid;
            qmid->dms_state = DMS_GOT_CID;
            //qmi_dms_send(qmid);
            break;
        case QMI_SERVICE_WDS:
            qmid->wds_id = cid;
            qmid->wds_state = WDS_GOT_CID;
            qmi_wds_send(qmid);
            break;
        case QMI_SERVICE_NAS:
            qmid->nas_id = cid;
            qmid->nas_state = NAS_GOT_CID;
            qmi_nas_send(qmid);
            break;
        default:
            if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
                QMID_DEBUG_PRINT(stderr, "CID for service not handled by "
                        "qmid\n");
            break;
    }

    return QMI_MSG_SUCCESS;
}

static uint8_t qmi_ctl_request_cid(struct qmi_device *qmid);
static uint8_t qmi_ctl_handle_sync_reply(struct qmi_device *qmid){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) qmid->buf;
    qmi_hdr_ctl_t *qmi_hdr = (qmi_hdr_ctl_t*) (qmux_hdr + 1);
    qmi_tlv_t *tlv = (qmi_tlv_t*) (qmi_hdr + 1);
    uint16_t result = *((uint16_t*) (tlv+1));

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
        QMID_DEBUG_PRINT(stderr, "Received SYNC reply\n");

    //All the "rouge" SYNC messages seem to have transaction_id == 0. Use that
    //for now, see if it is consistent or not. I know that I only send one sync
    //message with ID 1, so ignore all SYNC messages that does not have this ID
    if(qmi_hdr->transaction_id != 1 || qmid->ctl_state == CTL_SYNCED){
        if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
            QMID_DEBUG_PRINT(stderr, "Ignoring sync packet from modem. %u %u\n",
                    qmi_hdr->transaction_id, qmid->ctl_state);
        return QMI_MSG_IGNORE;
    }

    if(result == QMI_RESULT_FAILURE){
        if(qmid_verbose_logging >= QMID_LOG_LEVEL_1)
            QMID_DEBUG_PRINT(stderr, "Sync operation failed\n");
        return QMI_MSG_FAILURE;
    } else{
        qmid->ctl_state = CTL_SYNCED;

        //This can be viewed as the proper start of the dialer. After
        //getting the sync reply, request cid for each service I will use
        return qmi_ctl_request_cid(qmid);
    }
}

static uint8_t qmi_ctl_request_cid(struct qmi_device *qmid){
    //TODO: Add to timeout
    if(!qmid->nas_id &&
            qmi_ctl_update_cid(qmid, QMI_SERVICE_NAS, false, 0) <= 0)
        return QMI_MSG_FAILURE;

    if(!qmid->wds_id &&
            qmi_ctl_update_cid(qmid, QMI_SERVICE_WDS, false, 0) <= 0)
        return QMI_MSG_FAILURE;

    if(!qmid->dms_id &&
            qmi_ctl_update_cid(qmid, QMI_SERVICE_DMS, false, 0) <= 0)
        return QMI_MSG_FAILURE;

    return QMI_MSG_SUCCESS;
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

            retval = qmi_ctl_handle_cid_reply(qmid);
            break;
        case QMI_CTL_SYNC:
            //TODO: I suspected some of these packages are sent by the modem
            //every now and then. Check up on that and perhaps have a check on
            //state
            retval = qmi_ctl_handle_sync_reply(qmid);
            break;
        default:
            if(qmid_verbose_logging >= QMID_LOG_LEVEL_3)
                QMID_DEBUG_PRINT(stderr, "Unknown CTL message of type %x\n",
                        qmi_hdr->message_id);
            retval = QMI_MSG_IGNORE;
            break;
    }

    return retval;
}
