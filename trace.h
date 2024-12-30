#include <iostream>
#include <cmath>
#include <vector>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <cstring>
#include <chrono>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <algorithm>
#define MAX_SIZE 4096
#define TIMEOUT 1
#define MAX_HOPS 30
#define PROBES 1
extern int errno;

using namespace std;

int port = 33434; // Starting port for traceroute
struct line{
    string ip;
    double loss, snt, last, avg, best, wrst, stdev;
};
void trace(std::string IP)
{
    int sd;                    // descriptorul de socket
    struct sockaddr_in server; // structura folosita pentru conectare
    char msg[MAX_SIZE];        // mesajul bufffer
    int msglen = 0, length = 0;

    int recv_sd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (recv_sd < 0)
    {
        perror("Error at creating a war socket");
        return;
    }

    /* cream socketul */
    if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        perror("Error creating UDP socket");
        return;
    }

    struct timeval tv;
    tv.tv_sec = TIMEOUT;
    tv.tv_usec = 0;
    if (setsockopt(recv_sd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        perror("Error setting socket option");
        return;
    }

    /* umplem structura folosita pentru realizarea dialogului cu serverul */
    /* familia socket-ului */
    server.sin_family = AF_INET;
    /* adresa IP a serverului */
    server.sin_addr.s_addr = inet_addr(IP.c_str());
    /* portul de conectare */
    server.sin_port = htons(port);

    for (int ttl = 1; ttl <= MAX_HOPS; ttl++)
    {
        // Set TTL for the UDP socket
        if (setsockopt(sd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) < 0)
        {
            perror("Error setting TTL");
            return;
        }

        std::vector<double> times;
        int sent_probes = 0;
        int received_probes = 0;
        auto start = chrono::high_resolution_clock::now();
        sprintf(msg, "Traceroute test message");

        if (sendto(sd, msg, strlen(msg), 0, (struct sockaddr *)&server, sizeof(server)) <= 0)
        {
            perror("Error sending packet");
        }
        sent_probes++;

        // Receive ICMP message
        struct sockaddr_in from;
        socklen_t fromlen = sizeof(from);
        if ((msglen = recvfrom(recv_sd, msg, MAX_SIZE, 0, (struct sockaddr *)&from, &fromlen)) < 0)
        {
            // If no response is received, print asterisks
            printf("%2d. (waiting for reply)\n", ttl);
        }
        else
        {
            auto end = chrono::high_resolution_clock::now();
            double time = chrono::duration<double, milli>(end - start).count();
            times.push_back(time);
            received_probes++;

            struct ip *ip_hdr = (struct ip *)msg;
            struct icmp *icmp_hdr = (struct icmp *)(msg + (ip_hdr->ip_hl << 2));

            if (icmp_hdr->icmp_type == ICMP_TIME_EXCEEDED)
            {
                // If the ICMP message is a Time Exceeded message, print the address of the router
                char addr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &from.sin_addr, addr, sizeof(addr));

                // Resolve IP address to hostname
                char host[NI_MAXHOST];
                if (getnameinfo((struct sockaddr *)&from, sizeof(from), host, sizeof(host), NULL, 0, 0) == 0)
                {
                    printf("%2d. %s %.2f ms\n", ttl, host, time);
                }
                else
                {
                    printf("%2d. %s %.2f ms\n", ttl, addr, time);
                }
            }
            else if (icmp_hdr->icmp_type == ICMP_ECHOREPLY)
            {
                // If the ICMP message is an Echo Reply, print the address of the destination and break the loop
                char addr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &from.sin_addr, addr, sizeof(addr));

                // Resolve IP address to hostname
                char host[NI_MAXHOST];
                if (getnameinfo((struct sockaddr *)&from, sizeof(from), host, sizeof(host), NULL, 0, 0) == 0)
                {
                    printf("%2d. %s %.2f ms\n", ttl, host, time);
                }
                else
                {
                    printf("%2d. %s %.2f ms\n", ttl, addr, time);
                }
                break;
            }

            // Statistics part
        }
        // if (!times.empty())
        // {
        //     double sum = 0;
        //     double minT = *min_element(times.begin(), times.end());
        //     double maxT = *max_element(times.begin(), times.end());

        //     for (auto x : times)
        //         sum += x;
        //     double avg = sum / times.size();

        //     double sq_sum = 0;
        //     for (double x : times)
        //     {
        //         sq_sum += (x - avg) * (x - avg);
        //     }
        //     double stddev_rtt = sqrt(sq_sum / times.size());

        //     printf("TTL %2d: Sent = %d, Received = %d, Loss = %.2f%%, Min = %.2f ms, Avg = %.2f ms, Max = %.2f ms, Stddev = %.2f ms\n",
        //            ttl, sent_probes, received_probes, (1.0 - (double)received_probes / sent_probes) * 100.0, minT, avg, maxT, stddev_rtt);
        // }
    }
    close(sd);
    close(recv_sd);
}