#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>

#include "qmi_dialer.h"
#include "qmi_device.h"
#include "qmi_ctl.h"
#include "qmi_hdrs.h"
#include "qmi_wds.h"
#include "qmi_dms.h"
#include "qmi_nas.h"

//Define this variable globally (within scope of this file), so that I can
//access it from the signal handler
static struct qmi_device qmid;

static void qmi_cleanup(){
    uint8_t enable = 0;
    //Disable autoconnect (in case other applications will use modem)
    qmi_wds_send_update_autoconnect(&qmid, enable);
    
    //Disconnect connection (if any)
    //Beware that some modems, for example MF821D, seems to return NoEffect here
    if(qmid.pkt_data_handle){
        qmid.cur_service = NO_SERVICE;
        //qmi_wds_update_connect(&qmid);
        qmi_wds_disconnect(&qmid);
    }

    //Release all CID. It is nice to be important, but more important to be nice
    if(qmid.nas_id)
        qmi_ctl_update_cid(&qmid, QMI_SERVICE_NAS, true, qmid.nas_id);

    if(qmid.wds_id)
        qmi_ctl_update_cid(&qmid, QMI_SERVICE_WDS, true, qmid.wds_id);

    if(qmid.dms_id)
        qmi_ctl_update_cid(&qmid, QMI_SERVICE_DMS, true, qmid.dms_id);

    //Make sure all the messages are sent before exiting application (and
    //closing file descriptor)
    syncfs(qmid.qmi_fd);
}

static void qmi_signal_handler(int signum){
    qmi_cleanup();
    exit(EXIT_SUCCESS);
}

static int32_t qmid_open_modem(struct qmi_device *qmid){
    //This is not nice, add proper processing of arguments later
    if((qmid->qmi_fd = open(qmid->dev_path, O_RDWR)) == -1)
        return -1;

    //Send request for CID(s). The rest will then be controlled by messages from
    //the modem.
    qmi_ctl_send_sync(qmid);
    return qmid->qmi_fd;
}

//Return value of -1 means failure
//0 means no action is needed
//>0 means that qmi_fd has been updated
static int32_t qmid_handle_timeout(struct qmi_device *qmid){
    time_t cur_time = time(NULL);

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
        QMID_DEBUG_PRINT(stderr, "Checking for timeout events\n");

    if(qmid->ctl_num_cids == QMID_NUM_SERVICES){
        if(cur_time - qmid->nas_sent_time >= QMID_TIMEOUT_SEC)
            //If IDLE, no messages to send. Everything is indication based after
            //intial configuration
            if(qmid->nas_state != NAS_IDLE)
                qmi_nas_send(qmid);

        if(cur_time - qmid->wds_sent_time >= QMID_TIMEOUT_SEC)
            //While connected, no need to query WDS
            if(qmid->wds_state != WDS_CONNECTED)
                qmi_wds_send(qmid);

        return 0;
    } else{
        if(qmid_verbose_logging >= QMID_LOG_LEVEL_2)
            QMID_DEBUG_PRINT(stderr, "CTL took to long to reply, restarting\n");

        close(qmid->qmi_fd);

        //Reset parameters
        qmid->ctl_num_cids = 0;
        qmid->ctl_transaction_id = qmid->nas_transaction_id =
            qmid->wds_transaction_id = qmid->dms_transaction_id = 1;

        return qmid_open_modem(qmid);
    }
}

//Signal handler for closing down connection and releasing cid
//Seems like I have to wait for a reply?

