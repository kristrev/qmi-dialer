#include <stdio.h>
#include <assert.h>
#include <endian.h>
#include <string.h>
#include <time.h>

#include "qmi_wds.h"
#include "qmi_device.h"
#include "qmi_shared.h"
#include "qmi_dialer.h"
#include "qmi_hdrs.h"

//TEMP
#include "qmi_nas.h"

static inline ssize_t qmi_wds_write(struct qmi_device *qmid, uint8_t *buf,
        ssize_t len){
    //TODO: Only do this if request is sucessful?
    qmid->wds_transaction_id = (qmid->wds_transaction_id + 1) % UINT8_MAX;

    //According to spec, transaction id must be non-zero
    if(!qmid->wds_transaction_id)
        qmid->wds_transaction_id = 1;

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_3){
        QMID_DEBUG_PRINT(stderr, "Will send (WDS):\n");
        parse_qmi(buf);
    }

    qmid->wds_sent_time = time(NULL);

    //+1 is to include marker
    return qmi_helpers_write(qmid->qmi_fd, buf, len + 1);
}

static uint8_t qmi_wds_connect(struct qmi_device *qmid){
    uint8_t buf[QMI_DEFAULT_BUF_SIZE];
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) buf;
    uint8_t enable = 1;

    create_qmi_request(buf, QMI_SERVICE_WDS, qmid->wds_id,
            qmid->wds_transaction_id, QMI_WDS_START_NETWORK_INTERFACE);
    add_tlv(buf, QMI_WDS_TLV_SNI_APN_NAME, strlen(qmid->apn_name),
            qmid->apn_name);

    //Use autoconnect for this session. This TLV is deprecated, but seems to be
    //needed by for example MF821D. For later QMI versions, including this does
    //not matter as unrecognized TLVs shall be ignored (according to spec)
    add_tlv(buf, QMI_WDS_TLV_SNI_AUTO_CONNECT, sizeof(uint8_t), &enable);

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_1)
        QMID_DEBUG_PRINT(stderr, "Will connect to APN %s\n", qmid->apn_name);

    //This is so far the only critical write I have. However, I will not do
    //anything right now, the next connect will be controlled by a timeout
    if(qmi_wds_write(qmid, buf, qmux_hdr->length) == qmux_hdr->length + 1){
        qmid->wds_state = WDS_CONNECTING;
        return QMI_MSG_SUCCESS;
    } else
        return QMI_MSG_FAILURE;
}

uint8_t qmi_wds_disconnect(struct qmi_device *qmid){
    uint8_t buf[QMI_DEFAULT_BUF_SIZE];
    uint32_t pkt_data_handle = htole32(qmid->pkt_data_handle);
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) buf;
    uint8_t enable = 1;

    create_qmi_request(buf, QMI_SERVICE_WDS, qmid->wds_id,
            qmid->wds_transaction_id, QMI_WDS_STOP_NETWORK_INTERFACE);
    add_tlv(buf, QMI_WDS_TLV_SNI_PACKET_HANDLE, sizeof(uint32_t),
            &pkt_data_handle);
    add_tlv(buf, QMI_WDS_TLV_SNI_STOP_AUTO_CONNECT, sizeof(uint8_t),
            &enable);

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_1)
        QMID_DEBUG_PRINT(stderr, "Will disconnect\n");

    //TODO: There can't be any partial writes, the file descriptor is in
    //blocking mode
    if(qmi_wds_write(qmid, buf, qmux_hdr->length)){
        //TODO: Should perhaps be disconnecting, look into it
        qmid->wds_state = WDS_DISCONNECTED;
        return QMI_MSG_SUCCESS;
    } else
        return QMI_MSG_FAILURE;
}

