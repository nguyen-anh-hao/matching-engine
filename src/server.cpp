#include "server.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using sock_t = SOCKET;
#define BAD_SOCKET INVALID_SOCKET
#define SOCK_ERR SOCKET_ERROR
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using sock_t = int;
#define BAD_SOCKET (-1)
#define SOCK_ERR (-1)
#define closesocket close
#endif

namespace exchange {

namespace {

std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out += c; break;
        }
    }
    return out;
}

// Parse a fixed-point decimal string ("123.456") into an int64 raw value.
Price parse_fixed(const std::string& s) {
    char* end = nullptr;
    long double v = strtold(s.c_str(), &end);
    return static_cast<Price>(std::llround(static_cast<double>(v) * static_cast<double>(kScale)));
}

// Format an int64 fixed-point raw into "123.456" (trailing zeros trimmed).
std::string fmt_fixed(Price p) {
    bool neg = p < 0;
    uint64_t abs = neg ? static_cast<uint64_t>(-(p + 1)) + 1 : static_cast<uint64_t>(p);
    uint64_t whole = abs / static_cast<uint64_t>(kScale);
    uint64_t frac = abs % static_cast<uint64_t>(kScale);
    std::string s = std::to_string(whole);
    if (frac != 0) {
        std::string f = std::to_string(frac);
        f = std::string(8 - f.size(), '0') + f;
        while (!f.empty() && f.back() == '0') f.pop_back();
        s += "." + f;
    }
    return neg ? ("-" + s) : s;
}

// Parse a flat JSON object { "key": value, ... } where values are string,
// number or boolean scalars. Returns a map of key -> raw token.
std::map<std::string, std::string> parse_json_object(const std::string& body) {
    std::map<std::string, std::string> out;
    size_t i = body.find('{');
    if (i == std::string::npos) return out;

    auto skip_ws = [&](size_t& p) {
        while (p < body.size() && std::isspace(static_cast<unsigned char>(body[p]))) ++p;
    };

    for (++i;;) {
        skip_ws(i);
        if (i >= body.size() || body[i] == '}') break;
        if (body[i] != '"') return out;
        size_t ks = ++i;
        while (i < body.size() && body[i] != '"') ++i;
        std::string key = body.substr(ks, i - ks);
        ++i;
        skip_ws(i);
        if (i >= body.size() || body[i] != ':') return out;
        ++i;
        skip_ws(i);
        if (i >= body.size()) return out;

        std::string value;
        if (body[i] == '"') {
            size_t vs = ++i;
            while (i < body.size() && body[i] != '"') ++i;
            value = body.substr(vs, i - vs);
            ++i;
        } else {
            size_t vs = i;
            while (i < body.size() && body[i] != ',' && body[i] != '}') ++i;
            value = body.substr(vs, i - vs);
            size_t a = value.find_first_not_of(" \t\r\n");
            size_t b = value.find_last_not_of(" \t\r\n");
            value = (a == std::string::npos) ? "" : value.substr(a, b - a + 1);
        }
        out[key] = value;
        skip_ws(i);
        if (i < body.size() && body[i] == ',') ++i;
    }
    return out;
}

std::string query_param(const std::string& qs, const std::string& key) {
    std::string needle = key + "=";
    size_t p = qs.find(needle);
    if (p == std::string::npos) return "";
    size_t s = p + needle.size();
    size_t e = qs.find('&', s);
    return qs.substr(s, e == std::string::npos ? std::string::npos : e - s);
}

bool send_all(sock_t s, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        int n = static_cast<int>(::send(s, data.data() + sent, static_cast<int>(data.size() - sent), 0));
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

void http_response(sock_t s, int code, const std::string& body) {
    std::ostringstream h;
    h << "HTTP/1.1 " << code << " " << (code == 200 ? "OK" : "Bad Request") << "\r\n"
      << "Content-Type: application/json\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "Connection: close\r\n"
      << "\r\n";
    send_all(s, h.str());
    send_all(s, body);
}

} // namespace

