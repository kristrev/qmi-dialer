#include <stdio.h>
#include <assert.h>
#include <endian.h>

#include "qmi_nas.h"
#include "qmi_device.h"
#include "qmi_shared.h"
#include "qmi_hdrs.h"
#include "qmi_dialer.h"
#include "qmi_helpers.h"

static inline ssize_t qmi_nas_write(struct qmi_device *qmid, uint8_t *buf,
        ssize_t len){
    //TODO: Only do this if request is sucessful?
    qmid->nas_transaction_id = (qmid->nas_transaction_id + 1) % UINT8_MAX;

    //According to spec, transaction id must be non-zero
    if(!qmid->nas_transaction_id)
        qmid->nas_transaction_id = 1;

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_3){
        QMID_DEBUG_PRINT(stderr, "Will send (NAS):\n");
        parse_qmi(buf);
    }

    qmid->wds_sent_time = time(NULL);

    //+1 is to include marker
    return qmi_helpers_write(qmid->qmi_fd, buf, len + 1);
}

static ssize_t qmi_nas_send_reset(struct qmi_device *qmid){
    uint8_t buf[QMI_DEFAULT_BUF_SIZE];
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) buf;

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
        QMID_DEBUG_PRINT(stderr, "Resetting NAS\n");

    create_qmi_request(buf, QMI_SERVICE_NAS, qmid->nas_id,
            qmid->nas_transaction_id, QMI_NAS_RESET);
    qmid->nas_state = NAS_RESET;

    return qmi_nas_write(qmid, buf, qmux_hdr->length);
}

static ssize_t qmi_nas_send_indication_request(struct qmi_device *qmid){
    uint8_t buf[QMI_DEFAULT_BUF_SIZE];
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) buf;
    uint8_t enable = 1;

    create_qmi_request(buf, QMI_SERVICE_NAS, qmid->nas_id,
            qmid->nas_transaction_id, QMI_NAS_INDICATION_REGISTER);
    add_tlv(buf, QMI_NAS_TLV_IND_SYS_INFO, sizeof(uint8_t), &enable);
    //TODO: Could be that I do not need any more indications (except signal
    //strength). WDS gives me current technology
    qmid->nas_state = NAS_IND_REQ;

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
        QMID_DEBUG_PRINT(stderr, "Configuring NAS indications\n");

    return qmi_nas_write(qmid, buf, qmux_hdr->length);;
}

static ssize_t qmi_nas_req_sys_info(struct qmi_device *qmid){
    uint8_t buf[QMI_DEFAULT_BUF_SIZE];
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) buf;

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
        QMID_DEBUG_PRINT(stderr, "Requesting initial SYS_INFO\n");

    create_qmi_request(buf, QMI_SERVICE_NAS, qmid->nas_id,
            qmid->nas_transaction_id, QMI_NAS_GET_SYS_INFO);

    return qmi_nas_write(qmid, buf, qmux_hdr->length);
}

static ssize_t qmi_nas_set_sys_selection(struct qmi_device *qmid){
    uint8_t buf[QMI_DEFAULT_BUF_SIZE];
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) buf;
    //uint16_t mode_pref = 0xFFFF;
    //TODO: Add mode as a paramter, otherwise set to 0xFFFF
    uint8_t duration = 0; //Do not make change permanent

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_1)
        QMID_DEBUG_PRINT(stderr, "Setting system selection preference to %x\n",
                qmid->rat_mode_pref);

    create_qmi_request(buf, QMI_SERVICE_NAS, qmid->nas_id,
            qmid->nas_transaction_id, QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE);
    add_tlv(buf, QMI_NAS_TLV_SS_MODE, sizeof(uint16_t),
            &(htole16(qmid->rat_mode_pref)));
    add_tlv(buf, QMI_NAS_TLV_SS_DURATION, sizeof(uint8_t), &duration);

    qmid->nas_state = NAS_SET_SYSTEM;

    return qmi_nas_write(qmid, buf, qmux_hdr->length);
}

//Send message based on state in state machine
uint8_t qmi_nas_send(struct qmi_device *qmid){
    uint8_t retval = QMI_MSG_IGNORE;

    switch(qmid->nas_state){
        case NAS_GOT_CID:
        case NAS_RESET:
            qmi_nas_send_reset(qmid);
            break;
        case NAS_SET_SYSTEM:
            //TODO: Add check for if(mode != 0) here and allow for fallthrough
            qmi_nas_set_sys_selection(qmid);
            break;
        case NAS_IND_REQ:
            //Failed sends can be dealt with later
            qmi_nas_send_indication_request(qmid);
            break;
        case NAS_SYS_INFO_QUERY:
            qmi_nas_req_sys_info(qmid);
            break;
        case NAS_IDLE:
            if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
                QMID_DEBUG_PRINT(stderr, "Nothing to send for NAS\n");
            break;
    }

    return retval;
}