//TODO: Fix return values here
uint8_t qmi_wds_update_connect(struct qmi_device *qmid){
    //Establish a new connection if I have service and a connection is not
    //already established, or in progress
    uint8_t retval = 0;

    //After the first successful connection attempt, AUTOCONNECT will take care
    //of re-establishing any dropped connections. So only call connect if state
    //is DISCONNECTED (WDS is moved in IDLE when the first connection is
    //sucessful)
    if(qmid->cur_service && qmid->wds_state == WDS_DISCONNECTED)
        qmi_wds_connect(qmid);
    
    return 0;
}

static ssize_t qmi_wds_send_reset(struct qmi_device *qmid){
    uint8_t buf[QMI_DEFAULT_BUF_SIZE];
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) buf;

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
        QMID_DEBUG_PRINT(stderr, "Resetting WDS\n");

    create_qmi_request(buf, QMI_SERVICE_WDS, qmid->wds_id,
            qmid->wds_transaction_id, QMI_WDS_RESET);
    qmid->wds_state = WDS_RESET;

    return qmi_wds_write(qmid, buf, qmux_hdr->length);
}

static ssize_t qmi_wds_send_set_event_report(struct qmi_device *qmid){
    uint8_t buf[QMI_DEFAULT_BUF_SIZE];
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) buf;
    uint8_t enable = 1;

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
        QMID_DEBUG_PRINT(stderr, "Configuring event reports\n");

    create_qmi_request(buf, QMI_SERVICE_WDS, qmid->wds_id,
            qmid->wds_transaction_id, QMI_WDS_SET_EVENT_REPORT);
    add_tlv(buf, QMI_WDS_TLV_ER_CUR_DATA_BEARER_IND, sizeof(uint8_t), &enable);
    qmid->wds_state = WDS_IND_REQ;

    return qmi_wds_write(qmid, buf, qmux_hdr->length);
}

//Keep this function for now, even though it is not used. With the MF821D, I see
//that the modem remains online (according to the LED) even after I send
//disconnect (when exiting application). However, the connection is for all
//intensts and purposes dead, it is for example not possible to send data using
//the modem or get a DHCP reply. It seems like there is a bug in firmware,
//because when I restart application, pkt_srvc and sys_info indicates that I
//have service. However, a connection attempt causes me to loose service and
//start from the beginning. Similar behavior is not observed with for example
//the Alcatel OneTouch
static ssize_t qmi_wds_send_get_pkt_srvc(struct qmi_device *qmid){ 
    uint8_t buf[QMI_DEFAULT_BUF_SIZE]; 
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) buf;
    
    if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
        QMID_DEBUG_PRINT(stderr, "Requesting current packet serivce status\n");

    create_qmi_request(buf, QMI_SERVICE_WDS, qmid->wds_id,
            qmid->wds_transaction_id, QMI_WDS_GET_PKT_SRVC_STATUS);

    return qmi_wds_write(qmid, buf, qmux_hdr->length);
}

ssize_t qmi_wds_send_update_autoconnect(struct qmi_device *qmid,
        uint8_t enabled){
    uint8_t buf[QMI_DEFAULT_BUF_SIZE];
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) buf;

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
        if(enabled)
            QMID_DEBUG_PRINT(stderr, "Enabling autoconnect\n");
        else
            QMID_DEBUG_PRINT(stderr, "Disabling autoconnect\n");

    create_qmi_request(buf, QMI_SERVICE_WDS, qmid->wds_id,
            qmid->wds_transaction_id, QMI_WDS_SET_AUTOCONNECT_SETTINGS);
    add_tlv(buf, QMI_WDS_TLV_SAS_SETTING, sizeof(uint8_t), &enabled);

    return qmi_wds_write(qmid, buf, qmux_hdr->length);
}

static ssize_t qmi_wds_request_data_bearer(struct qmi_device *qmid){
    uint8_t buf[QMI_DEFAULT_BUF_SIZE];
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) buf;

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
        QMID_DEBUG_PRINT(stderr, "Requesting current data bearer\n");

    create_qmi_request(buf, QMI_SERVICE_WDS, qmid->wds_id,
            qmid->wds_transaction_id, QMI_WDS_GET_DATA_BEARER_TECHNOLOGY);

    return qmi_wds_write(qmid, buf, qmux_hdr->length);
}


