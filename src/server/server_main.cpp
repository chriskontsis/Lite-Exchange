#include "fix_server.hpp"
#include <exception>

int main()
{
    try 
    {   
        boost::asio::io_context io_context;
        FIXServer server(io_context);
        io_context.run();
    }
    catch(std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}