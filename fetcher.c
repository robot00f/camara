#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "bearssl.h"

static int sock_read(void *ctx, unsigned char *buf, size_t len) {
    for (;;) {
        ssize_t rlen = read(*(int *)ctx, buf, len);
        if (rlen <= 0) {
            if (rlen < 0 && errno == EINTR) continue;
            return -1;
        }
        return (int)rlen;
    }
}

static int sock_write(void *ctx, const unsigned char *buf, size_t len) {
    for (;;) {
        ssize_t wlen = write(*(int *)ctx, buf, len);
        if (wlen <= 0) {
            if (wlen < 0 && errno == EINTR) continue;
            return -1;
        }
        return (int)wlen;
    }
}

typedef struct {
    const br_x509_class *vtable;
    const br_x509_class **inner;
} x509_noanchor_context;

static void xwc_start_chain(const br_x509_class **ctx, const char *server_name) {
    x509_noanchor_context *xwc = (x509_noanchor_context *)ctx;
    (*xwc->inner)->start_chain(xwc->inner, server_name);
}
static void xwc_start_cert(const br_x509_class **ctx, uint32_t length) {
    x509_noanchor_context *xwc = (x509_noanchor_context *)ctx;
    (*xwc->inner)->start_cert(xwc->inner, length);
}
static void xwc_append(const br_x509_class **ctx, const unsigned char *buf, size_t len) {
    x509_noanchor_context *xwc = (x509_noanchor_context *)ctx;
    (*xwc->inner)->append(xwc->inner, buf, len);
}
static void xwc_end_cert(const br_x509_class **ctx) {
    x509_noanchor_context *xwc = (x509_noanchor_context *)ctx;
    (*xwc->inner)->end_cert(xwc->inner);
}
static unsigned xwc_end_chain(const br_x509_class **ctx) {
    x509_noanchor_context *xwc = (x509_noanchor_context *)ctx;
    unsigned r = (*xwc->inner)->end_chain(xwc->inner);
    if (r == BR_ERR_X509_NOT_TRUSTED) {
        r = 0;
    }
    return r;
}
static const br_x509_pkey *xwc_get_pkey(const br_x509_class *const *ctx, unsigned *usages) {
    x509_noanchor_context *xwc = (x509_noanchor_context *)ctx;
    return (*xwc->inner)->get_pkey(xwc->inner, usages);
}

static const br_x509_class x509_noanchor_vtable = {
    sizeof(x509_noanchor_context),
    xwc_start_chain,
    xwc_start_cert,
    xwc_append,
    xwc_end_cert,
    xwc_end_chain,
    xwc_get_pkey
};

static const uint16_t suites[] = {
    BR_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
    BR_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384
};

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <host> <path_with_query> <out_file>\n", argv[0]);
        return 1;
    }
    const char *host = argv[1];
    const char *path = argv[2];
    const char *outfile = argv[3];

    struct addrinfo hints, *si, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, "443", &hints, &si) != 0) {
        fprintf(stderr, "getaddrinfo failed\n");
        return 2;
    }

    int fd = -1;
    for (p = si; p != NULL; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(si);
    if (fd < 0) {
        fprintf(stderr, "connect to %s:443 failed\n", host);
        return 3;
    }

    br_ssl_client_context sc;
    br_x509_minimal_context xc;
    x509_noanchor_context xwc;
    unsigned char iobuf[BR_SSL_BUFSIZE_BIDI];

    // Minimal client initialization: only TLS 1.2, ECDHE-RSA, AES-GCM, SHA256/384
    br_ssl_client_zero(&sc);
    br_ssl_engine_set_versions(&sc.eng, BR_TLS12, BR_TLS12);
    br_ssl_engine_set_suites(&sc.eng, suites, sizeof(suites) / sizeof(suites[0]));
    br_ssl_engine_set_default_ec(&sc.eng);
    br_ssl_engine_set_default_rsavrfy(&sc.eng);
    br_ssl_engine_set_default_aes_gcm(&sc.eng);
    br_ssl_engine_set_hash(&sc.eng, br_sha256_ID, &br_sha256_vtable);
    br_ssl_engine_set_hash(&sc.eng, br_sha384_ID, &br_sha384_vtable);
    br_ssl_engine_set_prf_sha256(&sc.eng, &br_tls12_sha256_prf);
    br_ssl_engine_set_prf_sha384(&sc.eng, &br_tls12_sha384_prf);

    // Minimal x509 initialization
    br_x509_minimal_init(&xc, &br_sha256_vtable, NULL, 0);
    br_x509_minimal_set_hash(&xc, br_sha256_ID, &br_sha256_vtable);
    br_x509_minimal_set_hash(&xc, br_sha384_ID, &br_sha384_vtable);
    br_x509_minimal_set_rsa(&xc, br_rsa_pkcs1_vrfy_get_default());

    xwc.vtable = &x509_noanchor_vtable;
    xwc.inner = &xc.vtable;
    br_ssl_engine_set_x509(&sc.eng, &xwc.vtable);

    br_ssl_engine_set_buffer(&sc.eng, iobuf, sizeof iobuf, 1);
    br_ssl_client_reset(&sc, host, 0);

    br_sslio_context ioc;
    br_sslio_init(&ioc, &sc.eng, sock_read, &fd, sock_write, &fd);

    char req[4096];
    int req_len = snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: Sentinel/1.0\r\n"
        "Connection: close\r\n\r\n",
        path, host);

    if (br_sslio_write_all(&ioc, req, req_len) < 0 || br_sslio_flush(&ioc) < 0) {
        int err = br_ssl_engine_last_error(&sc.eng);
        fprintf(stderr, "SSL write failed, error: %d\n", err);
        close(fd);
        return 4;
    }

    int out_fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0755);
    if (out_fd < 0) {
        close(fd);
        return 5;
    }

    char header_buf[4096];
    int header_pos = 0;
    int body_started = 0;
    size_t total_bytes = 0;

    unsigned char buf[4096];
    for (;;) {
        int rlen = br_sslio_read(&ioc, buf, sizeof(buf));
        if (rlen < 0) break;
        if (!body_started) {
            for (int i = 0; i < rlen; i++) {
                if (header_pos < (int)sizeof(header_buf) - 1) {
                    header_buf[header_pos++] = (char)buf[i];
                    header_buf[header_pos] = '\0';
                }
                if (header_pos >= 4 &&
                    header_buf[header_pos - 4] == '\r' &&
                    header_buf[header_pos - 3] == '\n' &&
                    header_buf[header_pos - 2] == '\r' &&
                    header_buf[header_pos - 1] == '\n') {
                    body_started = 1;
                    int remaining = rlen - (i + 1);
                    if (remaining > 0) {
                        write(out_fd, buf + i + 1, remaining);
                        total_bytes += remaining;
                    }
                    break;
                }
            }
        } else {
            write(out_fd, buf, rlen);
            total_bytes += rlen;
        }
    }

    close(out_fd);
    br_ssl_engine_close(&sc.eng);
    close(fd);

    fprintf(stderr, "Download finished: %zu bytes written to %s\n", total_bytes, outfile);
    return 0;
}
