
#include "ahoi-module.h"

#include <errno.h>
#include <string.h>

#ifdef zlog_FOUND
#include <zlog.h>
#else
#define zlog_fatal(cat, ...)
#define zlog_error(cat, ...)
#define zlog_warn(cat, ...)
#define zlog_notice(cat, ...)
#define zlog_info(cat, ...)
#define zlog_debug(cat, ...)
#endif

#ifdef __zlog_h
static zlog_category_t* ahoi_log_cat = NULL;
#endif

typedef enum {
    PACKET_SEND_OK,
    PACKET_SEND_KO
} packet_send_status_t;

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

static consumer_status_t recv_handler(const uint8_t* data, size_t length, size_t* n_consumed, void* ctx) {
    consumer_status_t ret = CONSUMER_OK;

    ahoi_packet_consumer_t packet_handler = (ahoi_packet_consumer_t) ctx;
    ahoi_packet_t pkt = {0};

    const uint8_t* data_end_ptr = data + length;
    const uint8_t* pkt_encoded_start = ahoi_find_packet(data, length);
    if (pkt_encoded_start == data_end_ptr) {
        zlog_debug(ahoi_log_cat, "No AHOI packet found in %zu bytes", length);
        // Did not find, still ok
        *n_consumed = length;
        goto finish;
    }
    const uint8_t* pkt_encoded_end = data;
    packet_decode_status_t decode_st = ahoi_decode_packet(pkt_encoded_start, data_end_ptr - pkt_encoded_start, &pkt, &pkt_encoded_end);
    if (decode_st == AHOI_DECODE_NOT_COMPLETE) {
        zlog_debug(ahoi_log_cat, "AHOI packet decode unfinished, consumed=%zu", length);
        // Packet decode not finished
        ret = CONSUMER_UNFINISHED;
        *n_consumed = length;
        goto finish;
    }
    *n_consumed = pkt_encoded_end - data;
    if (decode_st == AHOI_DECODE_KO) {
        zlog_error(ahoi_log_cat, "AHOI packet decode failed, consumed=%zu", *n_consumed);
        // Packet is corrupt, still ok
        goto finish;
    }
    ret = packet_handler(&pkt);

    finish:
    return ret;
}

