#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <iostream>
#include <fcntl.h>
#include <istream>
#include <iomanip>
#include <vector>
#include <sstream>
using namespace std;

#define MAX_SIZE 4096

#define handle_error(msg, err) \
    {                          \
        perror(msg);           \
        exit(err);             \
    }

extern int errno;

struct line
{
    string ip;
    double loss, snt, last, avg, best, wrst, stdev;
};

std::vector<line> parseTrace(const std::string &str)
{
    std::vector<line> ans;
    std::istringstream stream(str);
    std::string line_str;

    while (std::getline(stream, line_str))
    {
        std::istringstream line_stream(line_str);
        line l;
        std::string temp;

        // Parse the line number
        line_stream >> temp;

        // Parse the IP address or "(waiting for reply)"
        line_stream >> l.ip;

        if (l.ip == "(waiting")
        {
            // Skip the rest of the line
            continue;
        }

        // Parse the rest of the fields
        line_stream >> l.loss >> l.snt >> l.last >> l.avg >> l.best >> l.wrst >> l.stdev;

        ans.push_back(l);
    }

    return ans;
}

int port;
int main(int argc, char *argv[])
{
    int sd;
    struct sockaddr_in server;
    char msg[MAX_SIZE];

    if (argc != 3)
    {
        cout << "Syntax: " << argv[0] << " <server_adress> <port>" << endl;
        return -1;
    }

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Error at socket().\n");
        return errno;
    }
    port = atoi(argv[2]);
    /* umplem structura folosita pentru realizarea conexiunii cu serverul */
    /* familia socket-ului */
    server.sin_family = AF_INET;
    /* adresa IP a serverului */
    server.sin_addr.s_addr = inet_addr(argv[1]);
    /* portul de conectare */
    server.sin_port = htons(port);

    /* ne conectam la server */
    if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[client]Error at connect().\n");
        return errno;
    }

    int flags = fcntl(sd, F_GETFL, 0);
    fcntl(sd, F_SETFL, flags | O_NONBLOCK);

    fd_set read_fds;
    std::string buffer;
    bool stop_tracing = false;
    while (true)
    {
        FD_ZERO(&read_fds);
        FD_SET(sd, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);

        int max_fd = std::max(sd, STDIN_FILENO) + 1;
        if (select(max_fd, &read_fds, NULL, NULL, NULL) < 0)
        {
            perror("[client]Error at select().\n");
        }
        if (FD_ISSET(sd, &read_fds))
        {
            int bytes_received = recv(sd, msg, MAX_SIZE - 1, 0);
            if (bytes_received < 0)
            {
                perror("[client]Error at receiving data.\n");
                break;
            }
            else if (bytes_received == 0)
            {
                std::cout << "[client]Connection with the server closed\n";
                break;
            }

            msg[bytes_received] = '\0';

            if (stop_tracing)
                continue;
            if (strlen(msg) <= 1)
                continue;

            int new_lines = 0;
            for (int i = 0; i < strlen(msg); i++)
                if (msg[i] == '\n')
                    new_lines++;

            std::cout << msg;
            for (int i = 0; i < new_lines + 1; i++)
            {
                std::cout << "\033[F"; // Move cursor up one line
            }
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds))
        {
            std::string input;
            std::getline(std::cin, input);
            if (input == "q")
            {
                system("clear");
                if (send(sd, input.c_str(), input.size(), 0) < 0)
                {
                    perror("[client]Error sending quit command\n");
                }
                break;
            }
            else if (input == "s")
            {
                system("clear");

                stop_tracing = true;

                std::cout << "[client] Stopping trace...\n";
                if (send(sd, input.c_str(), input.size(), 0) < 0)
                {
                    perror("[client]Error sending quit command\n");
                }
            }
            else if (input == "p")
            {
                stop_tracing = !stop_tracing;

                if (send(sd, input.c_str(), input.size(), 0) < 0)
                {
                    perror("[client]Error sending pause command\n");
                }
            }
            else
            {
                system("clear");

                stop_tracing = false;

                if (send(sd, input.c_str(), input.size(), 0) < 0)
                {
                    perror("[client]Error at sending input");
                }
            }
        }
    }
    close(sd);
}