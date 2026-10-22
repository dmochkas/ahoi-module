#pragma once

#include <stdint.h>
#include <stddef.h>

#include "ahoi-defs.h"

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

typedef enum packet_decode_status {
    AHOI_DECODE_OK,
    AHOI_DECODE_NOT_COMPLETE,
    AHOI_DECODE_KO
} packet_decode_status_t;

typedef enum packet_encode_status {
    AHOI_ENCODE_OK,
    AHOI_ENCODE_KO,
} packet_encode_status_t;

typedef struct ahoi_writer {
    uint8_t* buf;
    size_t buf_len;
    size_t pos;
} ahoi_writer_t;

const uint8_t* ahoi_find_packet(const uint8_t* data, size_t length);

packet_decode_status_t ahoi_decode_packet(const uint8_t* pkt_encoded, size_t max_len, ahoi_packet_t* pkt, const uint8_t** pkt_encoded_end);

packet_encode_status_t ahoi_encode_packet(ahoi_writer_t* writer, const ahoi_packet_t* pkt);

packet_encode_status_t ahoi_encode_hdr_pld(ahoi_writer_t* writer, const ahoi_header_t* hdr, const uint8_t* pld);