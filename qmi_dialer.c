#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "qmi_dialer.h"
#include "qmi_device.h"
#include "qmi_ctl.h"
#include "qmi_hdrs.h"

//Signal handler for closing down connection and releasing cid
//Seems like I have to wait for a reply?

static void read_data(struct qmi_device *qmid){
    ssize_t numbytes;
    qmux_hdr_t *qmux_hdr;

    //First, read a qmux header then read the data
    //TODO: Consider something more efficient than two reads. A circular buffer
    //for example.
    if(!qmid->cur_qmux_length){
        numbytes = read(qmid->qmi_fd, qmid->buf + qmid->qmux_progress,
                sizeof(qmux_hdr_t));

        if(numbytes != sizeof(qmux_hdr_t)){
            if(qmi_verbose_logging)
                fprintf(stderr, "Parial QMUX, length %zd\n", numbytes);

            qmid->qmux_progress += numbytes;
        } else {
            qmux_hdr = (qmux_hdr_t*) qmid->buf;
            qmid->qmux_progress = sizeof(qmux_hdr_t);
            //+1 is for the marker, which is also part of the data I want to
            //read
            qmid->cur_qmux_length = qmux_hdr->length + 1;
        }
    } 

    //If I have received a full header, try to read data. This might happen
    //immediatly 
    if(qmid->cur_qmux_length){
        numbytes = read(qmid->qmi_fd, qmid->buf + qmid->qmux_progress,
                qmid->cur_qmux_length - qmid->qmux_progress);
        qmid->qmux_progress += numbytes;

        if(qmid->qmux_progress == qmid->cur_qmux_length){
            printf("Finished qmux\n");

            if(qmi_verbose_logging)
                parse_qmi(qmid->buf);

            qmid->qmux_progress = 0;
            qmid->cur_qmux_length = 0;
        }
    }
}

int main(int argc, char *argv[]){
    //Should also be global, so I can access it in signal handler
    struct qmi_device qmid;
    int32_t efd;

    //TODO: Set using command line option
    qmi_verbose_logging = 1;

    //Use RAII
    memset(&qmid, 0, sizeof(qmid));

    //This is not nice, add proper processing of arguments later
    if((qmid.qmi_fd = open(argv[1], O_RDWR)) == -1){
        perror("Error opening QMI device");
        return EXIT_FAILURE;
    }

    //Send request for CID(s). The rest will then be controlled by messages from
    //the modem.
    //TODO: Get rid of magic numbers
    qmi_ctl_update_cid(&qmid, QMI_SERVICE_NAS, false, 0);

    //TODO: Assumes no packet loss. Is that safe?
    while(1){
        read_data(&qmid); 
    }

    return EXIT_SUCCESS;
}
