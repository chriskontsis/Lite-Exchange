#ifndef FIX_SESSION_MANAGER
#define FIX_SESSION_MANAGER

#include <vector>
#include <memory>
#include "FixSession.hpp"

namespace fix
{
    class FixSessionManager
    {
    public:
    private:
        static std::vector<std::unique_ptr<FixSession>> sessions;
    };
}
#endif