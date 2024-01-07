#ifndef FIX_SERVER_APPLICATION
#define FIX_SERVER_APPLICATION

#include "quickfix/Application.h"
#include "quickfix/MessageCracker.h"
#include "quickfix/Values.h"
#include "quickfix/Utility.h"
#include "quickfix/Mutex.h"
#include "quickfix/fix42/NewOrderSingle.h"

#include <cstdint>

class FIXServerApplication : public FIX::Application, public FIX::MessageCracker
{
private:
    u_int64_t orderId;

public:
    FIXServerApplication() : orderId(0) {}

    // Application overloads
    void onCreate(const FIX::SessionID &) {}
    void onLogon(const FIX::SessionID &sessionID) {}
    void onLogout(const FIX::SessionID &sessionID) {}
    void toAdmin(FIX::Message &, const FIX::SessionID &) {}
    void toApp(FIX::Message &, const FIX::SessionID &)
        EXCEPT(FIX::DoNotSend) {}
    void fromAdmin(const FIX::Message &, const FIX::SessionID &)
        EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::RejectLogon) {}
    void fromApp(const FIX::Message &message, const FIX::SessionID &sessionID)
        EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::UnsupportedMessageType) {}

    void onMessage( const FIX42::NewOrderSingle&, const FIX::SessionID& );
};

#endif