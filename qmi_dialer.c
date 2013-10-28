#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "qmi_device.h"
#include "qmi_ctl.h"

//Global variable controlling log level (binary for now)
uint8_t qmi_verbose_logging;

//Signal handler for closing down connection and releasing cid
//Seems like I have to wait for a reply?

int main(int argc, char *argv[]){
    //Should also be global, so I can access it in signal handler
    struct qmi_device qmid;
    int32_t efd;

    //Use RAII
    memset(&qmid, 0, sizeof(qmid));

    //This is not nice, add proper processing of arguments later
    if((qmid.qmi_fd = open(argv[1], O_RDWR)) == -1){
        perror("Error opening QMI device");
        return EXIT_FAILURE;
    }

    //Send request for CID(s). The rest will then be controlled by messages from
    //the modem.
    qmi_ctl_update_cid(&qmid, QMI_SERVICE_NAS, 0, 0);

    //TODO: Assumes no packet loss. Is that safe?
    while(1){
         
    }

    return EXIT_SUCCESS;
}
