#include <stdio.h>
#include <assert.h>

#include "qmi_wds.h"
#include "qmi_device.h"
#include "qmi_shared.h"

uint8_t qmi_wds_send(struct qmi_device *qmid){
    uint8_t retval = QMI_MSG_IGNORE;

    switch(qmid->wds_state){
        case WDS_GOT_CID:
        case WDS_IND_REQ:
            break;
        default:
            fprintf(stderr, "Unknown wds state (send())\n");
            assert(0);
    }

    return retval;
}
