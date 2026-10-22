#pragma once

#define AHOI_MAX_CONNECTIONS 1
#define AHOI_READ_SIZE 127

#define AHOI_MAX_PAYLOAD_SIZE 127
#define AHOI_HEADER_SIZE 6
#define AHOI_MAX_PACKET_SIZE (AHOI_MAX_PAYLOAD_SIZE + AHOI_HEADER_SIZE)
#define AHOI_FOOTER_SIZE 6
#define AHOI_MAX_ENCODED_SIZE ((AHOI_MAX_PACKET_SIZE + AHOI_FOOTER_SIZE) * 2 + 4)

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

#define AHOI_LOG_TAG "AHOI"

// A and R flags are incompatible
typedef enum ahoi_packet_flags {
    AHOI_NO_FLAGS = 0x00,
    AHOI_A_FLAG = 0x01,
    AHOI_R_FLAG = 0x02,
    AHOI_E_FLAG = 0x04,
    AHOI_AE_FLAGS = 0x05,
    AHOI_RE_FLAGS = 0x06,
} ahoi_packet_flags_t;