// main.cpp — C++ 算法与 HTML 裁判之间的桥
//
// HTML 负责棋盘、规则、步数、胜负；C++ 只负责：
//   1. 创建选手实例；
//   2. /start 时调用 on_match_start；
//   3. /act 时用 HTML 传来的 Observation 调用 act()，
//      把 Action 返回给 HTML。
//
// 坐标：x = 1..3 向右，y = 1..4 向梅林里增加（North）。
// 入口坐标 (2,0)，初始朝向 North；出口坐标 (2,5)。
//
// 接口：
//   POST /start
//     {"start_x":2,"start_y":0,"start_facing":"North","max_actions":20}
//     -> {"ok":true}
//
//   POST /act
//     {"actions_taken":0,
//      "self":{"x":2,"y":0,"facing":"North","status":"Normal"},
//      "vision_front":null}   // 或 {"height":200,"kfs":"R2Kfs"}
//     -> {"type":"Move"}  或  {"type":"Turn","direction":"East"}
//
//   GET /       可视化页面（默认读取 index.html；否则内置测试页）
//
// 构建：使用 CMake + GCC（Windows 下为 MinGW g++），C++23。
// 在 TESK1 目录执行：
//   cmake -S . -B build -G "MinGW Makefiles"
//   cmake --build build
// 输出 build/referee.exe。
//
// 可选参数（在 IDE 的运行/调试配置中传入）：
//   --port 8080
//   --html your_viewer.html

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <winsock2.h>
#  include <ws2tcpip.h>
using socket_t = SOCKET;
static constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#else
#  include <arpa/inet.h>
#  include <csignal>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
using socket_t = int;
static constexpr socket_t kInvalidSocket = -1;
#endif

#include "bot.hpp"

namespace m = merlin;


static const char* dir_name(m::Direction d) {
    switch (d) {
    case m::Direction::North: return "North";
    case m::Direction::East:  return "East";
    case m::Direction::South: return "South";
    case m::Direction::West:  return "West";
    }
    return "Unknown";
}

static const char* action_name(m::Action::Type t) {
    switch (t) {
    case m::Action::Type::Move:    return "Move";
    case m::Action::Type::Turn:    return "Turn";
    case m::Action::Type::Pickup:  return "Pickup";
    case m::Action::Type::Throw:   return "Throw";
    case m::Action::Type::Recover: return "Recover";
    }
    return "Unknown";
}

static m::Direction parse_dir(const std::string& s) {
    if (s == "East"  || s == "east"  || s == "E") return m::Direction::East;
    if (s == "South" || s == "south" || s == "S") return m::Direction::South;
    if (s == "West"  || s == "west"  || s == "W") return m::Direction::West;
    return m::Direction::North;
}

static m::RobotStatus parse_status(const std::string& s) {
    if (s == "TippedOver" || s == "tipped_over" || s == "tipped") return m::RobotStatus::TippedOver;
    if (s == "Foul"       || s == "foul") return m::RobotStatus::Foul;
    return m::RobotStatus::Normal;
}

static m::KfsType parse_kfs(const std::string& s) {
    if (s == "R2Kfs" || s == "R2") return m::KfsType::R2Kfs;
    if (s == "R1Kfs" || s == "R1") return m::KfsType::R1Kfs;
    if (s == "FakeKfs" || s == "Fake") return m::KfsType::FakeKfs;
    return m::KfsType::None;
}

// 极简 JSON 取值（只处理扁平的固定字段，够这个接口用）

static std::size_t skip_ws(const std::string& s, std::size_t i) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) ++i;
    return i;
}

