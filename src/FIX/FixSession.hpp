#ifndef FIX_SESSION_
#define FIX_SESSION_

#include <memory>
#include <boost/asio.hpp>

#include "FixMessage.hpp"
#include "Utility/MessageQueue.hpp"

namespace fix
{

    class FixSession : std::enable_shared_from_this<FixSession>
    {
        public:
            FixSession() {}
    
            
            bool connectToServer();
            bool disconnect();
            bool isConnected();

            bool send(const FixMessage& msg);

        protected:
            // each session has unique socket to a remote
            boost::asio::ip::tcp::socket socket_;
            // context is shared with whole asio instance
            boost::asio::io_context& io_context_;
            // queue holds all messsages to be sent to the remote side of this connection
            TSQueue<FixMessage> qMessagesOut;
            TSQueue<FixMessage>& qMessagesIn;
            
    };
}

#endif