static void handle_msg(struct qmi_device *qmid){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) qmid->buf;

    if(qmid_verbose_logging >= QMID_LOG_LEVEL_3){
        QMID_DEBUG_PRINT(stderr, "Received (serivce %x):\n",
                qmux_hdr->service_type);
        parse_qmi(qmid->buf);
    }

    //Ignore messages arriving before I have got my sync ack
    if(qmux_hdr->service_type != QMI_SERVICE_CTL &&
            qmid->ctl_state != CTL_SYNCED)
        return;

    //TODO: Compress by making qmi_*_handle_msg a function pointer, and then
    //just look up service and store pointer
    switch(qmux_hdr->service_type){
        case QMI_SERVICE_CTL:
            //Any error in CTL is critical
            if(qmi_ctl_handle_msg(qmid) == QMI_MSG_FAILURE){
                QMID_DEBUG_PRINT(stderr, "Error in handling of control message, "
                        "aborting\n");
                qmi_cleanup();
                exit(EXIT_FAILURE);
            }

            break;
        case QMI_SERVICE_NAS:
            //This will only happen if I cant set up indications
            if(qmi_nas_handle_msg(qmid) == QMI_MSG_FAILURE){
                QMID_DEBUG_PRINT(stderr, "Error in handling of NAS messge, "
                        "aborting\n");
                qmi_cleanup();
                exit(EXIT_FAILURE);
            }
            break;
        case QMI_SERVICE_WDS:
            if(qmi_wds_handle_msg(qmid) == QMI_MSG_FAILURE){
                QMID_DEBUG_PRINT(stderr, "Error in handling of WDS message, "
                        "aborting\n");
                qmi_cleanup();
                exit(EXIT_FAILURE);
            }
            break;
        case QMI_SERVICE_DMS:
            if(qmi_dms_handle_msg(qmid) == QMI_MSG_FAILURE){
                QMID_DEBUG_PRINT(stderr, "Error in handling of DMS message, "
                        "aborting\n");
                qmi_cleanup();
                exit(EXIT_FAILURE);
            }
            break;
        default:
            QMID_DEBUG_PRINT(stderr, "Message for non-supported service (%x)\n",
                    qmux_hdr->service_type);
            break;
    }
}

static ssize_t read_data(struct qmi_device *qmid){
    ssize_t numbytes;
    qmux_hdr_t *qmux_hdr;

    //First, read a qmux header then read the data
    //TODO: Consider something more efficient than two reads. A circular buffer
    //for example.
    if(!qmid->cur_qmux_length){
        numbytes = read(qmid->qmi_fd, qmid->buf + qmid->qmux_progress,
                sizeof(qmux_hdr_t));

        if(numbytes == -1){
            if(qmid_verbose_logging >= QMID_LOG_LEVEL_1)
                QMID_DEBUG_PRINT(stderr, "Read from device failed\n");

            return numbytes;
        } else if(numbytes != sizeof(qmux_hdr_t)){
            if(qmid_verbose_logging >= QMID_LOG_LEVEL_3)
                QMID_DEBUG_PRINT(stderr, "Parial QMUX, length %zd\n", numbytes);

            qmid->qmux_progress += numbytes;
        } else {
            qmux_hdr = (qmux_hdr_t*) qmid->buf;
            qmid->qmux_progress = sizeof(qmux_hdr_t);
            //+1 is for the marker, which is also part of the data I want to
            //read
            qmid->cur_qmux_length = le16toh(qmux_hdr->length) + 1;

            //Add check for too large qmux length
        }
    } 

    //If I have received a full header, try to read data. This might happen
    //immediatly 
    if(qmid->cur_qmux_length){
        numbytes = read(qmid->qmi_fd, qmid->buf + qmid->qmux_progress,
                qmid->cur_qmux_length - qmid->qmux_progress);
        qmid->qmux_progress += numbytes;

        if(qmid->qmux_progress == qmid->cur_qmux_length){
            handle_msg(qmid);
            qmid->qmux_progress = 0;
            qmid->cur_qmux_length = 0;
        }
    }

    return numbytes;
}

