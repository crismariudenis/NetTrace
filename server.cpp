#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <iostream>
#include <thread>
#include <vector>
#include <format>
#include <sstream>
#include "trace.h"
#define PORT 6969
#define MAX_SIZE 4096

#define handle_error(msg, err) \
    {                          \
        perror(msg);           \
        exit(err);             \
    }

void handle_client(int socket)
{
    char msg[MAX_SIZE];
    char res[MAX_SIZE] = " ";
    memset(msg, 0, sizeof(msg));

    //______________________HEADER STUFF______________________
    auto myid = std::this_thread::get_id();
    std::stringstream ss;
    ss << myid;
    std::string id = ss.str();
    std::string header = std::format("[server.thread: {}]", id);
    //________________________________________________________

    std::cout << header << " connected to client\n";

    while (true)
    {
        std::cout << header << " waiting for a message ...\n";
        if (read(socket, &msg, sizeof(msg)) < 0)
        {
            perror("[server]Error at read()\n");
            close(socket);
            return;
        }

        std::string message(msg);
        if (message == "q")
        {
            std::cout << header << " received quit command\n";
            break;
        }
        if (message == "trace help")
        {
            std::cout << header << " received commanad: trace -h\n";
            strcpy(res, "\nd       switching display mode\nr       reset all counters");
        }
        else if (message.find("trace ") == 0)
        {
            std::string arg = message.substr(6);
            size_t pos = arg.find(' ');

            std::string path = arg.substr(0, pos);

            std::string flags = (pos != std::string::npos) ? arg.substr(pos + 1) : "";

            std::cout << std::format("{} received command: trace <{}> <{}>\n", header, path, flags);
            strcpy(res, "Not implemented yet. :) Maybe try mtr ");
            strcat(res, arg.c_str());
        }
        else
        {
            std::cout << header << " received unknown command: " << message << "\n";
            strcpy(res, "received unknown command");
        }

        if (write(socket, res, MAX_SIZE) < 0)
        {
            perror("[server]Error at write()\n");
            close(socket);
            return;
        }
    }
    std::cout << header << " closing connection\n";
    close(socket);
}

int main(int argc, char const *argv[])
{
    struct sockaddr_in server, from;
    int sd;

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[server]Error at creating the socket\n");
        return errno;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    if (bind(sd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("[server]Error at bind()\n");
    }

    if (listen(sd, 5) == -1)
    {
        perror("[server]Error at listen().\n");
        return errno;
    }

    std::vector<std::thread> threads;
    while (1)
    {
        std::cout << "[server]Waiting at port " << PORT << " ..." << std::endl;
        fflush(stdout);

        int client;
        socklen_t len = sizeof(from);
        if ((client = accept(sd, (struct sockaddr *)&from, &len)) < 0)
        {
            perror("[server]Error at accept()\n");
            continue;
        }

        threads.emplace_back(std::thread(handle_client, client));
    }

    for (auto &t : threads)
    {
        if (t.joinable())
        {
            t.join();
        }
    }
    close(sd);

    return 0;
}