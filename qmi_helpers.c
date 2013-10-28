#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "qmi_hdrs.h"
#include "qmi_shared.h"

void create_qmi_request(uint8_t *buf, uint8_t service, uint8_t client_id, 
        uint16_t transaction_id, uint16_t message_id){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) buf;

    if(service == QMI_SERVICE_CTL)
        qmux_hdr->length = sizeof(qmux_hdr_t) + sizeof(qmi_hdr_ctl_t);
    else
        qmux_hdr->length = sizeof(qmux_hdr_t) + sizeof(qmi_hdr_gen_t);

    qmux_hdr->type = QMUX_IF_TYPE;
    //Messages are send from the control point
    qmux_hdr->control_flags = 0;
    //Which service I want to request something from. Remember that CTL is 0
    qmux_hdr->service_type = service;
    qmux_hdr->client_id = client_id;

    //Can I somehow do this more elegant?
    if(service == QMI_SERVICE_CTL){
        qmi_hdr_ctl_t *qmi_hdr = (qmi_hdr_ctl_t*) (qmux_hdr+1);
        //Always sends request (message type 0, only flag)
        qmi_hdr->control_flags = 0;
        //Internal transaction sequence number (one message exchange)
        qmi_hdr->transaction_id = transaction_id;
        //Type of message
        qmi_hdr->message_id = message_id;
        qmi_hdr->length = 0;
    } else{
        qmi_hdr_gen_t *qmi_hdr = (qmi_hdr_gen_t*) (qmux_hdr+1);
        qmi_hdr->control_flags = 0;
        qmi_hdr->transaction_id = transaction_id;
        qmi_hdr->message_id = message_id;
        qmi_hdr->length = 0;
    }
}

//Assume only one TLV parameter for now
void add_tlv(uint8_t *buf, uint8_t type, uint16_t length, void *value){
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) buf;
    qmi_tlv_t *tlv;

    assert(qmux_hdr->length + length + sizeof(qmi_tlv_t) < QMI_DEFAULT_BUF_SIZE);

    tlv = (qmi_tlv_t*) (buf + qmux_hdr->length);
    tlv->type = type;
    tlv->length = length;
    memcpy(tlv + 1, value, length);

    //Update the length of thw qmux and qmi headers
    qmux_hdr->length += sizeof(qmi_tlv_t) + length;

    //Updte QMI service length
    if(qmux_hdr->service_type == QMI_SERVICE_CTL){
        qmi_hdr_ctl_t *qmi_hdr = (qmi_hdr_ctl_t*) (qmux_hdr+1);
        qmi_hdr->length += sizeof(qmi_tlv_t) + length;
    } else {
        qmi_hdr_gen_t *qmi_hdr = (qmi_hdr_gen_t*) (qmux_hdr+1);
        qmi_hdr->length += sizeof(qmi_tlv_t) + length;
    }
}

void parse_qmi(uint8_t *buf){
    int i, j;
    uint8_t *tlv_val = NULL;
    qmux_hdr_t *qmux_hdr = (qmux_hdr_t*) buf;
    qmi_tlv_t *tlv = NULL;
    uint16_t tlv_length = 0;

    fprintf(stderr, "Complete message:\n");
    for(i=0; i < qmux_hdr->length; i++)
        fprintf(stderr, "%.2x:", buf[i]);
    fprintf(stderr, "%.2x\n\n", buf[i]);

    fprintf(stderr, "QMUX:\n");
    fprintf(stderr, "\tlength: %u\n", qmux_hdr->length);
    fprintf(stderr, "\tflags: 0x%.2x\n", qmux_hdr->control_flags);
    fprintf(stderr, "\tservice: 0x%.2x\n", qmux_hdr->service_type);
    fprintf(stderr, "\tclient id: %u\n", qmux_hdr->client_id);

    if(qmux_hdr->service_type == QMI_SERVICE_CTL){
        qmi_hdr_ctl_t *qmi_hdr = (qmi_hdr_ctl_t*) (qmux_hdr+1);
        fprintf(stderr, "QMI (service):\n");
        fprintf(stderr, "\tflags: %u\n", qmi_hdr->control_flags >> 1);
        fprintf(stderr, "\ttransaction id: %u\n", qmi_hdr->transaction_id);
        fprintf(stderr, "\tmessage type: 0x%.2x\n", qmi_hdr->message_id);
        fprintf(stderr, "\tlength: %u\n", qmi_hdr->length);
        tlv = (qmi_tlv_t *) (qmi_hdr+1);
        tlv_length = qmi_hdr->length;
    } else {
        qmi_hdr_gen_t *qmi_hdr = (qmi_hdr_gen_t*) (qmux_hdr+1);
        fprintf(stderr, "QMI (control):\n");
        fprintf(stderr, "\tflags: %u\n", qmi_hdr->control_flags >> 1);
        fprintf(stderr, "\ttransaction id: %u\n", qmi_hdr->transaction_id);
        fprintf(stderr, "\tmessage type: 0x%.2x\n", qmi_hdr->message_id);
        fprintf(stderr, "\tlength: %u\n", qmi_hdr->length);
        tlv = (qmi_tlv_t *) (qmi_hdr+1);
        tlv_length = qmi_hdr->length;
    }

    i=0;
    while(i<tlv_length){
        tlv_val = (uint8_t*) (tlv+1);
        fprintf(stderr, "TLV:\n");
        fprintf(stderr, "\ttype: 0x%.2x\n", tlv->type);
        fprintf(stderr, "\tlen: %u\n", tlv->length);
        fprintf(stderr, "\tvalue: ");
        
        for(j=0; j<tlv->length-1; j++)
            fprintf(stderr, "%.2x:", tlv_val[j]);
        fprintf(stderr, "%.2x", tlv_val[j]);

        fprintf(stderr, "\n");
        i += sizeof(qmi_tlv_t) + tlv->length;

        if(i==tlv_length)
            break;
        else
            tlv = (qmi_tlv_t*) (((uint8_t*) (tlv+1)) + tlv->length);
    }
}

ssize_t qmi_helpers_write(int32_t qmi_fd, uint8_t *buf, ssize_t len){
    return write(qmi_fd, buf, len);
}
