
#include "ahoi-module.h"

#include <errno.h>
#include <string.h>
#include <stdbool.h>

#define AHOI_DLE  0x10
#define AHOI_STX  0x02
#define AHOI_ETX  0x03

typedef enum {
    PACKET_SEND_OK,
    PACKET_SEND_KO
} packet_send_status_t;

typedef enum packet_decode_status {
    AHOI_DECODE_OK,
    AHOI_DECODE_NOT_COMPLETE,
    AHOI_DECODE_KO
} packet_decode_status_t;

typedef enum packet_encode_status {
    AHOI_ENCODE_OK,
    AHOI_ENCODE_KO,
} packet_encode_status_t;

typedef struct ahoi_con {
    dev_con_t con;
    writer_t recv_writer;
} ahoi_con_t;

typedef struct ahoi_con_table {
    ahoi_con_t ahoi_cons[AHOI_MAX_CONNECTIONS];
    uint8_t len;
} ahoi_con_table_t;

static ahoi_con_table_t ahoi_con_table = {0};

static ahoi_con_t* ahoi_find_con(dev_con_t con) {
    for (int i = 0; i < ahoi_con_table.len; ++i) {
        ahoi_con_t* curr = &ahoi_con_table.ahoi_cons[i];
        if (curr->con == con) {
            return curr;
        }
    }

    return NULL;
}

