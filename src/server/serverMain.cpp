#include <iostream>
#include <boost/asio.hpp>
#include "FixServer.hpp"
#include "../engine/EngineDispatcher.hpp"

int main() 
{
    fix::EngineDispatcher dispatcher;
    boost::asio::io_context io_context;
    fix::FixServer server(io_context, 12345, dispatcher);
    std::cout << "Exchange listening on port 12345\n";
    io_context.run();
    return 0;
}