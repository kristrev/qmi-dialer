#ifndef QMI_NAS_H
#define QMI_NAS_H

#include <stdint.h>
#include <sys/types.h>

//NAS message types 
#define QMI_NAS_RESET                           0x0000
#define QMI_NAS_INDICATION_REGISTER             0x0003
#define QMI_NAS_GET_SERVING_SYSTEM              0x0024
#define QMI_NAS_GET_RF_BAND_INFO                0x0031
#define QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE 0x0033
#define QMI_NAS_GET_SYS_INFO                    0x004D
#define QMI_NAS_SYS_INFO_IND                    0x004E
#define QMI_NAS_GET_SIG_INFO                    0x004F
#define QMI_NAS_RF_BAND_INFO_IND                0x0066

//TLVs
#define QMI_NAS_TLV_IND_SYS_INFO                0x18
#define QMI_NAS_TLV_IND_SIGNAL_STRENGTH         0x19
#define QMI_NAS_TLV_IND_RF_BAND                 0x20

//Sys info TLV
#define QMI_NAS_TLV_SI_GSM_SS                   0x12
#define QMI_NAS_TLV_SI_WCDMA_SS                 0x13
#define QMI_NAS_TLV_SI_LTE_SS                   0x14

//Service status info variables
#define QMI_NAS_TLV_SI_SRV_STATUS_SRV           0x02

//System selection TLV
#define QMI_NAS_TLV_SS_MODE                     0x11
#define QMI_NAS_TLV_SS_DURATION                 0x17
#define QMI_NAS_TLV_SS_ORDER                    0x1E

//System selection mode preference values
#define QMI_NAS_RAT_MODE_PREF_GSM               0x4
#define QMI_NAS_RAT_MODE_PREF_UMTS              0x8
#define QMI_NAS_RAT_MODE_PREF_LTE               0x10
#define QMI_NAS_RAT_MODE_PREF_MIN               (QMI_NAS_RAT_MODE_PREF_GSM | QMI_NAS_RAT_MODE_PREF_UMTS)
#define QMI_NAS_RAT_MODE_PREF_ALL               (QMI_NAS_RAT_MODE_PREF_MIN | QMI_NAS_RAT_MODE_PREF_LTE)

//SIGINFO TLVs
#define QMI_NAS_TLV_SIG_INFO_WCDMA              0x13
#define QMI_NAS_TLV_SIG_INFO_LTE                0x14

//Android constants for number of signal strength bars
#define SIGNAL_STRENGTH_NONE_OR_UNKNOWN         0
#define SIGNAL_STRENGTH_POOR                    1
#define SIGNAL_STRENGTH_MODERATE                2
#define SIGNAL_STRENGTH_GOOD                    3
#define SIGNAL_STRENGTH_GREAT                   4

//Why is there so many definitions of the same variable???
#define QMI_NAS_RADIO_IF_GSM                    0x04
#define QMI_NAS_RADIO_IF_UMTS                   0x05
#define QMI_NAS_RADIO_IF_LTE                    0x08

//GSM/WCDMA/LTE/... uses the same structure for service info
//TODO: CDMA is exception, but not able to test, so postpone implementation
struct qmi_nas_service_info{
    uint8_t srv_status;
    uint8_t true_srv_status;
    uint8_t is_pre_data_path;
} __attribute__((packed));

struct qmi_nas_wcdma_signal_info{
    int8_t rssi;
    int16_t ecio;
} __attribute__((packed));

struct qmi_nas_lte_signal_info{
    int8_t rssi;
    int8_t rsrq;
    int16_t rsrp;
    int16_t snr;
} __attribute__((packed));

struct qmi_nas_rf_band_info{
    uint8_t radio_if;
    uint16_t active_band;
    uint16_t active_channel;
} __attribute__((packed));

typedef struct qmi_nas_service_info qmi_nas_service_info_t;
typedef struct qmi_nas_wcdma_signal_info qmi_nas_wcdma_signal_info_t;
typedef struct qmi_nas_lte_signal_info qmi_nas_lte_signal_info_t;
typedef struct qmi_nas_rf_band_info qmi_nas_rf_band_info_t;
typedef struct qmi_nas_si_order qmi_nas_si_acq_order_t;

struct qmi_device;

//Handle a ctl message. Returns false if something went wrong
uint8_t qmi_nas_handle_msg(struct qmi_device *qmid);

//Send message based on state in state machine
uint8_t qmi_nas_send(struct qmi_device *qmid);

//Update the current system selection
ssize_t qmi_nas_set_sys_selection(struct qmi_device *qmid);
#endif
