#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "../src/LimitTree.hpp"

using namespace LOB;

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

SCENARIO("should initialize LimitTree")
{
    WHEN("a buy limit tree is constructed")
    {
        THEN("the initial parameters are correct")
        {
            LimitTree<Side::BUY> tree;
            REQUIRE(tree.limits.size() == 0);
            REQUIRE(tree.best == nullptr);
        }
    }
    WHEN("a sell limit tree is constructed")
    {
        THEN("the initial parameters are correct")
        {
            LimitTree<Side::SELL> tree;
            REQUIRE(tree.limits.size() == 0);
            REQUIRE(tree.best == nullptr);
        }
    }
}

// ---------------------------------------------------------------------------
// Limit()
// ---------------------------------------------------------------------------

SCENARIO("Adding single order to limit tree")
{
    Quantity qty = 100;
    Price price = 1000;
    GIVEN("A limit tree and a single buy order")
    {
        auto lt = LimitTree<Side::BUY>();
        WHEN("Order is added")
        {
            Order o = Order(1,price,qty,Side::BUY);
            lt.limit(o);
            THEN("The tree adjusts to record the new order added")
            {
                REQUIRE(lt.limits.size() == 1);
                REQUIRE(qty == lt.volumeAt(price));
                REQUIRE(1 == lt.countAt(price));
                REQUIRE(lt.best != nullptr);
                REQUIRE(o.uid == lt.best->ordersList.front().uid);
                REQUIRE(o.uid == lt.best->ordersList.back().uid);
                REQUIRE(lt.best == o.limit);
            }
        }
    }
    GIVEN("A limit tree and a single sell order")
    {
        auto lt = LimitTree<Side::SELL>();
        WHEN("Order is added")
        {
            Order o = Order(1,price,qty,Side::SELL);
            lt.limit(o);
            THEN("The tree adjusts to record the new order added")
            {
                REQUIRE(lt.limits.size() == 1);
                REQUIRE(qty == lt.volumeAt(price));
                REQUIRE(1 == lt.countAt(price));
                REQUIRE(lt.best != nullptr);
                REQUIRE(o.uid == lt.best->ordersList.front().uid);
                REQUIRE(o.uid == lt.best->ordersList.back().uid);
                REQUIRE(lt.best == o.limit);
            }
        }
    }
}

SCENARIO("Adding two limit orders to tree best being first")
{
    Quantity qty = 100;
    Price price = 1000, nextPrice = 1001;
    GIVEN("a limit tree and 2 buy orders")
    {
        auto lt = LimitTree<Side::BUY>();
        WHEN("the orders are added")
        {
            Order one {1,price,qty,Side::BUY};
            Order two {2,nextPrice,qty,Side::BUY};
            lt.limit(two);
            lt.limit(one);
            THEN("the limit tree contains the orders in the correct places")
            {
                REQUIRE(lt.countOrdersInTree == 2);
                REQUIRE(qty == lt.volumeAt(nextPrice));
                REQUIRE(qty == lt.volumeAt(price));
                REQUIRE(1 == lt.countAt(price));
                REQUIRE(1 == lt.countAt(nextPrice));
                REQUIRE(nullptr != lt.best);
                REQUIRE(lt.countOrdersInTree == 2);
                REQUIRE(nextPrice == lt.best->price);
                REQUIRE(two.uid == lt.best->ordersList.front().uid);
            }
        }
    }
    GIVEN("a limit tree and 2 sell orders")
    {
        auto lt = LimitTree<Side::SELL>();
        WHEN("the orders are added")
        {
            Order one {1,price,qty,Side::SELL};
            Order two {2,nextPrice,qty,Side::SELL};
            lt.limit(one);
            lt.limit(two);
            THEN("the limit tree contains the orders in the correct places")
            {
                REQUIRE(lt.countOrdersInTree == 2);
                REQUIRE(qty == lt.volumeAt(nextPrice));
                REQUIRE(qty == lt.volumeAt(price));
                REQUIRE(1 == lt.countAt(price));
                REQUIRE(1 == lt.countAt(nextPrice));
                REQUIRE(nullptr != lt.best);
                REQUIRE(lt.countOrdersInTree == 2);
                REQUIRE(price == lt.best->price);
                REQUIRE(one.uid == lt.best->ordersList.front().uid);
            }
        }
    }
}