// 取 "key": { ... } 里花括号内部的内容；key 不存在或值是 null 时返回空串。
static std::string extract_object(const std::string& s, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    std::size_t p = s.find(needle);
    if (p == std::string::npos) return {};
    p = skip_ws(s, p + needle.size());
    if (p >= s.size() || s[p] != ':') return {};
    p = skip_ws(s, p + 1);
    if (p >= s.size() || s[p] != '{') return {};

    int depth = 0;
    bool in_str = false;
    bool esc = false;
    const std::size_t start = p;
    for (std::size_t i = p; i < s.size(); ++i) {
        const char c = s[i];
        if (in_str) {
            if (esc) esc = false;
            else if (c == '\\') esc = true;
            else if (c == '"') in_str = false;
            continue;
        }
        if (c == '"') { in_str = true; continue; }
        if (c == '{') { ++depth; continue; }
        if (c == '}') {
            --depth;
            if (depth == 0) return s.substr(start + 1, i - start - 1);
        }
    }
    return {};
}

static std::string json_string(const std::string& s, const std::string& key,
                               const std::string& def) {
    const std::string needle = "\"" + key + "\"";
    std::size_t p = s.find(needle);
    if (p == std::string::npos) return def;
    p = skip_ws(s, p + needle.size());
    if (p >= s.size() || s[p] != ':') return def;
    p = skip_ws(s, p + 1);
    if (p >= s.size() || s[p] != '"') return def;

    std::string out;
    for (++p; p < s.size(); ++p) {
        const char c = s[p];
        if (c == '\\' && p + 1 < s.size()) {
            out += s[++p];
        } else if (c == '"') {
            break;
        } else {
            out += c;
        }
    }
    return out;
}

static int json_int(const std::string& s, const std::string& key, int def) {
    const std::string needle = "\"" + key + "\"";
    std::size_t p = s.find(needle);
    if (p == std::string::npos) return def;
    p = skip_ws(s, p + needle.size());
    if (p >= s.size() || s[p] != ':') return def;
    p = skip_ws(s, p + 1);

    bool neg = false;
    if (p < s.size() && s[p] == '-') { neg = true; ++p; }
    if (p >= s.size() || s[p] < '0' || s[p] > '9') return def;

    long v = 0;
    while (p < s.size() && s[p] >= '0' && s[p] <= '9') {
        v = v * 10 + (s[p] - '0');
        if (v > 1000000) break;
        ++p;
    }
    return static_cast<int>(neg ? -v : v);
}

// Observation / MatchStart / Action 的转换
// ------------------------------------------------------------

static m::MatchStart parse_start(const std::string& body) {
    // 支持 {"start":{...}} 和扁平 {...}
    std::string obj = extract_object(body, "start");
    if (obj.empty()) obj = body;

    m::MatchStart s{};
    s.start_x = static_cast<std::int8_t>(json_int(obj, "start_x", json_int(obj, "x", 2)));
    s.start_y = static_cast<std::int8_t>(json_int(obj, "start_y", json_int(obj, "y", 0)));
    s.start_facing = parse_dir(json_string(obj, "start_facing",
                                           json_string(obj, "facing", "North")));
    s.max_actions = static_cast<std::uint8_t>(json_int(obj, "max_actions", 20));
    return s;
}

static m::Observation parse_observation(const std::string& body) {
    m::Observation obs{};

    // 支持 {"self":{...}} 和扁平 {...}
    std::string self = extract_object(body, "self");
    if (self.empty()) self = body;

    obs.self.x = static_cast<std::int8_t>(json_int(self, "x", 0));
    obs.self.y = static_cast<std::int8_t>(json_int(self, "y", 0));
    obs.self.facing = parse_dir(json_string(self, "facing", "North"));
    obs.self.status = parse_status(json_string(self, "status", "Normal"));

    int taken = json_int(body, "actions_taken", json_int(self, "actions_taken", 0));
    if (taken < 0) taken = 0;
    if (taken > 255) taken = 255;
    obs.actions_taken = static_cast<std::uint8_t>(taken);

    const std::string vf = extract_object(body, "vision_front");
    if (!vf.empty()) {
        const int h = json_int(vf, "height", 200);
        obs.vision_front = m::CellInfo{
            static_cast<std::int16_t>(h),
            parse_kfs(json_string(vf, "kfs", "None")),
        };
    }
    return obs;
}