// TODO: Log
// TODO: Something ugly here. Need to refactor
// Will error if ongoing tx or rx
static packet_send_status_t send_ahoi_cmd(dev_con_t con, const ahoi_packet_t* pkt, uint8_t* resp_buf, size_t resp_buf_len, size_t* resp_len) {
    if (!AHOI_IS_COMMAND_PACKET(pkt)) {
        zlog_error(ahoi_log_cat, "Expected command packet, type=%u", pkt->type);
        return PACKET_SEND_KO;
    }
    uint8_t cmd_id = pkt->type;

    // TODO: Ensure that there is no data in read in the socket

    ahoi_writer_t writer = {0};
    ahoi_encode_packet(&writer, pkt);
    uint8_t cmd_buf[writer.pos];

    writer.buf = cmd_buf;
    writer.buf_len = writer.pos;
    writer.pos = 0;
    if (ahoi_encode_packet(&writer, pkt) != AHOI_ENCODE_OK) {
        zlog_error(ahoi_log_cat, "Failed to encode AHOI command packet");
        return PACKET_SEND_KO;
    }

    int n = io_tx(con, cmd_buf, (int) writer.pos);
    if (n < 0) {
        zlog_error(ahoi_log_cat, "Failed to write command to device con=%d", con);
        return PACKET_SEND_KO;
    }
    if (n != writer.pos) {
        zlog_error(ahoi_log_cat, "Partial write: %d of %zu bytes", n, writer.pos);
        return PACKET_SEND_KO;
    }

    uint8_t cmd_resp_buf[16];
    uint8_t cmd_resp_buf_len = sizeof(cmd_resp_buf);
    // TODO: define 100 ms
    if ((n = io_rx_blocking(con, cmd_resp_buf, cmd_resp_buf_len, 100)) < 1) {
        zlog_error(ahoi_log_cat, "Command response read failed or timed out, n=%d", n);
        return PACKET_SEND_KO;
    }

    ahoi_packet_t resp = {0};
    if (ahoi_decode_packet(cmd_resp_buf, n, &resp, NULL) != AHOI_DECODE_OK) {
        zlog_error(ahoi_log_cat, "Failed to decode command response");
        return PACKET_SEND_KO;
    }

    if (pkt->type != cmd_id) {
        zlog_error(ahoi_log_cat, "Malformed command response: type mismatch cmd=%u resp=%u", cmd_id, resp.type);
        return PACKET_SEND_KO;
    }

    if (resp_buf != NULL) {
        if (resp_buf_len < resp.pl_size) {
            zlog_error(ahoi_log_cat, "Response buffer too small: %zu < %u", resp_buf_len, resp.pl_size);
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
#ifdef __zlog_h
    if (ahoi_log_cat == NULL) {
        ahoi_log_cat = zlog_get_category(AHOI_LOG_TAG);
        if (ahoi_log_cat == NULL) {
            zlog_warn(ahoi_log_cat, "Failed to get zlog category '%s'", AHOI_LOG_TAG);
        }
    }
#endif

    for (int i = 0; i < inits_len; ++i) {
        const ahoi_init_t* curr_init = &inits[i];
        ahoi_con_t* curr_con = &ahoi_con_table.ahoi_cons[i];
        curr_con->con = io_open_serial(curr_init->open_addr, curr_init->baud);
        if (curr_con->con < 0) {
            zlog_error(ahoi_log_cat, "Failed to open serial '%s' at %u baud", curr_init->open_addr, curr_init->baud);
            goto error;
        }

        io_tx_drain(curr_con->con);
        io_rx_drain(curr_con->con);

        if (set_ahoi_id(curr_con->con, curr_init->id) != AHOI_COMMAND_SET_OK) {
            zlog_error(ahoi_log_cat, "Failed to set AHOI id=%u on con=%d", curr_init->id, curr_con->con);
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
        return READ_INVALID_PARAM;
    }
    return io_stateful_rx(con, 0, &ahoi->recv_writer, recv_handler, pkt_consumer);
}

rx_status_t ahoi_stateless_read(dev_con_t con, uint16_t guard_time_ms, ahoi_packet_consumer_t pkt_consumer) {
    ahoi_con_t* ahoi = ahoi_find_con(con);
    if (ahoi == NULL) {
        errno = EINVAL;
        return READ_INVALID_PARAM;
    }
    return io_stateless_rx(con, guard_time_ms, ahoi->recv_writer.buf, ahoi->recv_writer.buf_len, recv_handler, pkt_consumer);
}

static int ahoi_handle_sack(dev_con_t con) {
    // TODO: Handle ack
    uint8_t sack_cmd[10];
    int n = io_rx_blocking(con, sack_cmd, 10, 100);
    // Encoded ACK size
    if (n != 10) {
        zlog_error(ahoi_log_cat, "Invalid serial ACK size: %d", n);
        goto error;
    }

    ahoi_header_t header = {0};
    if (ahoi_decode_packet(sack_cmd, sizeof(sack_cmd), (ahoi_packet_t*) &header, NULL) != AHOI_DECODE_OK) {
        zlog_error(ahoi_log_cat, "Failed to decode serial ACK packet");
        goto error;
    }

    if (!AHOI_IS_SERIAL_ACK(&header)) {
        zlog_error(ahoi_log_cat, "Received non-ACK packet while awaiting serial ACK");
        goto error;
    }

    zlog_debug(ahoi_log_cat, "Serial ACK handle OK");
    return 1;

    error:
    return -1;
}

int ahoi_write_hdr_pld(dev_con_t con, const ahoi_header_t* hdr, const uint8_t* pld) {
    ahoi_writer_t w = {0};
    ahoi_encode_hdr_pld(&w, hdr, pld);
    uint8_t tx_buf[w.pos];
    w.buf = tx_buf;
    w.buf_len = w.pos;
    w.pos = 0;
    if (ahoi_encode_hdr_pld(&w, hdr, pld) != AHOI_ENCODE_OK) {
        zlog_error(ahoi_log_cat, "Failed to encode outgoing packet");
        goto error;
    }

    if (io_tx(con, tx_buf, w.pos) < 0) {
        zlog_error(ahoi_log_cat, "Failed to write packet to device con=%d", con);
        goto error;
    }

    return ahoi_handle_sack(con);

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