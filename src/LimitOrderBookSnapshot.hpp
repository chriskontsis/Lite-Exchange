#ifdef LIMITORDERBOOKSNAPSHOT_HPP
#define LIMITORDERBOOKSNAPSHOT_HPP


#include "Structures.hpp"
#include <array>

class LimitOrderBookSnapshot 
{
public:
    LimitOrderBookSnapshot() {}
    LimitOrderBookSnapshot(std::array<Order, 10> bids, std::array<Order, 10> asks);
private:

};
#endif