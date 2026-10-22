#pragma once

// Custom io lib
#include <io.h>

#include "ahoi-msg.h"

typedef struct ahoi_init {
    dev_open_addr_t open_addr;
    int baud;
    uint8_t id;
    uint8_t* recv_buf;
    uint16_t recv_buf_len;
} ahoi_init_t;

typedef enum {
    AHOI_COMMAND_SET_OK,
    AHOI_COMMAND_SET_KO
} command_set_status_t;

typedef consumer_status_t (*ahoi_packet_consumer_t)(ahoi_packet_t*);

int ahoi_connect(const ahoi_init_t* inits, uint8_t inits_len, dev_con_t* cons);

command_set_status_t set_ahoi_id(dev_con_t con, uint8_t id);

rx_status_t ahoi_stateful_read(dev_con_t con, ahoi_packet_consumer_t pkt_consumer);

rx_status_t ahoi_stateless_read(dev_con_t con, uint16_t guard_time_ms, ahoi_packet_consumer_t pkt_consumer);

int ahoi_write(dev_con_t con, const ahoi_packet_t* pkt);

int ahoi_write_hdr_pld(dev_con_t con, const ahoi_header_t* header, const uint8_t* payload);

void ahoi_disconnect(void);