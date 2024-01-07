#include "FIXClientApplication.hpp"


void FIXClientApplication::run() 
{
  std::cout << "HERE" << '\n';
  std::ifstream ifs("test_data.txt");
  std::string line;
  FIX::Message order;
  
  while(getline(ifs, line))
  {
    FIX::Message order(line);
    FIX::Session::sendToTarget(order);
  }
}

void FIXClientApplication::onLogon( const FIX::SessionID& sessionID )
{
  std::cout << std::endl << "Logon - " << sessionID << std::endl;
}

void FIXClientApplication::onLogout(const FIX::SessionID& sessionID)
{
    std::cout << std::endl << "Logout - " << sessionID << std::endl;
}
void FIXClientApplication::fromApp( const FIX::Message& message, const FIX::SessionID& sessionID )
EXCEPT( FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::UnsupportedMessageType )
{
  crack( message, sessionID );
  std::cout << std::endl << "IN: " << message << std::endl;
}

void FIXClientApplication::toApp( FIX::Message& message, const FIX::SessionID& sessionID )
EXCEPT( FIX::DoNotSend )
{
  try
  {
    FIX::PossDupFlag possDupFlag;
    message.getHeader().getField( possDupFlag );
    if ( possDupFlag ) throw FIX::DoNotSend();
  }
  catch ( FIX::FieldNotFound& ) {}

  std::cout << std::endl
  << "OUT: " << message << std::endl;
}