static const uint8_t* ahoi_find_packet(const uint8_t* data, size_t length) {
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

static packet_decode_status_t ahoi_decode_packet(const uint8_t* pkt_encoded, size_t max_len, ahoi_packet_t* pkt, const uint8_t** pkt_encoded_end) {
    if (pkt_encoded == NULL || max_len < 10 || pkt_encoded[0] != AHOI_DLE || pkt_encoded[1] != AHOI_STX) {
        return -1;
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

static consumer_status_t recv_handler(const uint8_t* data, size_t length, size_t* n_consumed, void* ctx) {
    consumer_status_t ret = CONSUMER_OK;

    ahoi_packet_consumer_t packet_handler = (ahoi_packet_consumer_t) ctx;
    ahoi_packet_t pkt = {0};

    const uint8_t* data_end_ptr = data + length;
    const uint8_t* pkt_encoded_start = ahoi_find_packet(data, length);
    if (pkt_encoded_start == data_end_ptr) {
        // Did not find, still ok
        *n_consumed = length;
        goto finish;
    }
    const uint8_t* pkt_encoded_end;
    int decode_st = ahoi_decode_packet(pkt_encoded_start, data_end_ptr - pkt_encoded_start, &pkt, &pkt_encoded_end);
    if (decode_st == 0) {
        // Packet decode not finished
        ret = CONSUMER_UNFINISHED;
        *n_consumed = length;
        goto finish;
    }
    *n_consumed = pkt_encoded_end - data;
    if (decode_st < 0) {
        // Packet is corrupt, still ok
        goto finish;
    }
    ret = packet_handler(&pkt);

    finish:
    return ret;
}

int io_mem_write(writer_t *w, const uint8_t* buf_in, size_t n)
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

static packet_encode_status_t ahoi_encode_hdr_pld(writer_t* writer, const ahoi_header_t* hdr, const uint8_t* pld) {
    const uint8_t* hdr_bytes = (const uint8_t*) hdr;

    if (hdr->pl_size > AHOI_MAX_PAYLOAD_SIZE) {
        // TODO: Log error
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

static packet_encode_status_t ahoi_encode_packet(writer_t* writer, const ahoi_packet_t* pkt) {
    return ahoi_encode_hdr_pld(writer, (const ahoi_header_t*) pkt, pkt->payload);
}

// TODO: Log
// TODO: Something ugly here. Need to refactor
// Will error if ongoing tx or rx
static packet_send_status_t send_ahoi_cmd(dev_con_t con, const ahoi_packet_t* pkt, uint8_t* resp_buf, size_t resp_buf_len, size_t* resp_len) {
    if (!AHOI_IS_COMMAND_PACKET(pkt)) {
//        fprintf(stderr, "Expecting ahoi command packet\n");
        return PACKET_SEND_KO;
    }
    uint8_t cmd_id = pkt->type;

    // TODO: Ensure that there is no data in read in the socket

    writer_t writer = {0};
    ahoi_encode_packet(&writer, pkt);
    uint8_t cmd_buf[writer.pos];

    writer.buf = cmd_buf;
    writer.buf_len = writer.pos;
    writer.pos = 0;
    if (ahoi_encode_packet(&writer, pkt) != AHOI_ENCODE_OK) {
        return PACKET_SEND_KO;
    }

    int n = io_tx(con, cmd_buf, writer.pos);
    if (n < 0) {
//        fprintf(stderr, "Error writing to serial port\n");
        return PACKET_SEND_KO;
    }
    if (n != writer.pos) {
//        fprintf(stderr, "Warning: Partial write (%zd of %lu bytes)\n", bytes_written, len);
        return PACKET_SEND_KO;
    }

    uint8_t cmd_resp_buf[16];
    uint8_t cmd_resp_buf_len = sizeof(cmd_resp_buf);
    // TODO: define 100 ms
    if ((n = io_rx_blocking(con, cmd_resp_buf, cmd_resp_buf_len, 100)) < 1) {
        // TODO: Log
        return PACKET_SEND_KO;
    }

    ahoi_packet_t resp = {0};
    if (ahoi_decode_packet(cmd_resp_buf, n, &resp, NULL) < 0) {
        return PACKET_SEND_KO;
    }

    if (pkt->type != cmd_id) {
//        fprintf(stderr, "Ahoi cmd is malformed\n");
        return PACKET_SEND_KO;
    }

    if (resp_buf != NULL) {
        if (resp_buf_len < resp.pl_size) {
//            fprintf(stderr, "Response buffer is too small\n");
            return PACKET_SEND_KO;
        }
        memcpy(resp_buf, resp.payload, resp.pl_size);
        *resp_len = resp.pl_size;
    }
    return PACKET_SEND_OK;
}

static command_set_status_t ahoi_set_command(dev_con_t con, const uint8_t type, const uint8_t* payload, const size_t pl_len) {
    ahoi_packet_t pkt = {
            .type = type,
            .pl_size = pl_len
    };
    memcpy(pkt.payload, payload, pl_len);

    return (command_set_status_t) send_ahoi_cmd(con, &pkt, NULL, 0, NULL);
}

command_set_status_t set_ahoi_id(dev_con_t con, const uint8_t id) {
    return ahoi_set_command(con, AHOI_ID_CMD, &id, 1);
}

int ahoi_connect(const ahoi_init_t* inits, uint8_t inits_len, dev_con_t* cons) {
    for (int i = 0; i < inits_len; ++i) {
        const ahoi_init_t* curr_init = &inits[i];
        ahoi_con_t* curr_con = &ahoi_con_table.ahoi_cons[i];
        curr_con->con = io_open_serial(curr_init->open_addr, curr_init->baud);
        if (curr_con->con < 0) {
            goto error;
        }

        io_tx_drain(curr_con->con);
        io_rx_drain(curr_con->con);

        if (set_ahoi_id(curr_con->con, curr_init->id) != AHOI_COMMAND_SET_OK) {
            goto error;
        }

        // TODO: Should not be called receive buffer rather send_recv
        curr_con->recv_writer.buf = curr_init->recv_buf;
        curr_con->recv_writer.buf_len = curr_init->recv_buf_len;
        curr_con->recv_writer.chunk = AHOI_READ_SIZE;
        curr_con->recv_writer.pos = 0;

        cons[i] = curr_con->con;
        ahoi_con_table.len++;
    }

    return 1;

    error:
    ahoi_disconnect();
    return -1;
}

rx_status_t ahoi_stateful_read(dev_con_t con, ahoi_packet_consumer_t pkt_consumer) {
    ahoi_con_t* ahoi = ahoi_find_con(con);
    if (ahoi == NULL) {
        errno = EINVAL;
        return -1;
    }
    return io_stateful_rx(con, 0, &ahoi->recv_writer, recv_handler, pkt_consumer);
}

static int ahoi_handle_ack(dev_con_t con) {
    // TODO: Handle ack
    uint8_t sack_cmd[10];
    int n = io_rx_blocking(con, sack_cmd, 10, 100);
    // Encoded ACK size
    if (n != 10) {
        goto error;
    }

    ahoi_header_t header = {0};
    if (ahoi_decode_packet(sack_cmd, sizeof(sack_cmd), (ahoi_packet_t*) &header, NULL) != AHOI_DECODE_OK) {
        goto error;
    }

    if (!AHOI_IS_SERIAL_ACK(&header)) {
        goto error;
    }

    return 1;

    error:
    return -1;
}

int ahoi_write_hdr_pld(dev_con_t con, const ahoi_header_t* hdr, const uint8_t* pld) {
    writer_t w = {0};
    ahoi_encode_hdr_pld(&w, hdr, pld);
    uint8_t tx_buf[w.pos];
    w.buf = tx_buf;
    w.buf_len = w.pos;
    w.pos = 0;
    if (ahoi_encode_hdr_pld(&w, hdr, pld) != AHOI_ENCODE_OK) {
        goto error;
    }

    if (io_tx(con, tx_buf, w.pos) < 0) {
        goto error;
    }

    return ahoi_handle_ack(con);

    error:
    return -1;
}

int ahoi_write(dev_con_t con, const ahoi_packet_t* pkt) {
    return ahoi_write_hdr_pld(con, (const ahoi_header_t*) pkt, pkt->payload);
}

void ahoi_disconnect() {
    for (int i = 0; i < ahoi_con_table.len; ++i) {
        io_close(ahoi_con_table.ahoi_cons[i].con);
    }
}