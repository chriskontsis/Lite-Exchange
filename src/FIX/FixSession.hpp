#ifndef FIX_SESSION_
#define FIX_SESSION_

#include <string> 
#include <ctime>
#include <cstdint>

#include "FixMessage.hpp"


namespace fix
{


class FixSession
{
public:
    FixSession();
    FixSession(const FixSession& other);
    ~FixSession();

    int receive(FixMessage&);
    int send(FixMessage&);

    
    void close();

     
private:
    int fixVersion;
    int outgoingSeqeunceNumber;
    int incomingSequenceNumber;
    std::string targetCompId;
    // sender compid shared by all sessions
    static std::string COMPId;

};

}
#endif