uint8_t qmi_wds_send(struct qmi_device *qmid){
    uint8_t retval = QMI_MSG_IGNORE;

    switch(qmid->wds_state){
        case WDS_GOT_CID:
        case WDS_RESET:
            qmi_wds_send_reset(qmid);
            break;
        case WDS_IND_REQ:
            //Failed sends are not that interesting. It will just take longer
            //before the indications will be set up (timeout)
            qmi_wds_send_update_autoconnect(qmid, 0);
            qmi_wds_send_set_event_report(qmid);
            break;
        case WDS_DISCONNECTED:
            //wds_send also needs to support disconnect, but this function
            //should only be called from within wds state machine (and only when
            //disconnected is actually set)
            if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
                QMID_DEBUG_PRINT(stderr, "Done configuring WDS, will attempt connection\n");

            qmi_wds_update_connect(qmid);
            break;
    }

    return retval;
}

static uint8_t qmi_wds_handle_reset(struct qmi_device *qmid){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) qmid->buf;
    qmi_hdr_gen_t *qmi_hdr = (qmi_hdr_gen_t*) (qmux_hdr + 1);
    qmi_tlv_t *tlv = (qmi_tlv_t*) (qmi_hdr + 1);
    uint16_t result = le16toh(*((uint16_t*) (tlv+1)));

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
        QMID_DEBUG_PRINT(stderr, "Received WDS_RESET_RESP\n");

    if(result == QMI_RESULT_FAILURE){
        if(qmid_verbose_logging >= QMID_LOG_LEVEL_1)
            QMID_DEBUG_PRINT(stderr, "Could not reset WDS\n");
        return QMI_MSG_FAILURE;
    } else {
        if(qmid_verbose_logging >= QMID_LOG_LEVEL_1)
            QMID_DEBUG_PRINT(stderr, "WDS is reset\n");
        qmid->wds_state = WDS_IND_REQ;
        qmi_wds_send(qmid);
        return QMI_MSG_SUCCESS;
    }
}