static std::string action_json(const m::Action& a) {
    std::ostringstream o;
    o << "{\"type\":\"" << action_name(a.type) << "\"";
    // direction 只对 Turn 有意义
    if (a.type == m::Action::Type::Turn) {
        o << ",\"direction\":\"" << dir_name(a.direction) << "\"";
    }
    o << "}";
    return o.str();
}

// ------------------------------------------------------------
// 极简 HTTP 服务器
// ------------------------------------------------------------

class BridgeServer {
public:
    BridgeServer(std::uint16_t port, std::string html)
        : port_(port), html_(std::move(html)) {}

    ~BridgeServer() {
        if (player_) player_.reset();
#ifdef _WIN32
        if (wsa_) WSACleanup();
#endif
    }

    bool start();
    void run();

private:
    static void close_socket(socket_t s) {
#ifdef _WIN32
        ::closesocket(s);
#else
        ::close(s);
#endif
    }

    static bool send_all(socket_t s, const std::string& data) {
        std::size_t sent = 0;
        while (sent < data.size()) {
            const std::size_t left = data.size() - sent;
#ifdef _WIN32
            const int n = ::send(s, data.data() + sent, static_cast<int>(left), 0);
#else
            const ssize_t n = ::send(s, data.data() + sent, left, 0);
#endif
            if (n <= 0) return false;
            sent += static_cast<std::size_t>(n);
        }
        return true;
    }

    static void send_response(socket_t c, const char* status,
                              const std::string& content_type,
                              const std::string& body) {
        std::ostringstream o;
        o << "HTTP/1.1 " << status << "\r\n"
          << "Content-Type: " << content_type << "\r\n"
          << "Content-Length: " << body.size() << "\r\n"
          << "Access-Control-Allow-Origin: *\r\n"
          << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
          << "Access-Control-Allow-Headers: Content-Type\r\n"
          << "Connection: close\r\n\r\n"
          << body;
        send_all(c, o.str());
    }

    static std::string to_lower(std::string s) {
        for (char& c : s) {
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        }
        return s;
    }

    bool read_request(socket_t c, std::string& method, std::string& path, std::string& body);
    void handle_client(socket_t c);
    void handle_start(socket_t c, const std::string& body);
    void handle_act(socket_t c, const std::string& body);

    std::uint16_t port_;
    std::string html_;
    socket_t listen_ = kInvalidSocket;
    std::unique_ptr<m::Player> player_;

#ifdef _WIN32
    bool wsa_ = false;
#endif
};

bool BridgeServer::start() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n";
        return false;
    }
    wsa_ = true;
#else
    std::signal(SIGPIPE, SIG_IGN);
#endif

    // 创建选手实例（/start 会再重新创建一个，保证每场比赛状态干净）
    try {
        player_.reset(create_player());
    } catch (...) {
        std::cerr << "create_player() threw\n";
        return false;
    }
    if (!player_) {
        std::cerr << "create_player() returned null\n";
        return false;
    }

    listen_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_ == kInvalidSocket) {
        std::cerr << "socket() failed\n";
        return false;
    }

    const int yes = 1;
    ::setsockopt(listen_, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&yes), sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);

    if (::bind(listen_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::cerr << "bind() failed on port " << port_ << "\n";
        close_socket(listen_);
        listen_ = kInvalidSocket;
        return false;
    }
    if (::listen(listen_, SOMAXCONN) != 0) {
        std::cerr << "listen() failed\n";
        close_socket(listen_);
        listen_ = kInvalidSocket;
        return false;
    }
    return true;
}

