#ifndef FIX_CLIENT_
#define FIX_CLIENT_

#include <string>
#include <thread>
#include <memory>
#include <cstdint>
#include <boost/asio.hpp>


#include "Utility/MessageQueue.hpp"
#include "fix/FIxMessage.hpp"
#include "fix/FixSession.hpp"

using namespace fix;

class FixClient
{
public:
    FixClient() : socket_(io_context_) {}
    virtual ~FixClient()
    {
        disconnect();
    }

    bool connect(const std::string& host, const uint16_t port)
    {
        try
        {
            connection = std::make_unique<FixSession>();
            boost::asio::ip::tcp::resolver resolver(io_context_);
            
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
        }
        
        return false;
    }

    bool disconnect()
    {
        return false
    }

    bool isConnected()
    {
        if(connection)
            return connection->isConnected();
        else
            return false;
    }

    TSQueue<FixMessage>& incomingMessages()
    {
        return qMessagesIn;
    }

protected:
    // asio context handles the data transfer
    boost::asio::io_context io_context_;
    // needs thread of its own to execute work
    std::thread thrContext;
    // hardware socket that is connected to server
    boost::asio::ip::tcp::socket socket_;
    // client has single instance of "session", handles data transfer
    std::unique_ptr<FixSession> connection;
private:
    TSQueue<FixMessage> qMessagesIn;
};

#endif