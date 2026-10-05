#ifndef _MAC_OS

#include "HttpsGet.h"
#include "Utils.h"
#include "url.h"

#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/x509_crt.h"

static const int HTTPS_MAX_REDIRECTS = 5;
static const size_t HTTPS_MAX_BODY = 12 * 1024 * 1024;
static const uint32_t HTTPS_READ_TIMEOUT_MS = 30000;

static bool loadSystemCa(mbedtls_x509_crt &cacert) {
    static const char *kCaFiles[] = {
        "/etc/ssl/cert.pem",
        "/etc/ssl/certs/ca-certificates.crt",
        "/etc/pki/tls/certs/ca-bundle.crt",
        "/usr/lib/ssl/cert.pem",
    };
    for (auto *fn : kCaFiles) {
        if (mbedtls_x509_crt_parse_file(&cacert, fn) == 0) {
            return true;
        }
    }
    return false;
}

static string joinUrl(const string &base, const string &location) {
    if (location.find("://") != string::npos) {
        return location;
    }
    if (startsWith(location.c_str(), "//")) {
        string scheme, host, path;
        int port = -1;
        if (urlParse(base.c_str(), scheme, host, port, path)) {
            return scheme + ":" + location;
        }
        return string("https:") + location;
    }

    string scheme, host, path;
    int port = -1;
    if (!urlParse(base.c_str(), scheme, host, port, path)) {
        return location;
    }

    string origin = scheme + "://" + host;
    if (port > 0 && port != 80 && port != 443) {
        origin += ":" + std::to_string(port);
    }

    if (startsWith(location.c_str(), "/")) {
        return origin + location;
    }

    string dir = path;
    auto slash = dir.rfind('/');
    if (slash != string::npos) {
        dir.resize(slash + 1);
    } else {
        dir = "/";
    }
    return origin + dir + location;
}

static int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool decodeChunkedBody(const string &in, string &out) {
    size_t pos = 0;
    out.clear();
    while (pos < in.size()) {
        size_t lineEnd = in.find("\r\n", pos);
        if (lineEnd == string::npos) {
            return false;
        }
        int size = 0;
        for (size_t i = pos; i < lineEnd; i++) {
            int v = hexValue(in[i]);
            if (v < 0) {
                if (in[i] == ';' || in[i] == ' ') {
                    break;
                }
                return false;
            }
            size = (size << 4) + v;
        }
        pos = lineEnd + 2;
        if (size == 0) {
            return true;
        }
        if (pos + (size_t)size > in.size()) {
            return false;
        }
        out.append(in.data() + pos, size);
        pos += size;
        if (pos + 1 < in.size() && in[pos] == '\r' && in[pos + 1] == '\n') {
            pos += 2;
        }
    }
    return false;
}

struct TlsSession {
    mbedtls_net_context serverFd;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctrDrbg;
    mbedtls_x509_crt cacert;

    TlsSession() {
        mbedtls_net_init(&serverFd);
        mbedtls_ssl_init(&ssl);
        mbedtls_ssl_config_init(&conf);
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&ctrDrbg);
        mbedtls_x509_crt_init(&cacert);
    }

    ~TlsSession() {
        mbedtls_ssl_close_notify(&ssl);
        mbedtls_net_free(&serverFd);
        mbedtls_x509_crt_free(&cacert);
        mbedtls_ssl_free(&ssl);
        mbedtls_ssl_config_free(&conf);
        mbedtls_ctr_drbg_free(&ctrDrbg);
        mbedtls_entropy_free(&entropy);
    }
};