bool BridgeServer::read_request(socket_t c, std::string& method, std::string& path,
                                std::string& body) {
    std::string buf;
    char tmp[4096];
    std::size_t header_end = std::string::npos;

    while (header_end == std::string::npos && buf.size() < (1u << 20)) {
        const int n = static_cast<int>(::recv(c, tmp, sizeof(tmp), 0));
        if (n <= 0) return false;
        buf.append(tmp, static_cast<std::size_t>(n));
        header_end = buf.find("\r\n\r\n");
    }
    if (header_end == std::string::npos) return false;

    const std::string head = buf.substr(0, header_end);
    body = buf.substr(header_end + 4);

    std::istringstream first(head);
    std::string version;
    first >> method >> path >> version;
    if (path.empty()) path = "/";
    const std::size_t q = path.find('?');
    if (q != std::string::npos) path = path.substr(0, q);

    // Content-Length
    std::size_t content_length = 0;
    const std::string lower = to_lower(head);
    const std::size_t cl = lower.find("content-length:");
    if (cl != std::string::npos) {
        std::size_t p = cl + std::strlen("content-length:");
        p = skip_ws(head, p);
        while (p < head.size() && head[p] >= '0' && head[p] <= '9') {
            content_length = content_length * 10 + static_cast<std::size_t>(head[p] - '0');
            ++p;
        }
    }

    while (body.size() < content_length) {
        const int n = static_cast<int>(::recv(c, tmp, sizeof(tmp), 0));
        if (n <= 0) break;
        body.append(tmp, static_cast<std::size_t>(n));
    }
    return true;
}

void BridgeServer::handle_start(socket_t c, const std::string& body) {
    const m::MatchStart s = parse_start(body);
    try {
        player_.reset(create_player());
        if (!player_) {
            send_response(c, "500 Internal Server Error", "application/json; charset=utf-8",
                          "{\"error\":\"create_player() returned null\"}");
            return;
        }
        player_->on_match_start(s);
    } catch (...) {
        send_response(c, "500 Internal Server Error", "application/json; charset=utf-8",
                      "{\"error\":\"on_match_start() threw\"}");
        return;
    }

    std::cout << "on_match_start: (" << static_cast<int>(s.start_x) << ","
              << static_cast<int>(s.start_y) << ") facing " << dir_name(s.start_facing)
              << " max_actions=" << static_cast<int>(s.max_actions) << "\n";
    send_response(c, "200 OK", "application/json; charset=utf-8", "{\"ok\":true}");
}

void BridgeServer::handle_act(socket_t c, const std::string& body) {
    if (!player_) {
        send_response(c, "500 Internal Server Error", "application/json; charset=utf-8",
                      "{\"error\":\"player not created\"}");
        return;
    }

    const m::Observation obs = parse_observation(body);
    m::Action action{};
    try {
        action = player_->act(obs);
    } catch (...) {
        send_response(c, "500 Internal Server Error", "application/json; charset=utf-8",
                      "{\"error\":\"act() threw\"}");
        return;
    }

    std::cout << "act (" << static_cast<int>(obs.actions_taken) << ") -> "
              << action_name(action.type) << "\n";
    send_response(c, "200 OK", "application/json; charset=utf-8", action_json(action));
}

void BridgeServer::handle_client(socket_t c) {
    // 防止不说话的客户端卡住
#ifdef _WIN32
    DWORD rcv_timeout = 5000;
    ::setsockopt(c, SOL_SOCKET, SO_RCVTIMEO,
                 reinterpret_cast<const char*>(&rcv_timeout), sizeof(rcv_timeout));
#else
    timeval rcv_timeout{};
    rcv_timeout.tv_sec = 5;
    ::setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &rcv_timeout, sizeof(rcv_timeout));
#endif

    std::string method, path, body;
    if (!read_request(c, method, path, body)) {
        close_socket(c);
        return;
    }

    if (method == "OPTIONS") {
        send_response(c, "204 No Content", "text/plain", "");
    } else if (method == "GET" && (path == "/" || path == "/index.html")) {
        send_response(c, "200 OK", "text/html; charset=utf-8", html_);
    } else if (method == "GET" && path == "/health") {
        send_response(c, "200 OK", "application/json; charset=utf-8", "{\"ok\":true}");
    } else if (method == "POST" && path == "/start") {
        handle_start(c, body);
    } else if (method == "POST" && path == "/act") {
        handle_act(c, body);
    } else {
        send_response(c, "404 Not Found", "application/json; charset=utf-8",
                      "{\"error\":\"not found\"}");
    }
    close_socket(c);
}

