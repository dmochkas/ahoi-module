#include "ahoi-msg.h"

#include <stdbool.h>
#include <string.h>

#define AHOI_DLE  0x10
#define AHOI_STX  0x02
#define AHOI_ETX  0x03

const uint8_t* ahoi_find_packet(const uint8_t* data, size_t length) {
    const uint8_t* data_ptr = data;
    const uint8_t* data_end_ptr = data + length;
    while (1) {
        uint8_t* start = memchr(data_ptr, AHOI_DLE, length);
        if (start == NULL || (data_end_ptr - start) <= 1) {
            return data + length;
        }
        if (start[1] == AHOI_STX) {
            return start;
        }
        data_ptr = start + 1;
    }
}

packet_decode_status_t ahoi_decode_packet(const uint8_t* pkt_encoded, size_t max_len, ahoi_packet_t* pkt, const uint8_t** pkt_encoded_end) {
    if (pkt_encoded == NULL || max_len < 10 || pkt_encoded[0] != AHOI_DLE || pkt_encoded[1] != AHOI_STX) {
        return AHOI_DECODE_KO;
    }

    uint8_t* pkt_bytes = (uint8_t*) pkt;
    int pkt_index = 0;
    uint8_t byte;
    bool in_dle = false;
    for (int i = 2; i < max_len; ++i) {
        byte = pkt_encoded[i];
        if (in_dle) {
            if (byte == AHOI_ETX) {
                if (pkt_encoded_end != NULL) {
                    *pkt_encoded_end = pkt_encoded + i + 1;
                }
                return AHOI_DECODE_OK;
            }
            // Not expected
            if (byte != AHOI_DLE) {
                if (pkt_encoded_end != NULL) {
                    *pkt_encoded_end = pkt_encoded + i + 1;
                }
                return AHOI_DECODE_KO;
            }
            in_dle = false;
        } else if (byte == AHOI_DLE) {
            in_dle = true;
            continue;
        }

        pkt_bytes[pkt_index++] = byte;
    }

    return AHOI_DECODE_NOT_COMPLETE;
}

static int io_mem_write(ahoi_writer_t *w, const uint8_t* buf_in, size_t n)
{
    if (w->buf == NULL) {
        w->pos += n;
        return (int) n;
    }

    if (w->pos >= w->buf_len) {
        return -1;
    }

    size_t remaining = w->buf_len - w->pos;
    if (n > remaining) {
        n = remaining;
    }

    memcpy(w->buf + w->pos, buf_in, n);
    w->pos += n;

    return (int) n;
}

packet_encode_status_t ahoi_encode_hdr_pld(ahoi_writer_t* writer, const ahoi_header_t* hdr, const uint8_t* pld) {
    const uint8_t* hdr_bytes = (const uint8_t*) hdr;

    if (hdr->pl_size > AHOI_MAX_PAYLOAD_SIZE) {
        return AHOI_ENCODE_KO;
    }

    // Write STX
    if (io_mem_write(writer, (uint8_t[]){AHOI_DLE, AHOI_STX}, 2) != 2) return AHOI_ENCODE_KO;

    for (int i = 0; i < AHOI_HEADER_SIZE; ++i) {
        uint8_t byte = hdr_bytes[i];
        if (byte == AHOI_DLE) {
            if (io_mem_write(writer, (uint8_t[]){AHOI_DLE, AHOI_DLE}, 2) != 2) return AHOI_ENCODE_KO;
        } else {
            if (io_mem_write(writer, &byte, 1) != 1) return AHOI_ENCODE_KO;
        }
    }

    for (size_t i = 0; i < hdr->pl_size; i++) {
        uint8_t byte = pld[i];
        if (byte == AHOI_DLE) {
            if (io_mem_write(writer, (uint8_t[]){AHOI_DLE, AHOI_DLE}, 2) != 2) return AHOI_ENCODE_KO;
        } else {
            if (io_mem_write(writer, &byte, 1) != 1) return AHOI_ENCODE_KO;
        }
    }

    // Write ETX
    if (io_mem_write(writer, (uint8_t[]){AHOI_DLE, AHOI_ETX}, 2) != 2) return AHOI_ENCODE_KO;

    return AHOI_ENCODE_OK;
}

packet_encode_status_t ahoi_encode_packet(ahoi_writer_t* writer, const ahoi_packet_t* pkt) {
    return ahoi_encode_hdr_pld(writer, (const ahoi_header_t*) pkt, pkt->payload);
}