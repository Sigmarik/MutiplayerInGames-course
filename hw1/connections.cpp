#include "connections.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <stdexcept>

SocketConnection::SocketConnection(SocketConnection&& other) {
    sock_ = other.sock_;
    fail_reason_ = other.fail_reason_;
    other.fail_reason_ = "[REMOVED]";
    other.sock_ = 0;
}

SocketConnection& SocketConnection::operator=(SocketConnection&& other) {
    sock_ = other.sock_;
    fail_reason_ = other.fail_reason_;
    other.fail_reason_ = "[REMOVED]";
    other.sock_ = 0;

    return *this;
}

SocketConnection::~SocketConnection() {
    if (sock_ != 0) close(sock_);
    sock_ = 0;
}

std::string SocketConnection::getFailReason() const {
    return fail_reason_.value_or("No reason");
}

bool SocketConnection::sendString(const std::string& str) {
    if (!alive()) return false;

    uint32_t length = htonl(str.size());

    char* buffer = new char[str.size() + sizeof(length)];
    memcpy(buffer, &length, sizeof(length));
    memcpy(buffer + sizeof(length), str.c_str(), str.size());

    int status = send(sock_, buffer, str.size() + sizeof(length), MSG_NOSIGNAL);

    delete[] buffer;

    if (status < 0) {
        processErrno();
        return false;
    }

    return true;
}

static bool setSocketBlockingEnabled(int fd, bool blocking) {
    if (fd < 0) return false;

#ifdef _WIN32
    unsigned long mode = blocking ? 0 : 1;
    return (ioctlsocket(fd, FIONBIO, &mode) == 0);
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
    return (fcntl(fd, F_SETFL, flags) == 0);
#endif
}

std::optional<std::string> SocketConnection::pollString() {
    if (!alive()) return {};

    setSocketBlockingEnabled(sock_, false);

    uint32_t length = 0;
    int status = recv(sock_, &length, sizeof(length), 0);

    if (status < 0) {
        processErrno();
        setSocketBlockingEnabled(sock_, true);
        return {};
    }

    length = ntohl(length);
    char* buffer = new char[length];
    status = recv(sock_, buffer, length, 0);

    setSocketBlockingEnabled(sock_, true);

    std::string result(buffer, length);
    delete[] buffer;

    if (status < 0) {
        processErrno();
        return {};
    }

    return result;
}

std::string SocketConnection::waitForString() {
    if (!alive()) return "";

    uint32_t length = 0;
    int status = recv(sock_, &length, sizeof(length), 0);

    if (status < 0) {
        processErrno();
        return "";
    }

    length = ntohl(length);
    char* buffer = new char[length];
    status = recv(sock_, buffer, length, 0);

    std::string result(buffer, length);
    delete[] buffer;

    if (status < 0) {
        processErrno();
        return "";
    }

    return result;
}

void SocketConnection::shut() {
    fail_reason_ = "Shut down";
    if (sock_) close(sock_);
    sock_ = 0;
}

void SocketConnection::processErrno() {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
        fail_reason_ = "Connection broken";
    }
    errno = 0;
}

C2SConnection::C2SConnection(const char* ip, unsigned short port) {
    sockaddr_in serv_addr{};

    if ((sock_ = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fail_reason_ = "Socket creation error";
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        fail_reason_ = "Invalid address / Address not supported";
        return;
    }

    if (connect(sock_, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        fail_reason_ = "Connection failed";
        return;
    }

    std::string message = "i-am-a-dwarf";

    int status = send(sock_, message.c_str(), message.size(), 0);
    if (status < 0) {
        fail_reason_ = "Failed to send verification message";
        return;
    }

    char buffer[1024];
    status = read(sock_, buffer, sizeof(buffer));
    if (status < 0) {
        fail_reason_ = "Failed to retrieve verification response";
        return;
    }

    if (strncmp(buffer, "and-i-am-digging-a-hole", sizeof(buffer))) {
        fail_reason_ = "Server sent invalid verification response";
        return;
    }
}

S2CConnection::S2CConnection(int socket, const std::string& name)
    : name_(name) {
    sock_ = socket;

    // TODO: This function stalls the thread until the new client verifies
    // itself.

    char buffer[1024] = {0};
    read(sock_, buffer, 1024);

    if (strcmp(buffer, "i-am-a-dwarf")) {
        fail_reason_ = "Failed to verify the client";
        return;
    }

    const char response[] = "and-i-am-digging-a-hole";
    send(sock_, response, strlen(response), 0);
}

HostingServer::HostingServer(unsigned short port) {
    sockaddr_in address;

    if ((server_sock_ = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        fail_reason_ = "Socket creation error";
        return;
    }

    int opt = 1;
    if (setsockopt(server_sock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        fail_reason_ = "Set socket options error";
        return;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_sock_, (sockaddr*)&address, sizeof(address)) < 0) {
        fail_reason_ = "Bind failed";
        return;
    }

    if (listen(server_sock_, 10) < 0) {
        fail_reason_ = "Listen failed";
        return;
    }

    if (!setSocketBlockingEnabled(server_sock_, false)) {
        fail_reason_ = "Failed make the server socket non-blocking";
    }
}

HostingServer::HostingServer(HostingServer&& other) {
    fail_reason_ = other.fail_reason_;
    clients_ = std::move(other.clients_);
    server_sock_ = other.server_sock_;

    other.server_sock_ = 0;
    other.fail_reason_ = "[REMOVED]";
}

HostingServer& HostingServer::operator=(HostingServer&& other) {
    fail_reason_ = other.fail_reason_;
    clients_ = std::move(other.clients_);
    server_sock_ = other.server_sock_;

    other.server_sock_ = 0;
    other.fail_reason_ = "[REMOVED]";

    return *this;
}

HostingServer::~HostingServer() {
    if (server_sock_) close(server_sock_);
    server_sock_ = 0;
}

std::string HostingServer::getFailReason() const {
    return fail_reason_.value_or("No reason");
}

void HostingServer::pollNewConnections(Functor& on_connect) {
    if (!alive()) return;

    int new_socket = 0;
    sockaddr_in address;
    int addrlen = sizeof(address);

    if ((new_socket = accept(server_sock_, (sockaddr*)&address,
                             (socklen_t*)&addrlen)) < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            fail_reason_ = "Accept failed";
        }
        return;
    }

    // TODO: Temporary solution, replace with traditional name
    //       generation/retrieval
    static unsigned client_count = 0;
    ++client_count;

    std::string nickname = std::string("Client") + std::to_string(client_count);

    //! Stalls the thread, read TODO in the constructor
    S2CConnection conn(new_socket, nickname);

    if (!conn.alive()) {
        std::cerr
            << "Failed to establish connection with a new client.\n Reason: "
            << conn.getFailReason() << std::endl;
        return;
    }

    on_connect(conn);

    if (!conn.alive()) {
        std::cerr << "Failed to maintain connection with a new client after "
                     "initialization.\n Reason: "
                  << conn.getFailReason() << std::endl;
        return;
    }

    clients_.try_emplace(nickname, std::move(conn));
}

void HostingServer::forEachClient(Functor& function) {
    std::vector<decltype(clients_)::key_type> erasure_keys;

    for (auto& [name, connection] : clients_) {
        function(connection);

        if (!connection.alive()) erasure_keys.push_back(name);
    }

    for (const auto& key : erasure_keys) {
        clients_.erase(key);
    }
}
