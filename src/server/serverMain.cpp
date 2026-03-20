  #include <iostream>
  #include <thread>
  #include <atomic>
  #include <boost/asio.hpp>
  #include "FixServer.hpp"
  #include "../engine/EngineDispatcher.hpp"
  #include "../ipc/MPSC_Queue.hpp"
  #include "../ipc/OrderEvent.hpp"
  #include "../ipc/FillEvent.hpp"
  #include "../gateway/SessionRegistry.hpp"
  #include "../fix/FixMessageBuilder.hpp"

int main() 
{
    MPSC_Queue<ipc::OrderEvent, 4096> inputQ;
    MPSC_Queue<ipc::FillEvent, 4096> outputQ;
    gateway::SessionRegistry registry;

    fix::EngineDispatcher dispatcher(inputQ, outputQ);

    boost::asio::io_context io_context;
    fix::FixServer server(io_context, 12345, inputQ, registry);

    std::atomic<bool> running { true };
    std::thread drainThread([&]() 
    {
        ipc::FillEvent fe;
        while(running.load(std::memory_order_relaxed))
        {
            if(outputQ.tryConsume(fe))
            {
                if(auto session = registry.lookup(fe.session_id_))
                    session->sendData(fix::FixMessageBuilder::executionReport(fe));
            }
        }
    });

    std::cout << "Exchange listening on port 12345\n";
    io_context.run();

    running.store(false, std::memory_order_relaxed);
    drainThread.join();
    return 0;
}