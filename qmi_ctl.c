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

    create_qmi_request(buf, QMI_SERVICE_CTL, 0, qmid->ctl_transaction_id,
            message_id);

    if(release)
        add_tlv(buf, QMI_CTL_TLV_ALLOC_INFO, sizeof(uint16_t), &tlv_value);
    else
        add_tlv(buf, QMI_CTL_TLV_ALLOC_INFO, sizeof(uint8_t), &service);

    if(qmi_verbose_logging){
        fprintf(stderr, "Will send:\n");
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

bool qmi_ctl_handle_msg(struct qmi_device *qmid){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) qmid->buf;
    qmi_hdr_ctl_t *qmi_hdr = (qmi_hdr_ctl_t*) (qmux_hdr + 1);
    bool retval = false;

    switch(qmi_hdr->message_id){
        case QMI_CTL_GET_CID:
        case QMI_CTL_RELEASE_CID:
            retval = qmi_ctl_handle_cid_reply(qmid);
            break;
        default:
            fprintf(stderr, "No handle for message of type %x\n",
                    qmi_hdr->message_id);
            break;
    }

    return retval;
}
