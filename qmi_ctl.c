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
    return qmi_helpers_write(qmid->qmi_fd, buf, len);
}

ssize_t qmi_ctl_update_cid(struct qmi_device *qmid, uint8_t service,
        bool release, uint8_t cid){
    uint8_t buf[QMI_DEFAULT_BUF_SIZE];
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) buf;
    uint16_t message_id = release ? QMI_CTL_RELEASE_CID : QMI_CTL_GET_CID;
    uint16_t tlv_value = htole16((cid << 8) | service);

    create_qmi_request(buf, QMI_SERVICE_CTL, 0, qmid->ctl_transaction_id,
            message_id);
    add_tlv(buf, QMI_CTL_TLV_ALLOC_INFO, sizeof(uint16_t), &tlv_value);

    if(qmi_verbose_logging){
        fprintf(stderr, "Will send:\n");
        parse_qmi(buf);
    }

    return qmi_ctl_write(qmid, buf, qmux_hdr->length);
}