void BridgeServer::run() {
    while (true) {
        sockaddr_in cli{};
#ifdef _WIN32
        int len = static_cast<int>(sizeof(cli));
#else
        socklen_t len = static_cast<socklen_t>(sizeof(cli));
#endif
        socket_t c = ::accept(listen_, reinterpret_cast<sockaddr*>(&cli), &len);
        if (c == kInvalidSocket) continue;
        handle_client(c);
    }
}

// ------------------------------------------------------------
// 内置测试页（不是裁判；正式可视化用 --html 指定）
// ------------------------------------------------------------

static const char* kFallbackHtml = R"HTML(<!doctype html>
<html lang="zh-CN">
<meta charset="utf-8">
<title>Merlin C++ Bridge</title>
<style>
  body{font-family:system-ui,sans-serif;margin:24px;background:#0f1115;color:#e6e6e6}
  button{margin-right:8px;padding:6px 12px}
  pre{background:#171a21;padding:12px;border-radius:8px;overflow:auto}
</style>
<h1>Merlin C++ Bridge</h1>
<p>HTML 负责裁判逻辑；这里只有 <code>POST /start</code> 和 <code>POST /act</code>。</p>
<button onclick="startMatch()">POST /start</button>
<button onclick="step()">POST /act</button>
<pre id="out"></pre>
<script>
const out = document.getElementById('out');
async function post(url, body) {
  const r = await fetch(url, {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(body)
  });
  const j = await r.json();
  out.textContent += url + ' -> ' + JSON.stringify(j) + '\n';
}
function startMatch() {
  post('/start', {start_x: 2, start_y: 0, start_facing: 'North', max_actions: 20});
}
function step() {
  post('/act', {
    actions_taken: 0,
    self: {x: 2, y: 0, facing: 'North', status: 'Normal'},
    vision_front: null
  });
}
</script>
</html>
)HTML";

// ------------------------------------------------------------
// main
// ------------------------------------------------------------

static void print_usage(const char* prog) {
    std::cout << "用法: " << prog << " [--port 8080] [--html viewer.html]\n";
}

static bool load_file(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

static bool file_exists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

static std::string executable_directory(const char* argv0) {
    std::error_code ec;
    std::filesystem::path exe = std::filesystem::absolute(argv0, ec);
    if (ec) return {};
    exe = std::filesystem::weakly_canonical(exe, ec);
    if (ec) return {};
    return exe.parent_path().string();
}

// 依次查找：当前工作目录、可执行文件所在目录、源码目录。
// 这样在 IDE 中直接运行/调试，或在 build 目录中运行，都能加载 index.html。
static bool load_index_html(const char* argv0, std::string& html) {
    if (file_exists("index.html") && load_file("index.html", html)) return true;

    const std::string dir = executable_directory(argv0);
    if (!dir.empty()) {
        const std::string candidate = dir + "/index.html";
        if (file_exists(candidate) && load_file(candidate, html)) return true;
    }

#ifdef MERLIN_SOURCE_DIR
    const std::string source_candidate = std::string(MERLIN_SOURCE_DIR) + "/index.html";
    if (file_exists(source_candidate) && load_file(source_candidate, html)) return true;
#endif

    return false;
}

int main(int argc, char** argv) {
    std::uint16_t port = 8080;
    std::string html_path;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = static_cast<std::uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--html" && i + 1 < argc) {
            html_path = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            std::cerr << "未知参数: " << arg << "\n";
            print_usage(argv[0]);
            return 2;
        }
    }

    std::string html;
    if (!html_path.empty()) {
        if (!load_file(html_path, html)) {
            std::cerr << "无法读取 " << html_path << "，改用内置页面\n";
        }
    }
    if (html.empty()) {
        load_index_html(argv[0], html);
    }
    if (html.empty()) html = kFallbackHtml;

    BridgeServer server(port, html);
    if (!server.start()) return 1;

    std::cout << "桥接服务已启动: http://localhost:" << port << "/\n"
              << "  POST /start , POST /act , GET /health\n";
    server.run();
    return 0;
}