void HttpServer::run() {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    sock_t listen_sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == BAD_SOCKET) return;
    listen_fd_ = static_cast<intptr_t>(listen_sock);

    int yes = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&yes), sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);
    if (::bind(listen_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCK_ERR) {
        closesocket(listen_sock);
        return;
    }
    if (::listen(listen_sock, 1024) == SOCK_ERR) {
        closesocket(listen_sock);
        return;
    }

    while (running_) {
        sockaddr_in client{};
#ifdef _WIN32
        int clen = sizeof(client);
#else
        socklen_t clen = sizeof(client);
#endif
        sock_t csock = ::accept(listen_sock, reinterpret_cast<sockaddr*>(&client), &clen);
        if (csock == BAD_SOCKET) continue;

        std::thread([this, csock]() {
        std::string req;
        char buf[4096];
        int n = 0;
        do {
            n = static_cast<int>(::recv(csock, buf, sizeof(buf), 0));
            if (n > 0) req.append(buf, static_cast<size_t>(n));
        } while (n > 0 && req.find("\r\n\r\n") == std::string::npos);

        size_t header_end = req.find("\r\n\r\n");
        std::string headers = header_end == std::string::npos ? req : req.substr(0, header_end);
        std::string body = header_end == std::string::npos ? "" : req.substr(header_end + 4);

        size_t content_len = 0;
        {
            std::string lower = headers;
            for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            std::string needle = "content-length:";
            size_t cp = lower.find(needle);
            if (cp != std::string::npos) {
                size_t vs = cp + needle.size();
                while (vs < lower.size() && std::isspace(static_cast<unsigned char>(lower[vs]))) ++vs;
                content_len = static_cast<size_t>(std::strtoull(lower.c_str() + vs, nullptr, 10));
            }
        }
        while (body.size() < content_len) {
            n = static_cast<int>(::recv(csock, buf, sizeof(buf), 0));
            if (n <= 0) break;
            body.append(buf, static_cast<size_t>(n));
        }

        std::istringstream hl(headers.substr(0, headers.find("\r\n")));
        std::string method, path, proto;
        hl >> method >> path >> proto;

        std::string qs;
        size_t qpos = path.find('?');
        if (qpos != std::string::npos) {
            qs = path.substr(qpos + 1);
            path = path.substr(0, qpos);
        }

        std::string out;
        int code = 200;

        if (path == "/health") {
            out = "{\"status\":\"ok\"}";
        } else if (path == "/orders" && method == "POST") {
            auto obj = parse_json_object(body);
            OrderRequest oreq;
            oreq.symbol = obj["symbol"];
            std::string side = obj["side"];
            std::string type = obj["type"];
            std::string tif = obj["time_in_force"];
            oreq.side = (side == "SELL") ? Side::Sell : Side::Buy;
            oreq.type = (type == "MARKET") ? OrderType::Market : OrderType::Limit;
            oreq.tif = (tif == "IOC") ? TimeInForce::IOC
                       : (tif == "FOK") ? TimeInForce::FOK
                                        : TimeInForce::GTC;
            oreq.price = parse_fixed(obj["price"]);
            oreq.quantity = parse_fixed(obj["quantity"]);
            oreq.account_id = static_cast<AccountID>(std::strtoull(obj["account_id"].c_str(), nullptr, 10));
            oreq.client_order_id = obj["client_order_id"];

            PlaceResult r;
            {
                std::lock_guard<std::mutex> lock(engine_mutex_);
                r = engine_.place_order(oreq);
            }
            out = "{\"accepted\":" + std::string(r.accepted ? "true" : "false") +
                  ",\"order_id\":" + std::to_string(r.order_id) +
                  ",\"status\":\"" + to_string(r.status) + "\"";
            if (!r.reject_reason.empty()) out += ",\"reject_reason\":\"" + json_escape(r.reject_reason) + "\"";
            out += ",\"trades\":[";
            for (size_t i = 0; i < r.trades.size(); ++i) {
                if (i) out += ",";
                const Trade& t = r.trades[i];
                out += "{\"trade_id\":" + std::to_string(t.id) +
                       ",\"symbol\":\"" + json_escape(t.symbol) +
                       "\",\"price\":\"" + fmt_fixed(t.price) +
                       "\",\"quantity\":\"" + fmt_fixed(t.quantity) +
                       "\",\"maker_order_id\":" + std::to_string(t.maker_order_id) +
                       ",\"taker_order_id\":" + std::to_string(t.taker_order_id) +
                       ",\"maker_account_id\":" + std::to_string(t.maker_account_id) +
                       ",\"taker_account_id\":" + std::to_string(t.taker_account_id) + "}";
            }
            out += "]}";
        } else if (path == "/orders" && method == "DELETE") {
            OrderID id = static_cast<OrderID>(std::strtoull(query_param(qs, "id").c_str(), nullptr, 10));
            std::string sym = query_param(qs, "symbol");
            bool ok;
            {
                std::lock_guard<std::mutex> lock(engine_mutex_);
                ok = engine_.cancel_order(id, sym);
            }
            out = ok ? "{\"success\":true}" : "{\"success\":false}";
        } else if (path == "/book" && method == "GET") {
            std::string sym = query_param(qs, "symbol");
            uint32_t depth = static_cast<uint32_t>(std::strtoul(query_param(qs, "depth").c_str(), nullptr, 10));
            OrderBookSnapshot snap;
            {
                std::lock_guard<std::mutex> lock(engine_mutex_);
                snap = engine_.snapshot(sym, depth);
            }
            out = "{\"symbol\":\"" + json_escape(sym) + "\",\"bids\":[";
            for (size_t i = 0; i < snap.bids.size(); ++i) {
                if (i) out += ",";
                out += "[\"" + fmt_fixed(snap.bids[i].price) + "\",\"" + fmt_fixed(snap.bids[i].quantity) + "\"]";
            }
            out += "],\"asks\":[";
            for (size_t i = 0; i < snap.asks.size(); ++i) {
                if (i) out += ",";
                out += "[\"" + fmt_fixed(snap.asks[i].price) + "\",\"" + fmt_fixed(snap.asks[i].quantity) + "\"]";
            }
            out += "]}";
        } else if (path == "/ticker" && method == "GET") {
            std::string sym = query_param(qs, "symbol");
            Ticker t;
            {
                std::lock_guard<std::mutex> lock(engine_mutex_);
                t = engine_.ticker(sym);
            }
            out = "{\"symbol\":\"" + json_escape(sym) +
                  "\",\"last_price\":\"" + fmt_fixed(t.last_price) +
                  "\",\"best_bid\":\"" + fmt_fixed(t.best_bid) +
                  "\",\"best_ask\":\"" + fmt_fixed(t.best_ask) + "\"}";
        } else {
            code = 404;
            out = "{\"error\":\"not_found\"}";
        }

        http_response(csock, code, out);
        closesocket(csock);
        }).detach();
    }

    closesocket(listen_sock);
#ifdef _WIN32
    WSACleanup();
#endif
}

} // namespace exchange


