#ifndef FIX_MESSAGE_HPP
#define FIX_MESSAGE_HPP

#include <string>
#include <unordered_map>

class FixMessage
{
    private:
        int fixVersion;
        int seqNum;
        int messageType;
        std::string senderCompId;
        std::string targetCompId;
        std::string sendingTime;
        std::unordered_map<int, std::string> bodyTagValuePairs;
};

#endif