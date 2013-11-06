#include <stdint.h>
#include <string.h>

#include "qmi_dialer.h"
#include "qmi_helpers.h"
#include "qmi_hdrs.h"
#include "qmi_dms.h"
#include "qmi_device.h"
#include "qmi_wds.h"

static inline ssize_t qmi_dms_write(struct qmi_device *qmid, uint8_t *buf,
        ssize_t len){
    //TODO: Only do this if request is sucessful?
    qmid->dms_transaction_id = (qmid->dms_transaction_id + 1) % UINT8_MAX;

    //According to spec, transaction id must be non-zero
    if(!qmid->dms_transaction_id)
        qmid->dms_transaction_id = 1;

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_3){
        QMID_DEBUG_PRINT(stderr, "Will send (NAS):\n");
        parse_qmi(buf);
    }

    qmid->dms_sent_time = time(NULL);

    //+1 is to include marker
    //len is passed as qmux_hdr->length, which is store as little endian
    return qmi_helpers_write(qmid->qmi_fd, buf, le16toh(len) + 1);
}

static ssize_t qmi_dms_send_reset(struct qmi_device *qmid){
    uint8_t buf[QMI_DEFAULT_BUF_SIZE];
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) buf;

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
        QMID_DEBUG_PRINT(stderr, "Resetting DMS\n");

    create_qmi_request(buf, QMI_SERVICE_DMS, qmid->dms_id,
            qmid->dms_transaction_id, QMI_DMS_RESET);
    qmid->dms_state = NAS_RESET;

    return qmi_dms_write(qmid, buf, qmux_hdr->length);
}

static ssize_t qmi_dms_verify_pin(struct qmi_device *qmid){
    uint8_t buf[QMI_DEFAULT_BUF_SIZE];
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) buf;
    qmi_dms_verify_pin_t uvp;

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
        QMID_DEBUG_PRINT(stderr, "Will verify PIN\n");

    memset(&uvp, 0, sizeof(uvp));

    //For now, I am only interested in serivce 0 (Primary GW provision), so all
    //values in usi are zero (see QMI documentation for usi description)
    uvp.pin_id = 1;
    uvp.pin_len = strlen(qmid->pin_code);
    memcpy(uvp.pin_value, qmid->pin_code, uvp.pin_len);

    create_qmi_request(buf, QMI_SERVICE_DMS, qmid->dms_id,
            qmid->dms_transaction_id, QMI_DMS_VERIFY_PIN);
    //TODO: Add another add_tlv method, which returns a pointer to the data area
    //of TLV. Would get rid of a lot of memcpy's
    //Subtraction is needed since uvp contains room for maximum pin length, and
    //we should only copy the actual pin code length
    add_tlv(buf, QMI_DMS_TLV_VP_VERIFY_PIN, sizeof(qmi_dms_verify_pin_t) -
            (QMID_MAX_LENGTH_PIN - uvp.pin_len), &uvp);

    return qmi_dms_write(qmid, buf, qmux_hdr->length);
}

static ssize_t qmi_dms_set_oper_mode(struct qmi_device *qmid,
        uint8_t oper_mode){
    uint8_t buf[QMI_DEFAULT_BUF_SIZE];
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) buf;

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
        QMID_DEBUG_PRINT(stderr, "Setting operating mode to %u\n", oper_mode);

    create_qmi_request(buf, QMI_SERVICE_DMS, qmid->dms_id,
            qmid->dms_transaction_id, QMI_DMS_SET_OPERATING_MODE);
    add_tlv(buf, QMI_DMS_TLV_OPERATING_MODE, sizeof(uint8_t), &oper_mode);

    return qmi_dms_write(qmid, buf, qmux_hdr->length);
}

uint8_t qmi_dms_send(struct qmi_device *qmid){
    uint8_t retval = QMI_MSG_IGNORE;

    switch(qmid->dms_state){
        case DMS_GOT_CID:
        case DMS_RESET:
            qmi_dms_send_reset(qmid);
            break;
        case DMS_VERIFY_PIN:
            qmi_dms_verify_pin(qmid);
            break;
        default:
            break;
    } 
    return retval;
}

static uint8_t qmi_dms_handle_reset(struct qmi_device *qmid){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) qmid->buf;
    qmi_hdr_gen_t *qmi_hdr = (qmi_hdr_gen_t*) (qmux_hdr + 1);
    qmi_tlv_t *tlv = (qmi_tlv_t*) (qmi_hdr + 1);
    uint16_t result = le16toh(*((uint16_t*) (tlv+1)));

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
        QMID_DEBUG_PRINT(stderr, "Received DMS_RESET_RESP\n");

    if(result == QMI_RESULT_FAILURE){
        if(qmid_verbose_logging >= QMID_LOG_LEVEL_1)
            QMID_DEBUG_PRINT(stderr, "Could not reset DMS\n");
        return QMI_MSG_FAILURE;
    } else {
        if(qmid_verbose_logging >= QMID_LOG_LEVEL_1)
            QMID_DEBUG_PRINT(stderr, "DMS is reset\n");
        qmid->dms_state = DMS_VERIFY_PIN;
        qmi_dms_send(qmid);
        return QMI_MSG_SUCCESS;
    }
}

static uint8_t qmi_dms_handle_verify_pin(struct qmi_device *qmid){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) qmid->buf;
    qmi_hdr_gen_t *qmi_hdr = (qmi_hdr_gen_t*) (qmux_hdr + 1);
    qmi_tlv_t *tlv = (qmi_tlv_t*) (qmi_hdr + 1);
    uint16_t result = le16toh(*((uint16_t*) (tlv+1)));
    //TODO: Consider adding a generic qmi error struct, if I have to extract the
    //error code in more places
    uint16_t err_code = le16toh(*(((uint16_t*) (tlv+1))+1));

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
        QMID_DEBUG_PRINT(stderr, "Received DMS_VERIFY_PIN\n");

    //No effect is returned when SIM card has no PIN code
    if(result == QMI_RESULT_FAILURE && err_code != QMI_ERR_NO_EFFECT){
        if(qmid_verbose_logging >= QMID_LOG_LEVEL_1)
            QMID_DEBUG_PRINT(stderr, "Could not verify PIN\n");

        parse_qmi(qmid->buf);
        return QMI_MSG_FAILURE;
    }

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_1)
        QMID_DEBUG_PRINT(stderr, "PIN is verified, SIM card unlocked\n");
    
    qmid->dms_state = DMS_IDLE;
    qmid->pin_unlocked = 1;
    qmi_wds_update_connect(qmid);

    return QMI_MSG_SUCCESS;
}

uint8_t qmi_dms_handle_msg(struct qmi_device *qmid){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) qmid->buf;
    qmi_hdr_gen_t *qmi_hdr = (qmi_hdr_gen_t*) (qmux_hdr + 1);
    uint8_t retval = QMI_MSG_IGNORE;

    switch(le16toh(qmi_hdr->message_id)){
        case QMI_DMS_RESET:
            retval = qmi_dms_handle_reset(qmid);
            break;
        case QMI_DMS_VERIFY_PIN:
            retval = qmi_dms_handle_verify_pin(qmid);
            break;
        default:
            if(qmid_verbose_logging >= QMID_LOG_LEVEL_3)
                QMID_DEBUG_PRINT(stderr, "Unknown DMS packet of type %x\n",
                        qmi_hdr->message_id);
            break;
    }

    return retval;
}
