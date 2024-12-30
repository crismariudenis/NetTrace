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

using namespace std;

#define MAX_SIZE 4096

#define handle_error(msg, err) \
    {                          \
        perror(msg);           \
        exit(err);             \
    }

extern int errno;

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

    while (true)
    {
        cout << "[client]Enter a command: ";
        cin.getline(msg, MAX_SIZE);

        if (write(sd, msg, MAX_SIZE) <= 0)
        {
            perror("[client]Error at write() to server.\n");
            return errno;
        }

        if (strcmp(msg, "q") == 0)
        {
            cout << "[client]Quitting..." << endl;
            break;
        }

        if (read(sd, msg, MAX_SIZE) < 0)
        {
            perror("[client]Error at read() from server.\n");
            return errno;
        }
        cout << "[client]Received message: " << msg << endl;
    }
    close(sd);
}