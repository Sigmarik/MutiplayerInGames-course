#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

#include "connections.h"

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage:\n";
        std::cerr << argv[0] << " [SERVER_IP] [PORT]" << std::endl;
        return EXIT_FAILURE;
    }

    const char* ip = argv[1];
    short unsigned port = atoi(argv[2]);

    // Fool protection?
    // Nah, that's not what this course is about.

    std::cout << "IP: " << ip << ", port: " << port << std::endl;

    pid_t proc = fork();

    if (proc == -1) {
        std::cerr << "Failed to switch" << std::endl;
    }

    C2SConnection client(ip, port);

    if (!client.alive()) {
        std::cerr << "Client connection failed to initialize" << std::endl;
        return EXIT_FAILURE;
    }

    if (proc == 0) {
        client.sendString("Do we need to act out a natural conversation?");
        client.waitForString();
        client.sendString(
            "Don't you think we have to verify each other's messages?");
        client.waitForString();
        client.sendString(
            "TL, DR. What will happen if I say two things at once?");
        client.sendString("Will you be able to catch what is going on?");
        client.waitForString();
        std::string msg = client.waitForString();
        if (msg != "VERIFIABLE") {
            std::cerr << "A client has received an unverifiable string \""
                      << msg << "\", shutting down." << std::endl;
            client.sendString("Traitor.");
            return EXIT_FAILURE;
        }

        client.sendString("Damn, that string sure is verifiable.");
        client.waitForString();
        client.sendString("\\complete");
    } else {
        client.waitForString();
        client.sendString("Yup. I definitely think so.");
        client.waitForString();
        client.sendString(
            "What, you think the Higher Beings won't believe our creator does "
            "not know how to compare strings? Or that they will think he needs "
            "a slap on a wrist to learn of the importance of good coding "
            "practices?");
        client.waitForString();
        client.waitForString();
        client.sendString(
            "Yes, I will. And I'll send a verifiable string in a similar way.");
        client.sendString("VERIFIABLE");
        client.waitForString();
        client.sendString("\\start-quest");
        client.waitForString();
        client.waitForString();
        client.sendString("\\complete");
    }

    return EXIT_SUCCESS;
}