static uint8_t qmi_wds_handle_event_report(struct qmi_device *qmid){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) qmid->buf;
    qmi_hdr_gen_t *qmi_hdr = (qmi_hdr_gen_t*) (qmux_hdr + 1);
    qmi_tlv_t *tlv = (qmi_tlv_t*) (qmi_hdr + 1), *tlv_db = NULL;
    qmi_wds_cur_db_t *cur_db = NULL;

    uint16_t tlv_length = le16toh(qmi_hdr->length), i = 0;
    uint16_t result = le16toh(*((uint16_t*) (tlv+1)));
    uint8_t retval = QMI_MSG_IGNORE;

    if(result == QMI_RESULT_FAILURE)
        return QMI_MSG_FAILURE;

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
        QMID_DEBUG_PRINT(stderr, "Received an EVENT_REPORT_RESP/IND\n");

    //WDS is configured and ready to connect
    //EventReport is both used as the indication AND the reply for the intial
    //request. I need to make sure I dont mess up the state, so only set it for
    //the request (WDS can only move into >= PKT_SRVC_QUERY from here)
    //if(qmid->wds_state < WDS_DISCONNECTED){
    if(qmi_hdr->control_flags & QMI_CTL_FLAGS_RESP){
        qmid->wds_state = WDS_DISCONNECTED;
        retval = QMI_MSG_SUCCESS;
        qmi_wds_send(qmid);
    }
    
    //Remove first tlv if this message was the result of a request
    if(qmi_hdr->transaction_id){
        tlv_length = tlv_length - sizeof(qmi_tlv_t) - tlv->length;
        tlv = (qmi_tlv_t*) (((uint8_t*) (tlv+1)) + tlv->length);
    }

    while(i<tlv_length){
        if(tlv->type == QMI_WDS_TLV_ER_CUR_DATA_BEARER){
            //I have found what I have been looking for, break
            tlv_db = tlv;
            break;
        }

        i += sizeof(qmi_tlv_t) + tlv->length;

        if(i==tlv_length)
            break;
        else
            tlv = (qmi_tlv_t*) (((uint8_t*) (tlv+1)) + tlv->length);
    }

    //The reason I cant check for for example i == tlv_length here, is that
    //CUR_DATA_BEARER might be the only TLV
    if(tlv_db == NULL)
        return retval;

    cur_db = (qmi_wds_cur_db_t*) (tlv+1);

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_1){
        if(cur_db->rat_mask & QMI_WDS_ER_RAT_WCDMA)
            QMID_DEBUG_PRINT(stderr, "Data bearer is changed to WCDMA\n");
        if(cur_db->rat_mask & QMI_WDS_ER_RAT_GPRS)
            QMID_DEBUG_PRINT(stderr, "Data bearer is changed to GPRS\n");
        if(cur_db->rat_mask & QMI_WDS_ER_RAT_HSDPA)
            QMID_DEBUG_PRINT(stderr, "Data bearer is changed to HSDPA\n");
        if(cur_db->rat_mask & QMI_WDS_ER_RAT_HSUPA)
            QMID_DEBUG_PRINT(stderr, "Data bearer is changed to HSUPA\n");
        if(cur_db->rat_mask & QMI_WDS_ER_RAT_EDGE)
            QMID_DEBUG_PRINT(stderr, "Data bearer is changed to EDGE\n");
        if(cur_db->rat_mask & QMI_WDS_ER_RAT_LTE)
            QMID_DEBUG_PRINT(stderr, "Data bearer is changed to LTE\n");
        if(cur_db->rat_mask & QMI_WDS_ER_RAT_HSDPA_PLUS)
            QMID_DEBUG_PRINT(stderr, "Data bearer is changed to HSDPA+\n");
        if(cur_db->rat_mask & QMI_WDS_ER_RAT_DC_HSDPA_PLUS)
            QMID_DEBUG_PRINT(stderr, "Data bearer is changed to DC_HSDPA+\n");
    }

    return retval;
}

static uint8_t qmi_wds_handle_connect(struct qmi_device *qmid){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) qmid->buf;
    qmi_hdr_gen_t *qmi_hdr = (qmi_hdr_gen_t*) (qmux_hdr + 1);
    qmi_tlv_t *tlv = (qmi_tlv_t*) (qmi_hdr + 1);
    uint16_t tlv_length = le16toh(qmi_hdr->length), i = 0;
    uint16_t result = le16toh(*((uint16_t*) (tlv+1)));
    uint8_t retval = QMI_MSG_IGNORE;

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
        QMID_DEBUG_PRINT(stderr, "Received a START_NETWORK_INTERFACE_RESP\n");

    if(result == QMI_RESULT_FAILURE){
        //TODO: Consider adding the actual error code too
        if(qmid_verbose_logging >= QMID_LOG_LEVEL_1)
            QMID_DEBUG_PRINT(stderr, "Connection attempt failed\n");

        //Disable autoconnect. This seems to be a required step for getting the
        //MF821D to work properly with the second connection attempt (otherwise
        //it just returns NO_EFFECT). For other modems this should not matter, I
        //want to be in control of the first connection attempt. Auto connect is
        //only for moving between HSDPA and LTE, for example
        //The WDS connected check is in case autoconnect has kicked in and
        //worked on some modem (testing the pkt_srvc theory)
        if(qmid->wds_state != WDS_CONNECTED){
            qmi_wds_send_update_autoconnect(qmid, 0);
            qmid->wds_state = WDS_DISCONNECTED;

            //TODO: Hack to make application keep trying to connect even if
            //technology does not change
            qmid->cur_service = NO_SERVICE;

            //Next connection attempt will be decided by timeout or technology
            //change
        } else if(qmid_verbose_logging >= QMID_LOG_LEVEL_1)
            QMID_DEBUG_PRINT(stderr, "Connection attempt failed, but "
                    "autoconnected\n");
        return retval;
    }

    tlv = (qmi_tlv_t*) (((uint8_t*) (tlv+1)) + tlv->length);
    qmid->pkt_data_handle = le32toh(*((uint32_t*) (tlv + 1)));
    qmid->wds_state = WDS_CONNECTED;

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_1)
        QMID_DEBUG_PRINT(stderr, "Modem is connected. Handle %x\n",
                qmid->pkt_data_handle);

    //Send autoconnect in case modem does not support 
    qmi_wds_send_update_autoconnect(qmid, 1);

    qmid->wds_state = WDS_IDLE;

    return retval;
}