static uint8_t qmi_nas_handle_reset(struct qmi_device *qmid){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) qmid->buf;
    qmi_hdr_gen_t *qmi_hdr = (qmi_hdr_gen_t*) (qmux_hdr + 1);
    qmi_tlv_t *tlv = (qmi_tlv_t*) (qmi_hdr + 1);
    uint16_t result = le16toh(*((uint16_t*) (tlv+1)));

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
        QMID_DEBUG_PRINT(stderr, "Received NAS_RESET_RESP\n");

    if(result == QMI_RESULT_FAILURE){
        if(qmid_verbose_logging >= QMID_LOG_LEVEL_1)
            QMID_DEBUG_PRINT(stderr, "Could not reset NAS\n");
        return QMI_MSG_FAILURE;
    } else {
        if(qmid_verbose_logging >= QMID_LOG_LEVEL_1)
            QMID_DEBUG_PRINT(stderr, "NAS is reset\n");
        qmid->nas_state = NAS_SET_SYSTEM;
        qmi_nas_send(qmid);
        return QMI_MSG_SUCCESS;
    }
}

static uint8_t qmi_nas_handle_system_selection(struct qmi_device *qmid){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) qmid->buf;
    qmi_hdr_gen_t *qmi_hdr = (qmi_hdr_gen_t*) (qmux_hdr + 1);
    qmi_tlv_t *tlv = (qmi_tlv_t*) (qmi_hdr + 1);
    uint16_t result = le16toh(*((uint16_t*) (tlv+1)));

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
        QMID_DEBUG_PRINT(stderr, "Received SYSTEM_SELECTION_RESP\n");

    if(result == QMI_RESULT_FAILURE){
        if(qmid_verbose_logging >= QMID_LOG_LEVEL_1)
            QMID_DEBUG_PRINT(stderr, "Could not set system selection\n");
        return QMI_MSG_FAILURE;
    } else {
        if(qmid_verbose_logging >= QMID_LOG_LEVEL_1)
            QMID_DEBUG_PRINT(stderr, "Successfully set system selection preference\n");
        qmid->nas_state = NAS_IND_REQ;
        qmi_nas_send(qmid);
        return QMI_MSG_SUCCESS;
    }
}

static uint8_t qmi_nas_handle_ind_req_reply(struct qmi_device *qmid){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) qmid->buf;
    qmi_hdr_gen_t *qmi_hdr = (qmi_hdr_gen_t*) (qmux_hdr + 1);
    qmi_tlv_t *tlv = (qmi_tlv_t*) (qmi_hdr + 1);
    uint16_t result = le16toh(*((uint16_t*) (tlv+1)));

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
        QMID_DEBUG_PRINT(stderr, "Received SET_INDICATION_RESP\n");

    if(result == QMI_RESULT_FAILURE){
        if(qmid_verbose_logging >= QMID_LOG_LEVEL_1)
            QMID_DEBUG_PRINT(stderr, "Could not register indications\n");
        return QMI_MSG_FAILURE;
    } else {
        if(qmid_verbose_logging >= QMID_LOG_LEVEL_1)
            QMID_DEBUG_PRINT(stderr, "Sucessfully set NAS indications\n");

        qmid->nas_state = NAS_SYS_INFO_QUERY;
        //I don't care about the return value. If something fails, a timeout
        //will make sure the message is resent
        qmi_nas_send(qmid);
        return QMI_MSG_SUCCESS;
    }
}

