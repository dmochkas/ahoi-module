#pragma once

#include <stdint.h>
#include <stddef.h>

// Custom io lib
#include <io.h>

#define AHOI_MAX_CONNECTIONS 1
#define AHOI_READ_SIZE 127
#define AHOI_READ_GUARD_TIME_MS 50

#define AHOI_MAX_PAYLOAD_SIZE 127
#define AHOI_HEADER_SIZE 6
#define AHOI_MAX_PACKET_SIZE (AHOI_MAX_PAYLOAD_SIZE + AHOI_HEADER_SIZE)
#define AHOI_FOOTER_SIZE 6

#define AHOI_ACK_TYPE 0x7F
#define AHOI_ID_CMD 0x84
#define AHOI_IS_DATA_PACKET(pkt) ((pkt)->type < 0x7F)
#define AHOI_IS_ACK(pkt) ((pkt)->type == AHOI_ACK_TYPE)
#define AHOI_IS_COMMAND_PACKET(pkt) ((pkt)->type >= 0x80 && (pkt)->type <= 0xFD)
#define AHOI_IS_SERIAL_ACK(pkt) ((pkt)->type == 0xFF)
#define AHOI_IS_SERIAL_NACK(pkt) ((pkt)->type == 0xFE)
#define AHOI_IS_FOOTER_CARRIER(pkt) (AHOI_IS_DATA_PACKET(pkt) || AHOI_IS_ACK(pkt))
#define AHOI_BROADCAST_ADDR 255
#define AHOI_IS_BROADCAST(pkt) ((pkt)->dst == AHOI_BROADCAST_ADDR)

typedef struct ahoi_init {
    dev_open_addr_t open_addr;
    int baud;
    uint8_t id;
    uint8_t* recv_buf;
    uint16_t recv_buf_len;
} ahoi_init_t;

// A and R flags are incompatible
typedef enum ahoi_packet_flags {
    AHOI_NO_FLAGS = 0x00,
    AHOI_A_FLAG = 0x01,
    AHOI_R_FLAG = 0x02,
    AHOI_E_FLAG = 0x04,
    AHOI_AE_FLAGS = 0x05,
    AHOI_RE_FLAGS = 0x06,
} ahoi_packet_flags_t;

typedef struct {
    uint8_t src;
    uint8_t dst;
    uint8_t type;
    uint8_t flags;
    uint8_t seq;
    uint8_t pl_size;
} ahoi_header_t;

typedef struct {
    uint8_t power;
    uint8_t rssi;
    uint8_t biterrors;
    uint8_t agcMean;
    uint8_t agcMin;
    uint8_t agcMax;
} ahoi_footer_t;

typedef struct {
    uint8_t src;
    uint8_t dst;
    uint8_t type;
    uint8_t flags;
    uint8_t seq;
    uint8_t pl_size;
    uint8_t payload[AHOI_MAX_PAYLOAD_SIZE];
    ahoi_footer_t footer;
} ahoi_packet_t;

typedef enum {
    AHOI_COMMAND_SET_OK,
    AHOI_COMMAND_SET_KO
} command_set_status_t;

typedef consumer_status_t (*ahoi_packet_consumer_t)(ahoi_packet_t*);

int ahoi_connect(const ahoi_init_t* inits, uint8_t inits_len, dev_con_t* cons);

command_set_status_t set_ahoi_id(dev_con_t con, uint8_t id);

void ahoi_disconnect();