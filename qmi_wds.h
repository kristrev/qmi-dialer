#ifndef QMI_WDS_H
#define QMI_WDS_H

#include <stdint.h>

//Message types
#define QMI_WDS_RESET                       0x0000
#define QMI_WDS_SET_EVENT_REPORT            0x0001
#define QMI_WDS_EVENT_REPORT_IND            QMI_WDS_SET_EVENT_REPORT
#define QMI_WDS_START_NETWORK_INTERFACE     0x0020
#define QMI_WDS_STOP_NETWORK_INTERFACE      0x0021
#define QMI_WDS_GET_PKT_SRVC_STATUS         0x0022
#define QMI_WDS_GET_DATA_BEARER_TECHNOLOGY  0x0037
#define QMI_WDS_SET_AUTOCONNECT_SETTINGS    0x0051

//Event report TLVs
//This one has a confusing name. It is used to set the indication
#define QMI_WDS_TLV_ER_CUR_DATA_BEARER_IND  0x15
#define QMI_WDS_TLV_ER_CUR_DATA_BEARER      0x1D

//START_NETWORK_INTERFACE TLVs
#define QMI_WDS_TLV_SNI_APN_NAME            0x14
#define QMI_WDS_TLV_SNI_AUTO_CONNECT        0x33

//STOP_NETWORK_INTERFACE TLV
#define QMI_WDS_TLV_SNI_PACKET_HANDLE       0x01
#define QMI_WDS_TLV_SNI_STOP_AUTO_CONNECT   0x10

//SET_AUTOCONNECT_SETTINGS TLVs
#define QMI_WDS_TLV_SAS_SETTING             0x01

//UMTS RAT flags
#define QMI_WDS_ER_RAT_WCDMA                0x01
#define QMI_WDS_ER_RAT_GPRS                 0x02
#define QMI_WDS_ER_RAT_HSDPA                0x04
#define QMI_WDS_ER_RAT_HSUPA                0x08
#define QMI_WDS_ER_RAT_EDGE                 0x10
#define QMI_WDS_ER_RAT_LTE                  0x20
#define QMI_WDS_ER_RAT_HSDPA_PLUS           0x40
#define QMI_WDS_ER_RAT_DC_HSDPA_PLUS        0x80
#define QMI_WDS_ER_RAT_64_QAM               0x100
#define QMI_WDS_ER_RAT_TDSCDMA              0x200

//Data bearer technology values (which for some reason is different than flags)
//Only picked the technologies available in Norway
#define QMI_WDS_DB_GSM                      0x03
#define QMI_WDS_DB_UMTS                     0x04
#define QMI_WDS_DB_EDGE                     0x06
#define QMI_WDS_DB_HSDPA_WCDMA              0x07
#define QMI_WDS_DB_WCDMA_HSUPA              0x08
#define QMI_WDS_DB_HSDPA_HSUPA              0x09
#define QMI_WDS_DB_LTE                      0x0A
#define QMI_WDS_DB_HSDPA_PLUS_WCDMA         0x0C
#define QMI_WDS_DB_HSDPA_PLUS_HSUPA         0x0D
#define QMI_WDS_DB_DC_HSDPA_WCDMA           0x0E
#define QMI_WDS_DB_DC_HSDPA_HSUPA           0x0F

struct qmi_wds_cur_db{
    uint8_t current_nw;
    uint32_t rat_mask;
    uint32_t so_mask;
} __attribute__((packed));

typedef struct qmi_wds_cur_db qmi_wds_cur_db_t;

struct qmi_device;

//Handle a WDS message. Returns false if something went wrong
uint8_t qmi_wds_handle_msg(struct qmi_device *qmid);

//Enable/disable autoconnect
ssize_t qmi_wds_send_update_autoconnect(struct qmi_device *qmid,
        uint8_t enabled);

//Send message based on state in state machine
uint8_t qmi_wds_send(struct qmi_device *qmid);

//Update a connection based on a change in service or WDS connection
uint8_t qmi_wds_update_connect(struct qmi_device *qmid);

//Disconnect is only called when I exit application
uint8_t qmi_wds_disconnect(struct qmi_device *qmid);
#endif
