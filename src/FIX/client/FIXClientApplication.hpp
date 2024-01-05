#ifndef FIX_CLIENT_HPP
#define FIX_CLIENT_HPP

#include "quickfix/Application.h"
#include "quickfix/MessageCracker.h"
#include "quickfix/Values.h"
#include "quickfix/Mutex.h"
#include "quickfix/Session.h"
#include "quickfix/config.h"

#include <iostream>

class FIXClientApplication : public FIX::Application, public FIX::MessageCracker
{
public:
    void run();

private:
    void onCreate(const FIX::SessionID &) {}
    void onLogon(const FIX::SessionID &sessionID);
    void onLogout(const FIX::SessionID &sessionID);
    void toAdmin(FIX::Message &, const FIX::SessionID &) {}
    void toApp(FIX::Message &, const FIX::SessionID &)
        EXCEPT(FIX::DoNotSend);
    void fromAdmin(const FIX::Message &, const FIX::SessionID &)
        EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::RejectLogon) {}
    void fromApp(const FIX::Message &message, const FIX::SessionID &sessionID)
        EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::UnsupportedMessageType);
};
#endif