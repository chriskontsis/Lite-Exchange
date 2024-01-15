#include "fix_client_automation.hpp"


void FixAutomationClient::initialize(const std::string& version, const std::string& ordersFile_, const std::string& compIdBase_, const std::string& targetCompId_, int numClients_)
{
    fixVersion = version;
    ordersFile = ordersFile_;
    compIdBase = compIdBase_;
    targetCompId = targetCompId_;
    numClients = numClients_;
}

void FixAutomationClient::fixClientAutomationThread(int clientIdx)
{
    std::string senderCompId = compIdBase + std::to_string(clientIdx);
    

}
void FixAutomationClient::start()
{
    for(size_t i = 0; i < numClients; ++i)
    {
        fixClientThreads.push_back(std::thread(fixClientAutomationThread, i+1));
    }
}

void FixAutomationClient::join()
{
    for(auto& th : fixClientThreads)    
        th.join();
}