#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "qmi_device.h"

//Global variable controlling log level (binary for now)
uint8_t qmi_verbose_logging;

int main(int argc, char *argv[]){
    struct qmi_device qmid;

    //Use RAII
    memset(&qmid, 0, sizeof(qmid));

    //printf("Ready to start 

    //Return 0 on success, -1 on failure
    return 0;
}
