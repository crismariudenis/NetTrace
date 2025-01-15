#include <iostream>
#include <cmath>
#include <vector>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <cstring>
#include <fcntl.h>
#include <chrono>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <map>
#include <random>
#include <iomanip>
#include <thread>
#include <algorithm>
#define MAX_SIZE 4096
#define TIMEOUT 3
#define PROBES 1
extern int errno;

using namespace std;

struct line
{
    string ip;
    double loss, snt, last, avg, best, wrst, stdev;
    int received = 0, lost = 0;
};

void print_header()
{
    std::vector<int> px = {24, 4, 5, 8, 7, 7};
    std::vector<std::string> names = {"Host", "Loss%", "Snt", "Avg", "Best", "Wrst", "StDev"};
    for (int i = 0; i < names.size(); i++)
    {
        std::cout << names[i] << std::string(px[i], ' ');
    }
    std::cout << '\n';
}

class Tracer
{
public:
    int sd, recv_sd;
    struct sockaddr_in server;
    char msg[MAX_SIZE];
    int msglen;
    bool destination_reached;
    std::string IP;
    std::vector<line> results;
    int ttl;
    int MAX_HOPS = 30;
    int icmp_id_;
    ~Tracer()
    {
        close(sd);
        close(recv_sd);
    }
    Tracer(std::string IP) : IP(IP)
    {
        results.resize(MAX_HOPS);
        // Create sockets
        sd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
        recv_sd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
        // int flags = fcntl(recv_sd, F_GETFL, 0);
        // fcntl(recv_sd, F_SETFL, flags | O_NONBLOCK);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1, 65535);
        icmp_id_ = dis(gen);

        // std::cout << "Tracer created with sockets: sd = " << sd << ", recv_sd = " << recv_sd << std::endl;
        if (sd < 0 || recv_sd < 0)
        {
            perror("Error creating socket");
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

        server.sin_family = AF_INET;
        server.sin_addr.s_addr = inet_addr(IP.c_str());

        bool destination_reached = false;

        for (auto &r : results)
            r.best = std::numeric_limits<double>::max();
    }

    void run()
    {
        destination_reached = false;

        for (ttl = 1; ttl <= MAX_HOPS && !destination_reached; ttl++)
        {
            int loc_ttl = ttl;
            if (setsockopt(sd, IPPROTO_IP, IP_TTL, &loc_ttl, sizeof(loc_ttl)) < 0)
            {
                perror("Error setting TTL");
                return;
            }
            line &hop = results[ttl - 1];
            hop.snt += PROBES;

            vector<double> times;
            for (int probe = 0; probe < PROBES; probe++)
            {
                auto start = std::chrono::high_resolution_clock::now();

                // Create ICMP Echo Request packet
                struct icmp icmp_hdr;
                icmp_hdr.icmp_type = ICMP_ECHO;
                icmp_hdr.icmp_code = 0;
                icmp_hdr.icmp_id = icmp_id_;
                icmp_hdr.icmp_seq = ttl * PROBES + probe;
                icmp_hdr.icmp_cksum = 0;
                icmp_hdr.icmp_cksum = calculate_checksum(&icmp_hdr, sizeof(icmp_hdr));

                if (sendto(sd, &icmp_hdr, sizeof(icmp_hdr), 0, (struct sockaddr *)&server, sizeof(server)) <= 0)
                    perror("Error sending packet");

                struct sockaddr_in from;
                socklen_t fromlen = sizeof(from);
                msglen = recvfrom(recv_sd, msg, MAX_SIZE, 0, (struct sockaddr *)&from, &fromlen);
                if (msglen <= 0)
                {
                    hop.lost++;
                }
                else
                {
                    hop.received++;
                    auto end = std::chrono::high_resolution_clock::now();
                    double time = std::chrono::duration<double, std::milli>(end - start).count();
                    times.push_back(time);

                    struct ip *ip_hdr = (struct ip *)msg;
                    struct icmp *icmp_hdr = (struct icmp *)(msg + (ip_hdr->ip_hl << 2));

                    if (icmp_hdr->icmp_type == ICMP_TIME_EXCEEDED || icmp_hdr->icmp_type == ICMP_ECHOREPLY)
                    {
                        char addr[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &from.sin_addr, addr, sizeof(addr));

                        hop.ip = std::string{addr};
                        char host[NI_MAXHOST];
                        if (getnameinfo((struct sockaddr *)&from, sizeof(from), host, sizeof(host), NULL, 0, 0) == 0)
                        {
                            hop.ip = std::string{host};
                        }

                        if (time < hop.best)
                            hop.best = time;
                        if (time > hop.wrst)
                            hop.wrst = time;
                        hop.avg += time;

                        if (icmp_hdr->icmp_type == ICMP_ECHOREPLY)
                        {
                            destination_reached = true;
                        }
                    }
                }
            }

            if (times.size() > 0)
            {
                hop.avg /= times.size();
                for (double t : times)
                {
                    hop.stdev += (t - hop.avg) * (t - hop.avg);
                }
                hop.stdev = sqrt(hop.stdev / times.size());
            }

            hop.loss = (double)(hop.lost) / (hop.lost + hop.received);
        }
        // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    unsigned short calculate_checksum(void *b, int len)
    {
        unsigned short *buf = (unsigned short *)b;
        unsigned int sum = 0;
        unsigned short result;

        for (sum = 0; len > 1; len -= 2)
            sum += *buf++;
        if (len == 1)
            sum += *(unsigned char *)buf;
        sum = (sum >> 16) + (sum & 0xFFFF);
        sum += (sum >> 16);
        result = ~sum;
        return result;
    }
};

void print(Tracer &tracer)
{
    for (int i = 0; i < tracer.ttl - 1; i++)
    {
        // std::cout << "\033[" << tracer.ttl + 1 << ";0H";
        const line &h = tracer.results[i];
        if (h.ip.empty())
        {
            if (i + 1 < 10)
                std::cout << ' ';
            std::cout << i + 1 << ". (waiting for reply)" << std::string(64, ' ') << '\n';
        }
        else
        {
            int ip_length = h.ip.size();
            int loss_spacing = 25 - ip_length;
            // std::cout << h.loss << '\n';
            std::cout << std::fixed << std::setprecision(2);
            if (i + 1 < 10)
                std::cout << ' ';
            std::cout << (i + 1) << ". " << h.ip
                      << std::string(std::max(0, loss_spacing), ' ') << h.loss
                      << std::string(5, ' ') << (int)h.snt
                      << std::string(std::max(0, 15 - (int)std::to_string(h.avg).size()), ' ') << h.avg
                      << std::string(std::max(0, 15 - (int)std::to_string(h.best).size()), ' ') << h.best
                      << std::string(std::max(0, 15 - (int)std::to_string(h.wrst).size()), ' ') << h.wrst
                      << std::string(std::max(0, 15 - (int)std::to_string(h.stdev).size()), ' ') << h.stdev
                      << '\n';
        }
    }
}

std::vector<line> log(Tracer &tracer)
{
    std::vector<line> ans(tracer.ttl - 1);

    for (int i = 0; i < tracer.ttl - 1; i++)
    {
        ans[i] = tracer.results[i];
    }
    return ans;
}