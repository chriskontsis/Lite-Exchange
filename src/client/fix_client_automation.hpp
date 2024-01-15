#ifndef FIX_ClIENT_AUTOMATION
#define FIX_ClIENT_AUTOMATION

#include <thread>
#include <string>
#include <queue>

class FixAutomationClient
{
public:
    FixAutomationClient() {}
    void initialize(const std::string& version, const std::string& ordersFile, const std::string& compIdBase, const std::string& targetCompId, int numClients);
    void shutdown();
    void start();
    void join();
    void fixClientAutomationThread(int id);
private:
    std::queue<std::string> resultsQueue;
    std::vector<std::thread> fixClientThreads;
    std::string fixVersion;
    std::string ordersFile;
    std::string compIdBase;
    std::string targetCompId;
    int numClients;
};
#endif