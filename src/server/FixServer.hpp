#ifndef FIX_SERVER_H
#define FIX_SERVER_H

#include <boost/asio.hpp>
#include "ServerBase.hpp"



namespace fix {
class FixServer : public ServerBase {
    public:
        FixServer(boost::asio::io_context& io_context, short port)
            : ServerBase(io_context, port) {}

};

} // end namespace fix

#endif