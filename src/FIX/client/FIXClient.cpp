#include "quickfix/config.h"
#include "quickfix/FileStore.h"
#include "quickfix/FileLog.h"
#include "quickfix/SocketInitiator.h"
#include "quickfix/SessionSettings.h"
#include "quickfix/Log.h"
#include "FIXClientApplication.hpp"

#include <string>
#include <iostream>
#include <fstream>
#include <memory>

int main() 
{   
    std::string file = "sender.cfg";
    FIX::SessionSettings settings(file);
    FIXClientApplication client;
    FIX::FileStoreFactory storeFactory(settings);
    FIX::FileLogFactory logFactory(settings);
    auto initiator = std::unique_ptr<FIX::Initiator>( 
      new FIX::SocketInitiator( client, storeFactory, settings, logFactory ) );
    
    initiator->start();
    client.run();
    initiator->stop();

}