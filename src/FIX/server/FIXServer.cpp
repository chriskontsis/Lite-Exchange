#include "quickfix/config.h"
#include "quickfix/FileStore.h"
#include "quickfix/SocketAcceptor.h"
#include "quickfix/Log.h"
#include "quickfix/SessionSettings.h"
#include "FIXServerApplication.hpp"

#include <string>
#include <iostream>
#include <fstream>
#include <memory>

void wait()
{
    while(true)
    {
        FIX::process_sleep(1);
    }

}
int main() 
{
    FIX::SessionSettings settings("acceptor.cfg");
    FIXServerApplication server;
    FIX::FileStoreFactory storeFactory(settings);
    FIX::ScreenLogFactory logFactory(settings);

    std::unique_ptr<FIX::Acceptor> acceptor;
    acceptor = std::unique_ptr<FIX::Acceptor>(
      new FIX::SocketAcceptor ( server, storeFactory, settings, logFactory ));
    
    acceptor->start();
    wait();
    acceptor->stop();

}