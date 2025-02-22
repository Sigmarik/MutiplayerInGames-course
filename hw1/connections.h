#pragma once

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <functional>
#include <iostream>
#include <optional>
#include <unordered_map>

struct SocketConnection {
    SocketConnection(const SocketConnection&) = delete;
    SocketConnection& operator=(const SocketConnection&) = delete;

    SocketConnection(SocketConnection&&);
    SocketConnection& operator=(SocketConnection&&);

    ~SocketConnection();

    bool alive() const { return !fail_reason_; }
    std::string getFailReason() const;

    bool sendString(const std::string& str);
    std::optional<std::string> pollString();
    std::string waitForString();

    void shut();

   protected:
    SocketConnection() = default;

    int sock_ = 0;
    std::optional<std::string> fail_reason_{};

   private:
    void processErrno();
};

struct C2SConnection : public SocketConnection {
    C2SConnection(const char* ip, unsigned short port);
};

struct S2CConnection : public SocketConnection {
    friend struct HostingServer;

    std::string getName() { return name_; }

   private:
    S2CConnection(int socket, const std::string& name);

    std::string name_;
};

struct HostingServer {
    HostingServer(unsigned short port);

    HostingServer(const HostingServer&) = delete;
    HostingServer& operator=(const HostingServer&) = delete;

    HostingServer(HostingServer&&);
    HostingServer& operator=(HostingServer&&);

    ~HostingServer();

    bool alive() const { return !fail_reason_; }
    std::string getFailReason() const;

    using Functor = std::function<void(S2CConnection&)>;

    void pollNewConnections(Functor& on_connect);
    void forEachClient(Functor& function);

    std::optional<S2CConnection> getClientConnection(const std::string& id);

   private:
    std::optional<std::string> fail_reason_;

    std::unordered_map<std::string, S2CConnection> clients_{};

    int server_sock_ = 0;
};