static int tlsGetOnce(cstr_t url, cstr_t userAgent, cstr_t referer, int &httpCode, string &body, string &locationOut) {
    string scheme, host, path;
    int port = -1;
    if (!urlParse(url, scheme, host, port, path)) {
        return ERR_HTTP_BAD_URL;
    }
    if (strcasecmp(scheme.c_str(), "https") != 0) {
        return ERR_HTTP_BAD_URL;
    }
    if (path.empty()) {
        path = "/";
    }
    if (port <= 0) {
        port = 443;
    }

    httpCode = 0;
    body.clear();
    locationOut.clear();

    TlsSession tls;
    const char *pers = "MusicPlayerHttps";
    if (mbedtls_ctr_drbg_seed(&tls.ctrDrbg, mbedtls_entropy_func, &tls.entropy,
            (const unsigned char *)pers, strlen(pers)) != 0) {
        return ERR_FAILED;
    }

    bool hasCa = loadSystemCa(tls.cacert);
    if (mbedtls_ssl_config_defaults(&tls.conf, MBEDTLS_SSL_IS_CLIENT,
            MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
        return ERR_FAILED;
    }
    mbedtls_ssl_conf_authmode(&tls.conf, hasCa ? MBEDTLS_SSL_VERIFY_REQUIRED : MBEDTLS_SSL_VERIFY_OPTIONAL);
    if (hasCa) {
        mbedtls_ssl_conf_ca_chain(&tls.conf, &tls.cacert, nullptr);
    }
    mbedtls_ssl_conf_rng(&tls.conf, mbedtls_ctr_drbg_random, &tls.ctrDrbg);
    mbedtls_ssl_conf_read_timeout(&tls.conf, HTTPS_READ_TIMEOUT_MS);

    if (mbedtls_ssl_setup(&tls.ssl, &tls.conf) != 0) {
        return ERR_FAILED;
    }
    if (mbedtls_ssl_set_hostname(&tls.ssl, host.c_str()) != 0) {
        return ERR_FAILED;
    }

    char portBuf[16];
    snprintf(portBuf, sizeof(portBuf), "%d", port);
    if (mbedtls_net_connect(&tls.serverFd, host.c_str(), portBuf, MBEDTLS_NET_PROTO_TCP) != 0) {
        return ERR_NET_HOST_NOT_FOUND;
    }
    mbedtls_ssl_set_bio(&tls.ssl, &tls.serverFd, mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout);

    int hs;
    while ((hs = mbedtls_ssl_handshake(&tls.ssl)) != 0) {
        if (hs != MBEDTLS_ERR_SSL_WANT_READ && hs != MBEDTLS_ERR_SSL_WANT_WRITE) {
            return ERR_FAILED;
        }
    }

    string request = "GET " + path + " HTTP/1.1\r\n";
    request += "Host: " + host;
    if (port != 443) {
        request += ":" + std::to_string(port);
    }
    request += "\r\n";
    if (userAgent && userAgent[0]) {
        request += string("User-Agent: ") + userAgent + "\r\n";
    }
    if (referer && referer[0]) {
        request += string("Referer: ") + referer + "\r\n";
    }
    request += "Accept: */*\r\n";
    request += "Connection: close\r\n\r\n";

    size_t written = 0;
    while (written < request.size()) {
        int n = mbedtls_ssl_write(&tls.ssl, (const unsigned char *)request.data() + written,
            request.size() - written);
        if (n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_WANT_WRITE) {
            continue;
        }
        if (n <= 0) {
            return ERR_FAILED;
        }
        written += (size_t)n;
    }

    string raw;
    unsigned char buf[4096];
    while (raw.size() < HTTPS_MAX_BODY) {
        int n = mbedtls_ssl_read(&tls.ssl, buf, sizeof(buf));
        if (n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_WANT_WRITE) {
            continue;
        }
        if (n == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || n == 0 || n == MBEDTLS_ERR_NET_CONN_RESET) {
            break;
        }
        if (n < 0) {
            break;
        }
        raw.append((const char *)buf, n);
    }

    auto headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == string::npos) {
        return ERR_HTTP_HEAD_NOT_END;
    }
    string headers = raw.substr(0, headerEnd);
    string respBody = raw.substr(headerEnd + 4);

    if (headers.size() < 12 || !startsWith(headers.c_str(), "HTTP/")) {
        return ERR_HTTP_BAD_FORMAT;
    }
    auto sp1 = headers.find(' ');
    if (sp1 == string::npos) {
        return ERR_HTTP_BAD_FORMAT;
    }
    httpCode = atoi(headers.c_str() + sp1 + 1);

    bool chunked = false;
    string contentLength;
    size_t linePos = 0;
    while (true) {
        auto nl = headers.find("\r\n", linePos);
        if (nl == string::npos) {
            break;
        }
        string line = headers.substr(linePos, nl - linePos);
        linePos = nl + 2;
        auto colon = line.find(':');
        if (colon == string::npos) {
            continue;
        }
        string name = line.substr(0, colon);
        string value = line.substr(colon + 1);
        trimStr(name);
        trimStr(value);
        if (strcasecmp(name.c_str(), "Location") == 0) {
            locationOut = value;
        } else if (strcasecmp(name.c_str(), "Transfer-Encoding") == 0) {
            if (stristr(value.c_str(), "chunked")) {
                chunked = true;
            }
        } else if (strcasecmp(name.c_str(), "Content-Length") == 0) {
            contentLength = value;
        }
    }

    if (chunked) {
        if (!decodeChunkedBody(respBody, body)) {
            body.swap(respBody);
        }
    } else {
        body.swap(respBody);
        if (!contentLength.empty()) {
            int n = atoi(contentLength.c_str());
            if (n >= 0 && (size_t)n < body.size()) {
                body.resize((size_t)n);
            }
        }
    }

    return ERR_OK;
}

int httpGetUrl(cstr_t url, cstr_t userAgent, int &httpCode, string &body, cstr_t referer) {
    string current = url ? url : "";
    for (int i = 0; i < HTTPS_MAX_REDIRECTS; i++) {
        string location;
        int nRet = tlsGetOnce(current.c_str(), userAgent, referer, httpCode, body, location);
        if (nRet != ERR_OK) {
            return nRet;
        }
        if (httpCode >= 300 && httpCode < 400 && !location.empty()) {
            current = joinUrl(current, location);
            continue;
        }
        return ERR_OK;
    }
    return ERR_HTTP_302;
}

#endif // !_MAC_OS
