#ifndef QMI_CTL_H
#define QMI_CTL_H

#include <stdint.h>
#include <sys/types.h>

//CTL message types
#define QMI_CTL_GET_CID         0x0022
#define QMI_CTL_RELEASE_CID     0x0023

//CTL TLVs
#define QMI_CTL_TLV_ALLOC_INFO  0x01

struct qmi_device;

//Method for either releasing or updating a CID
//TODO: Look into transaction ID paramter? Is it needed?
ssize_t qmi_ctl_update_cid(struct qmi_device *qmid, uint8_t service,
        uint8_t release, uint8_t cid);

#endif
