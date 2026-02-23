#ifndef FIX_SESSION_HPP
#define FIX_SESSION_HPP

#include <boost/asio.hpp>
#include <memory>
#include <iostream>
#include "FixMessage.hpp"
#include "../engine/EngineDispatcher.hpp"

namespace fix {
    class FixSession : public std::enable_shared_from_this<FixSession>
    {
        private:
            boost::asio::ip::tcp::socket socket_;
            EngineDispatcher&            dispatcher;
            static constexpr int         BUFFER_SIZE = 1024;
            char                         data_[BUFFER_SIZE];
    };

} // namespace fix


#endif