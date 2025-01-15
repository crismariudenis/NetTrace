#include <iostream>
#include <mutex>
#include "trace.h"
#include <sstream>
#include <thread>
std::mutex cout_mutex;

void run_for_client(std::string client_ip)
{
    //______________________HEADER STUFF______________________
    auto myid = std::this_thread::get_id();
    std::stringstream ss;
    ss << myid;
    std::string id = ss.str();
    //________________________________________________________

    Tracer tracer(client_ip);
    while (true)
    {
        tracer.run();

        // Lock the mutex before printing
        std::lock_guard<std::mutex> lock(cout_mutex);

        std::cout << id << ":\n";
        print(tracer);
    }
}
int main()
{

    std::thread client_thread_1(run_for_client, "1.1.1.1");
    sleep(1);
    std::thread client_thread_2(run_for_client, "192.1.2.2");

    // Join the threads to ensure they run to completion
    client_thread_1.join();
    client_thread_2.join();
}