#ifndef FIX_CLIENT_AUTOMATION
#define FIX_CLIENT_AUTOMATION

#include <cstdint>
#include <thread>
#include <vector>
#include <string>


class FixClientAutomation {
    public:

    private:
        std::vector<std::thread> clients;
        u_int8_t numClients = 0;
        std::string ordersFile;
        std::string compIdBase;
        std::string targetCompId;
};


#endif