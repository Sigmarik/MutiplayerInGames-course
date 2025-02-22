#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <deque>
#include <iostream>

#include "connections.h"

struct UserMessage {
    std::string user = "Anonymous";
    std::string content = "[NO CONTENT]";
};

struct ServerStatus {
    std::deque<UserMessage> message_queue{};
    std::optional<std::string> quest_winner{};
    bool has_active_quest = false;
};

static const std::string SERVER_NAME = "[SERVER]";

void pullClientMessages(ServerStatus &server, S2CConnection &client) {
    auto message = client.pollString();

    if (!client.alive()) {
        std::cout << client.getName() << " disconnected" << std::endl;
    }

    if (message && message->size()) {
        UserMessage usr_msg;

        // There could have been an equation generator with the ability to parse
        // user's answers and stuff, but I am too lazy to implement it.
        // The network part of things seems fine, though, and that's what
        // this course is about, eh?
        if (message == "\\start-quest") {
            server.has_active_quest = true;
            server.quest_winner = {};

            usr_msg.user = SERVER_NAME;
            usr_msg.content =
                client.getName() +
                " started a quest. Type \"\\complete\" to complete it!";
        } else if (message == "\\complete") {
            usr_msg.user = SERVER_NAME;
            if (server.has_active_quest && !server.quest_winner) {
                server.quest_winner = client.getName();
                usr_msg.content =
                    *server.quest_winner + " completed the quest first!";
            } else if (server.quest_winner) {
                usr_msg.content =
                    client.getName() + " completed a quest, though " +
                    *server.quest_winner + " has completed it before them!";
            } else {
                usr_msg.content = client.getName() +
                                  " tried to complete a non-existent quest!";
            }
        } else {
            usr_msg.user = client.getName();
            usr_msg.content = *message;
        }

        server.message_queue.push_back(usr_msg);

        std::cout << usr_msg.user << ": " << usr_msg.content << std::endl;
    }
}

void pushClientResponses(ServerStatus &server, S2CConnection &client) {
    for (UserMessage &msg : server.message_queue) {
        if (msg.user == client.getName()) continue;

        client.sendString(msg.content);

        if (!client.alive()) {
            std::cout << client.getName() << " disconnected" << std::endl;
            break;
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage:\n";
        std::cerr << argv[0] << " [PORT]" << std::endl;
        return EXIT_FAILURE;
    }

    short unsigned port = atoi(argv[1]);

    HostingServer server(port);

    if (!server.alive()) {
        std::cerr << "Server failed to initialize. Reason: "
                  << server.getFailReason() << std::endl;
        return EXIT_FAILURE;
    }

    HostingServer::Functor on_client_join = [](S2CConnection &) {
        std::cout << "A new client has connected" << std::endl;
    };

    ServerStatus game_server;

    // Looks an awful lot like a round-robin scheduler, doesn't it?

    HostingServer::
        Functor client_pull_tick = [&game_server](S2CConnection &client) {
            pullClientMessages(game_server, client);
        };
    HostingServer::
        Functor client_response_tick = [&game_server](S2CConnection &client) {
            pushClientResponses(game_server, client);
        };

    bool going = true;
    while (going) {
        server.pollNewConnections(on_client_join);
        server.forEachClient(client_pull_tick);
        server.forEachClient(client_response_tick);
        game_server.message_queue.clear();
    }

    return EXIT_SUCCESS;
}
