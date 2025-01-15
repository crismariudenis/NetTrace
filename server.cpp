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
#include <map>
#include "trace.h"
#include "fcntl.h"
#define PORT 6969
#define MAX_SIZE 4096

#define handle_error(msg, err) \
    {                          \
        perror(msg);           \
        exit(err);             \
    }

std::map<std::string, std::string> parse_flags(const std::string &flags)
{
    std::istringstream iss(flags);
    std::string token;
    std::string lastFlag;
    std::map<std::string, std::string> parsedFlags;

    while (iss >> token)
    {
        if (token[0] == '-')
        {
            if (token.size() > 1)
            {
                lastFlag = token;
                parsedFlags[lastFlag] = "";
            }
        }
        else if (!lastFlag.empty())
        {
            parsedFlags[lastFlag] = token;
            lastFlag.clear();
        }
    }
    return parsedFlags;
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

    int flags = fcntl(socket, F_GETFL, 0);
    fcntl(socket, F_SETFL, flags | O_NONBLOCK);

    while (true)
    {
        memset(msg, 0, sizeof(msg));
        ssize_t bytes_read = read(socket, &msg, sizeof(msg));
        if (bytes_read < 0)
        {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                continue;
            }
            else
            {
                perror("[server]Error at read()\n");
                close(socket);
                return;
            }
        }

        std::string message(msg);
        if (message == "q")
        {
            std::cout << header << " received quit command\n";
            break;
        }
        if (message == "trace -h")
        {
            std::cout << header << " received commanad: trace -h\n";
            strncpy(res, "Usage:\ntrace hostname [flags]\n\nKeyBindings:\n-h              help\n-q              quits\n-s              stop trace\n-p              pause/unpause trace\nFlags:\n-d [duration]   delay between traces in seconds\n-m [number]     max number of hops\n", MAX_SIZE - 1);
            res[MAX_SIZE - 1] = '\0';
        }
        else if (message.find("trace ") == 0)
        {
            std::string arg = message.substr(6);
            size_t pos = arg.find(' ');

            std::string path = arg.substr(0, pos);

            std::string flags = (pos != std::string::npos) ? arg.substr(pos + 1) : "";

            std::cout << std::format("{} received command: trace <{}> <{}>\n", header, path, flags);

            std::map<std::string, std::string> flg = parse_flags(flags);

            // strncpy(res, "Starting trace...\n", MAX_SIZE - 1);
            // res[MAX_SIZE - 1] = '\0';
            if (write(socket, res, MAX_SIZE) < 0)
            {
                perror("[server]Error at write()\n");
                close(socket);
                return;
            }
            bool stopped = false;
            bool paused = false;
            bool hdr = true;
            int sleep_time = 0;
            Tracer tracer{path};
            for (auto &f : flg)
            {
                if (f.first == "-d")
                    sleep_time = atoi(f.second.c_str());
                if (f.first == "-m")
                    tracer.MAX_HOPS = atoi(f.second.c_str());
            }
            while (!stopped)
            {
                if (!paused)
                {
                    tracer.run();
                    std::vector<line> log_lines = log(tracer);
                    std::ostringstream oss;

                    std::vector<int> px = {24, 4, 5, 8, 7, 7};
                    std::vector<std::string> names = {"Host", "Loss%", "Snt", "Avg", "Best", "Wrst", "StDev"};
                    for (int i = 0; i < names.size(); i++)
                    {
                        oss << names[i] << std::string(px[i], ' ');
                    }
                    oss << '\n';

                    for (int i = 0; i < log_lines.size(); i++)
                    {
                        const auto &line = log_lines[i];
                        oss << std::fixed << std::setprecision(2);

                        if (i < 10)
                            oss << " ";
                        oss << i << ". ";
                        if (line.ip.empty())
                        {
                            oss << "(waiting for reply)\n";
                        }
                        else
                        {
                            auto &h = line;
                            int ip_length = h.ip.size();
                            int loss_spacing = 25 - ip_length;
                            oss << std::fixed << std::setprecision(2);
                            oss << h.ip
                                << std::string(std::max(0, loss_spacing), ' ') << h.loss
                                << std::string(5, ' ') << (int)h.snt
                                << std::string(std::max(0, 15 - (int)std::to_string(h.avg).size()), ' ') << h.avg
                                << std::string(std::max(0, 15 - (int)std::to_string(h.best).size()), ' ') << h.best
                                << std::string(std::max(0, 15 - (int)std::to_string(h.wrst).size()), ' ') << h.wrst
                                << std::string(std::max(0, 15 - (int)std::to_string(h.stdev).size()), ' ') << h.stdev
                                << '\n';
                        }
                    }
                    std::string log_str = oss.str();
                    strncpy(res, log_str.c_str(), MAX_SIZE - 1);
                    res[MAX_SIZE - 1] = '\0';
                    if (write(socket, res, MAX_SIZE) < 0)
                    {
                        perror("[server]Error at write()\n");
                        close(socket);
                        return;
                    }
                    sleep(sleep_time);
                }

                memset(msg, 0, sizeof(msg));
                int bytes_read = read(socket, &msg, sizeof(msg));
                if (bytes_read < 0)
                {
                    if (errno == EWOULDBLOCK || errno == EAGAIN)
                    {
                        continue;
                    }
                    else
                    {
                        perror("[server]Error at read()\n");
                        close(socket);
                        return;
                    }
                }
                std::string message{msg};
                if (bytes_read > 0)
                {

                    if (message == "s")
                    {
                        std::cout << header << " received stop command\n";
                        stopped = true;
                        break;
                    }
                    else if (message == "p")
                    {
                        std::cout << header << " received pause command\n";
                        paused = !paused;
                    }
                    else if (message == "q")
                    {
                        std::cout << header << " closing connection\n";
                        close(socket);
                        return;
                    }
                }
            }
        }
        else
        {
            std::cout << header << " received unknown command: " << message << "\n";
            strncpy(res, "received unknown command", MAX_SIZE - 1);
            res[MAX_SIZE - 1] = '\0';
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