//No return value needed, as no action will be taken if this message is not
//correct
static uint8_t qmi_nas_handle_sys_info(struct qmi_device *qmid){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) qmid->buf;
    qmi_hdr_gen_t *qmi_hdr = (qmi_hdr_gen_t*) (qmux_hdr + 1);
    qmi_tlv_t *tlv = (qmi_tlv_t*) (qmi_hdr + 1);
    uint16_t tlv_length = le16toh(qmi_hdr->length), i = 0;
    uint16_t result = le16toh(*((uint16_t*) (tlv+1)));
    qmi_nas_service_info_t *qsi = NULL;
    uint8_t cur_service = NO_SERVICE;

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
        QMID_DEBUG_PRINT(stderr, "Received SYS_INFO_RESP/IND\n");

    //Indications don't have failure TLV, but responses do. Remove initial TLV
    //for response
    if(qmi_hdr->control_flags & QMI_CTL_FLAGS_RESP){
        if(result == QMI_RESULT_FAILURE)
            return QMI_MSG_FAILURE;

        //Remove first tlv
        tlv_length = tlv_length - sizeof(qmi_tlv_t) - tlv->length;
        tlv = (qmi_tlv_t*) (((uint8_t*) (tlv+1)) + tlv->length);
    }

    qmid->nas_state = NAS_IDLE;

    //The goal right now is just to check if one is attached (srv_status !=
    //NO_SERVICE). If so and not connected, start connect. Then I need to figure
    //out how to only get statistics
    //TODO: Assumes mutually exclusive for now, might not always be the case
    while(i<tlv_length && !cur_service){
        switch(tlv->type){
            case QMI_NAS_TLV_SI_GSM_SS:
            case QMI_NAS_TLV_SI_WCDMA_SS:
            case QMI_NAS_TLV_SI_LTE_SS:
                if(tlv->type == QMI_NAS_TLV_SI_GSM_SS)
                    cur_service = SERVICE_GSM;
                else if(tlv->type == QMI_NAS_TLV_SI_WCDMA_SS)
                    cur_service = SERVICE_UMTS;
                else if(tlv->type == QMI_NAS_TLV_SI_LTE_SS)
                    cur_service = SERVICE_LTE;

                qsi = (qmi_nas_service_info_t*) (tlv+1);
               
                if(qsi->srv_status != QMI_NAS_TLV_SI_SRV_STATUS_SRV)
                         cur_service = NO_SERVICE;
                break;
        }

        i += sizeof(qmi_tlv_t) + tlv->length;

        if(i==tlv_length)
            break;
        else
            tlv = (qmi_tlv_t*) (((uint8_t*) (tlv+1)) + tlv->length);
    }

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_1 && cur_service
            != qmid->cur_service)
        if(cur_service)
            QMID_DEBUG_PRINT(stderr, "Modem is connected to technology %u\n",
                    cur_service);
        else
            QMID_DEBUG_PRINT(stderr, "Modem has no service\n");

    //update_connect takes care of the logic related to cur_service
    qmid->cur_service = cur_service;
    qmi_wds_update_connect(qmid);

    return QMI_MSG_SUCCESS;
}

uint8_t qmi_nas_handle_msg(struct qmi_device *qmid){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) qmid->buf;
    qmi_hdr_gen_t *qmi_hdr = (qmi_hdr_gen_t*) (qmux_hdr + 1);
    uint8_t retval = QMI_MSG_IGNORE;

    switch(qmi_hdr->message_id){
        case QMI_NAS_RESET:
            //Guards to make sure I am in the right state when receiving a
            //message to prevent reordered messages confusing the state machine.
            //Reordering sometimes occur when qmid is started right after device
            //is connected.
            //
            //Checking for state should be correct. I want to send a request
            //before I receive a reply. Since packets are processed
            //sequentially, I know that I can't recieve a reply before a request
            //is sent. However, I could be unlucky and deal with a delayed
            //response, so I should in the future add a check for transaction
            //ID.
            if(qmid->nas_state == NAS_RESET) 
                retval = qmi_nas_handle_reset(qmid);
            break;
        case QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE:
            if(qmid->nas_state == NAS_SET_SYSTEM)
                retval = qmi_nas_handle_system_selection(qmid);
            break;
        case QMI_NAS_INDICATION_REGISTER:
            if(qmid->nas_state == NAS_IND_REQ)
                retval = qmi_nas_handle_ind_req_reply(qmid);
            break;
        case QMI_NAS_GET_SYS_INFO:
        case QMI_NAS_SYS_INFO_IND:
            //The result TLV is only included in my initial SYS_INFO request. If
            //something has failed with that request, consider it critical.
            retval = qmi_nas_handle_sys_info(qmid);
            break;
        default:
            if(qmid_verbose_logging >= QMID_LOG_LEVEL_3)
                QMID_DEBUG_PRINT(stderr, "Unknown NAS packet of type %x\n",
                        qmi_hdr->message_id);
            break;
    }

    return retval;
}