static uint8_t qmi_wds_handle_get_db_tech(struct qmi_device *qmid){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) qmid->buf;
    qmi_hdr_gen_t *qmi_hdr = (qmi_hdr_gen_t*) (qmux_hdr + 1);
    qmi_tlv_t *tlv = (qmi_tlv_t*) (qmi_hdr + 1);
    uint16_t tlv_length = le16toh(qmi_hdr->length), i = 0;
    uint16_t result = le16toh(*((uint16_t*) (tlv+1)));
    uint8_t retval = QMI_MSG_IGNORE;
    uint8_t data_bearer = 0;

    if(result == QMI_RESULT_FAILURE){
        //TODO: Consider adding the actual error code too
        if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
            QMID_DEBUG_PRINT(stderr, "Failed to get current data bearer\n");

        return retval;
    }

    tlv = (qmi_tlv_t*) (((uint8_t*) (tlv+1)) + tlv->length);
    data_bearer = *((uint8_t*) (tlv+1));

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_1)
        if(data_bearer == QMI_WDS_DB_GSM)
            QMID_DEBUG_PRINT(stderr, "Current data bearer is GSM\n");
        else if(data_bearer == QMI_WDS_DB_UMTS)
            QMID_DEBUG_PRINT(stderr, "Current data bearer is UMTS\n");
        else if(data_bearer == QMI_WDS_DB_EDGE)
            QMID_DEBUG_PRINT(stderr, "Current data bearer is EDGE\n");
        else if(data_bearer == QMI_WDS_DB_HSDPA_WCDMA)
            QMID_DEBUG_PRINT(stderr, "Current data bearer is HSDPA/WCDMA\n");
        else if(data_bearer == QMI_WDS_DB_WCDMA_HSUPA)
            QMID_DEBUG_PRINT(stderr, "Current data bearer is WCDMA/HSUPA\n");
        else if(data_bearer == QMI_WDS_DB_HSDPA_HSUPA)
            QMID_DEBUG_PRINT(stderr, "Current data bearer is HSDPA/HSUPA\n");
        else if(data_bearer == QMI_WDS_DB_LTE)
            QMID_DEBUG_PRINT(stderr, "Current data bearer is LTE\n");
        else if(data_bearer == QMI_WDS_DB_HSDPA_PLUS_WCDMA)
            QMID_DEBUG_PRINT(stderr, "Current data bearer is HSDPA+/WCDMA\n");
        else if(data_bearer == QMI_WDS_DB_HSDPA_PLUS_HSUPA)
            QMID_DEBUG_PRINT(stderr, "Current data bearer is HSDPA+/HSUPA\n");
        else if(data_bearer == QMI_WDS_DB_DC_HSDPA_WCDMA)
            QMID_DEBUG_PRINT(stderr, "Current data bearer is DC-HSDPA+/WCDMA\n");
        else if(data_bearer == QMI_WDS_DB_DC_HSDPA_HSUPA)
            QMID_DEBUG_PRINT(stderr, "Current data bearer is DC-HSDPA+/HSUPA\n");

    return retval;
}

