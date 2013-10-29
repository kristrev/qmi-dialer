#ifndef QMI_CTL_H
#define QMI_CTL_H

#include <stdint.h>
#include <sys/types.h>
//Assume C99
#include <stdbool.h>

//CTL message types
#define QMI_CTL_GET_CID         0x0022
#define QMI_CTL_RELEASE_CID     0x0023
#define QMI_CTL_SYNC            0x0027

//CTL TLVs
#define QMI_CTL_TLV_ALLOC_INFO  0x01

struct qmi_device;

//Method for either releasing or updating a CID. Propagtes return from write()
//TODO: Look into transaction ID paramter? Is it needed?
ssize_t qmi_ctl_update_cid(struct qmi_device *qmid, uint8_t service,
        bool release, uint8_t cid);

//Handle a ctl message. Returns false if something went wrong
uint8_t qmi_ctl_handle_msg(struct qmi_device *qmid);

//Send a sync message to release all CIDs. Propagates return from write()
ssize_t qmi_ctl_send_sync(struct qmi_device *qmid);
#endif
