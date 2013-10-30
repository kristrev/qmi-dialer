#ifndef QMI_WDS_H
#define QMI_WDS_H

#include <stdint.h>

//Message types
#define QMI_WDS_SET_EVENT_REPORT            0x0001
#define QMI_WDS_EVENT_REPORT_IND            QMI_WDS_SET_EVENT_REPORT
#define QMI_WDS_START_NETWORK_INTERFACE     0x0020
#define QMI_WDS_STOP_NETWORK_INTERFACE      0x0021
#define QMI_WDS_GET_PKT_SRVC_STATUS         0x0022
#define QMI_WDS_SET_AUTOCONNECT_SETTINGS    0x0051

//Event report TLVs
#define QMI_WDS_TLV_ER_CUR_DATA_BEARER_IND  0x15

//START_NETWORK_INTERFACE TLVs
#define QMI_WDS_TLV_SNI_APN_NAME            0x14
#define QMI_WDS_TLV_SNI_AUTO_CONNECT        0x33

//STOP_NETWORK_INTERFACE TLV
#define QMI_WDS_TLV_SNI_PACKET_HANDLE       0x01
#define QMI_WDS_TLV_SNI_STOP_AUTO_CONNECT   0x10

//SET_AUTOCONNECT_SETTINGS TLVs
#define QMI_WDS_TLV_SAS_SETTING             0x01

struct qmi_device;

//Handle a WDS message. Returns false if something went wrong
uint8_t qmi_wds_handle_msg(struct qmi_device *qmid);

//Enable/disable autoconnect
uint8_t qmi_wds_send_update_autoconnect(struct qmi_device *qmid,
        uint8_t enabled);

//Send message based on state in state machine
uint8_t qmi_wds_send(struct qmi_device *qmid);

//Update a connection based on a change in service or WDS connection
uint8_t qmi_wds_update_connect(struct qmi_device *qmid);
#endif
