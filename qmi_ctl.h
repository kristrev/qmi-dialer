#ifndef QMI_CTL_H
#define QMI_CTL_H

#include <stdint.h>
#include <sys/types.h>

//Method for either releasing or updating a CID
//TODO: Look into transaction ID paramter? Is it needed?
ssize_t ctl_update_cid(int32_t qmi_fd, uint8_t service,
        uint8_t transaction_id, uint8_t release);

#endif