//This method only returns if there has been a critical failure
static void qmid_run_eventloop(struct qmi_device *qmid){
    int32_t efd, nfds, sleep_time, retval;
    struct epoll_event ev;
    time_t cur_time, next_timeout;

    if((efd = epoll_create(1)) == -1){
        perror("epoll_create");
        return;
    }

    ev.events = EPOLLIN;
    ev.data.fd = qmid->qmi_fd;

    if(epoll_ctl(efd, EPOLL_CTL_ADD, qmid->qmi_fd, &ev) == -1){
        perror("epoll_ctl");
        return;
    }

    next_timeout = time(NULL) + 5;

    while(1){
        cur_time = time(NULL);

        if(cur_time > next_timeout)
            sleep_time = 0;
        else
            sleep_time = next_timeout - cur_time;
            
        nfds = epoll_wait(efd, &ev, 1, sleep_time*1000);

        if(nfds == -1){
            if(qmid_verbose_logging >= QMID_LOG_LEVEL_1)
                QMID_DEBUG_PRINT(stderr, "epoll_wait() failed\n");

            return;
        } else if(nfds == 0){
            retval = qmid_handle_timeout(qmid);

            if(retval == -1){
                if(qmid_verbose_logging >= QMID_LOG_LEVEL_1)
                    QMID_DEBUG_PRINT(stderr, 
                            "Could not reopen modem after timeout, abort\n");
                return;
            } else if(retval > 0){
                ev.events = EPOLLIN;
                ev.data.fd = qmid->qmi_fd;
                epoll_ctl(efd, EPOLL_CTL_ADD, qmid->qmi_fd, &ev);
            }

            next_timeout = time(NULL) + 5;
        } else{
            if(read_data(qmid) == -1){
                close(qmid->qmi_fd);
                return;
            }
        }
    }
}

struct option qmi_options[] = {
    {"device",  required_argument, NULL, 'd'},
    {"apn",     required_argument, NULL, 'a'},
    {"pin",     optional_argument, NULL, 'p'},
    {"lock",    optional_argument, NULL, 'l'},
};

static void usage(){
    fprintf(stderr, "How to run: ./qmid <arguments>\n");
    fprintf(stderr, "\t--device/-d Path to qmi device (/dev/cdc-wdmX)\n");
    fprintf(stderr, "\t--apn/-a Apn to connect to\n");
    fprintf(stderr, "\t--pin/-p PIN code (optional)\n");
    fprintf(stderr, "\t--lock/-l Lock to UMTS (optional)\n");
    fprintf(stderr, "\t-v Verbosity level (up to vvvv)\n");
}

int main(int argc, char *argv[]){
    //Should also be global, so I can access it in signal handler
    ssize_t numbytes = 0;
    struct sigaction sa;
    int c = 0;

    memset(&qmid, 0, sizeof(qmid));
   
    //Add signal handler
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = qmi_signal_handler;

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    //Default is to prefer both LTE and UMTS
    qmid.rat_mode_pref = QMI_NAS_RAT_MODE_PREF_LTE | QMI_NAS_RAT_MODE_PREF_UMTS;

    //Parse arguments
    while(1){
        c = getopt_long(argc, argv, "hvld:a:p:", qmi_options, NULL);

        if(c == -1)
            break;

        switch(c){
            case 'd':
                qmid.dev_path = optarg;
                break;
            case 'a':
                qmid.apn_name = optarg;
                break;
            case 'v':
                if(qmid_verbose_logging + 1 < QMID_LOG_LEVEL_MAX)
                    qmid_verbose_logging++;
                break;
            case 'l':
                qmid.rat_mode_pref = QMI_NAS_RAT_MODE_PREF_UMTS;
                break;
            case 'p':
                if(strlen(optarg) > QMID_MAX_LENGTH_PIN){
                    fprintf(stderr, "PIN code too long\n");
                    exit(EXIT_FAILURE);
                }
                qmid.pin_code = optarg;
                break;
            case 'h':
            default:
                usage();
                exit(EXIT_SUCCESS);
        }
    }

    if(qmid.dev_path == NULL || qmid.apn_name == NULL){
        fprintf(stderr, "Missing required argument\n");
        usage();
        exit(EXIT_FAILURE);
    }

    qmid.ctl_transaction_id = qmid.nas_transaction_id = qmid.wds_transaction_id
        = qmid.dms_transaction_id = 1;
    
    if(qmid_open_modem(&qmid) == -1){
        perror("Could not open modem");
        return EXIT_FAILURE;
    }

    qmid_run_eventloop(&qmid);

    //Only gets here if device fails to read from interface
    qmi_cleanup();
    return EXIT_FAILURE;
}