static uint8_t qmi_wds_handle_pkt_srvc(struct qmi_device *qmid){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) qmid->buf;
    qmi_hdr_gen_t *qmi_hdr = (qmi_hdr_gen_t*) (qmux_hdr + 1);
    qmi_tlv_t *tlv = (qmi_tlv_t*) (qmi_hdr + 1);
    uint8_t retval = QMI_MSG_IGNORE;

    //I am only interested in the first TLV and never request this one
    uint8_t conn_status = *((uint8_t*) (tlv+1));
    uint8_t reconn_required = *(((uint8_t*) (tlv+1))+1);

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_1)
        QMID_DEBUG_PRINT(stderr, "pkt srvc status: %x reconn: %x\n",
                conn_status, reconn_required);

    //Some modems, at least the MF821D, seems to behave a bit strange when
    //service is lost. It seems as if loss of service is announced through 
    //a SYS_INFO, autoconnect works fine. If it is announced through a packet
    //service, autconnect does not work. What these checks do is to update the
    //state machine also based on the packet service messages. Treat PSS_CONNECT
    //as CONNECTED and other statuses as disconnected.
    //
    //This should work and will not race because a packet service message is
    //always preceeded by a sys info message (at least it seems so). On modems
    //that do not display the autoconnect-behavior, the connect will fail with
    //the "NoEffect" and connection be established automatically. On modems with
    //this cause, connect will work as intended.
    if(conn_status == QMI_WDS_PSS_CONNECTED){
        qmid->wds_state = WDS_CONNECTED;
        //Request current data bearer (in case I have missed the initial
        //indication)
        qmi_wds_request_data_bearer(qmid);
    } else{
        qmid->wds_state = WDS_DISCONNECTED;
        qmid->cur_service = 0;
    }

    return retval;
}

uint8_t qmi_wds_handle_msg(struct qmi_device *qmid){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) qmid->buf;
    qmi_hdr_gen_t *qmi_hdr = (qmi_hdr_gen_t*) (qmux_hdr + 1);
    uint8_t retval = QMI_MSG_IGNORE;

    if(qmi_hdr->message_id == 0x22)
        parse_qmi(qmid->buf);

    switch(qmi_hdr->message_id){
        case QMI_WDS_RESET:
            if(qmid->wds_state == WDS_RESET)
                retval = qmi_wds_handle_reset(qmid);
            break;
        //This one also covers the reply to the set event report
        case QMI_WDS_EVENT_REPORT_IND:
            //Adding a guard against reordering here is tricky, since this value
            //is used both by response and indication. Think some more
            retval = qmi_wds_handle_event_report(qmid);
            //Setting up the event report is the only configuration step for
            //WDS, so check if I can connect. The reason for checking sucess and
            //not just >0 is the indications that also match
            //QMI_WDS_EVENT_REPORT_IND.
            break;
        case QMI_WDS_START_NETWORK_INTERFACE:
            if(qmid->wds_state >= WDS_CONNECTING)
                retval = qmi_wds_handle_connect(qmid);
            break;
        case QMI_WDS_GET_DATA_BEARER_TECHNOLOGY:
            if(qmid->wds_state == WDS_IDLE)
                retval = qmi_wds_handle_get_db_tech(qmid);
            break;
        case QMI_WDS_GET_PKT_SRVC_STATUS:
            retval = qmi_wds_handle_pkt_srvc(qmid);
            break;
        default:
            if(qmid_verbose_logging >= QMID_LOG_LEVEL_3)
                QMID_DEBUG_PRINT(stderr, "Unknown WDS packet of type %x\n",
                        qmi_hdr->message_id);
            break;
    }
    
    return retval;
}
