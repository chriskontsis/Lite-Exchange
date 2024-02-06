#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "../../src/matching-engine/LimitTree.hpp"

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
            Order o = Order(1, price, qty, Side::BUY);
            lt.limit(&o);
            THEN("The tree adjusts to record the new order added")
            {
                REQUIRE(lt.limits.size() == 1);
                REQUIRE(qty == lt.volumeAt(price));
                REQUIRE(1 == lt.countAt(price));
                REQUIRE(lt.best != nullptr);
                REQUIRE(&o == lt.best->ordersList.back());
                REQUIRE(&o == lt.best->ordersList.front());
                REQUIRE(lt.best == o.limit);
            }
        }
    }
    GIVEN("A limit tree and a single sell order")
    {
        auto lt = LimitTree<Side::SELL>();
        WHEN("Order is added")
        {
            Order o = Order(1, price, qty, Side::SELL);
            lt.limit(&o);
            THEN("The tree adjusts to record the new order added")
            {
                REQUIRE(lt.limits.size() == 1);
                REQUIRE(qty == lt.volumeAt(price));
                REQUIRE(1 == lt.countAt(price));
                REQUIRE(lt.best != nullptr);
                REQUIRE(&o == lt.best->ordersList.back());
                REQUIRE(&o == lt.best->ordersList.front());
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
            Order one{1, price, qty, Side::BUY};
            Order two{2, nextPrice, qty, Side::BUY};
            lt.limit(&two);
            lt.limit(&one);
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
                REQUIRE(&two == lt.best->ordersList.front());
                REQUIRE(&two == lt.best->ordersList.back());
            }
        }
    }
    GIVEN("a limit tree and 2 sell orders")
    {
        auto lt = LimitTree<Side::SELL>();
        WHEN("the orders are added")
        {
            Order one{1, price, qty, Side::SELL};
            Order two{2, nextPrice, qty, Side::SELL};
            lt.limit(&one);
            lt.limit(&two);
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
                REQUIRE(&one == lt.best->ordersList.front());
                REQUIRE(&one == lt.best->ordersList.back());
            }
        }
    }
}

SCENARIO("Adding two limit orders to tree best being last")
{
    Quantity qty = 100;
    Price price = 1000, nextPrice = 1001;
    GIVEN("a limit tree and 2 buy orders")
    {
        auto lt = LimitTree<Side::BUY>();
        WHEN("the orders are added")
        {
            Order one{1, price, qty, Side::BUY};
            Order two{2, nextPrice, qty, Side::BUY};
            lt.limit(&one);
            lt.limit(&two);
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
                REQUIRE(&two == lt.best->ordersList.front());
                REQUIRE(&two == lt.best->ordersList.back());
            }
        }
    }
    GIVEN("a limit tree and 2 sell orders")
    {
        auto lt = LimitTree<Side::SELL>();
        WHEN("the orders are added")
        {
            Order one{1, price, qty, Side::SELL};
            Order two{2, nextPrice, qty, Side::SELL};
            lt.limit(&two);
            lt.limit(&one);
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
                REQUIRE(&one == lt.best->ordersList.front());
                REQUIRE(&one == lt.best->ordersList.back());
            }
        }
    }
}

// ---------------------------------------------------------------------------
// cancel()
// ---------------------------------------------------------------------------

SCENARIO("Cancel single order from limit tree")
{
    Quantity qty = 100;
    Price price = 1000;
    GIVEN("Limit tree with single buy order")
    {
        auto lt = LimitTree<Side::BUY>();
        Order o = Order(1, price, qty, Side::BUY);
        lt.limit(&o);
        WHEN("The order is cancelled")
        {
            lt.cancel(&o);
            THEN("the tree's data structures are updated accordingly")
            {
                REQUIRE(lt.limits.size() == 0);
                REQUIRE(lt.volumeAt(price) == 0);
                REQUIRE(lt.countAt(price) == 0);
                REQUIRE(lt.best == nullptr);
                REQUIRE(lt.lastBestPrice == price);
            }
        }
    }
    GIVEN("Limit tree with single sell order")
    {
        auto lt = LimitTree<Side::SELL>();
        Order o = Order(1, price, qty, Side::SELL);
        lt.limit(&o);
        WHEN("The order is cancelled")
        {
            lt.cancel(&o);
            THEN("the tree's data structures are updated accordingly")
            {
                REQUIRE(lt.limits.size() == 0);
                REQUIRE(lt.volumeAt(price) == 0);
                REQUIRE(lt.countAt(price) == 0);
                REQUIRE(lt.best == nullptr);
                REQUIRE(lt.lastBestPrice == price);
            }
        }
    }
}

SCENARIO("Cancel orders from limit tree starting with best first")
{
    Quantity qty = 100;
    Price price = 1000, nextPrice = 1001;
    GIVEN("Limit tree with 2 buy orders")
    {
        auto lt = LimitTree<Side::BUY>();
        Order lower(1, price, qty, Side::BUY);
        Order higher(1, nextPrice, qty, Side::BUY);
        lt.limit(&higher);
        lt.limit(&lower);
        WHEN("the best buy order is cancelled")
        {
            lt.cancel(&higher);
            THEN("the limit tree is updated")
            {
                REQUIRE(lt.countOrdersInTree == 1);
                REQUIRE(lt.countAt(nextPrice) == 0);
                REQUIRE(lt.volumeAt(nextPrice) == 0);
                REQUIRE(lt.volumeOfOrdersInTree == qty);
                REQUIRE(lt.best->price == price);
                REQUIRE(lt.best != nullptr);
                REQUIRE(lt.best->ordersList.front() == &lower);
                REQUIRE(lt.best->ordersList.back() == &lower);
                REQUIRE(price == lt.lastBestPrice);
            }
        }
        WHEN("the lower price buy order is cancelled")
        {
            lt.cancel(&lower);
            THEN("the limit tree is updated")
            {
                REQUIRE(lt.countOrdersInTree == 1);
                REQUIRE(lt.countAt(price) == 0);
                REQUIRE(lt.volumeAt(price) == 0);
                REQUIRE(lt.countAt(nextPrice) == 1);
                REQUIRE(lt.best == higher.limit);
                REQUIRE(lt.best->ordersList.size() == 1);
                REQUIRE(nextPrice == lt.lastBestPrice);
                REQUIRE(lt.best->ordersList.front() == &higher);
                REQUIRE(lt.best->ordersList.back() == &higher);
            }
        }
    }
    GIVEN("Limit tree with 2 sell orders")
    {
        auto lt = LimitTree<Side::SELL>();
        Order lower(1, price, qty, Side::SELL);
        Order higher(1, nextPrice, qty, Side::SELL);
        lt.limit(&lower);
        lt.limit(&higher);
        WHEN("the lower sell order is cancelled")
        {
            lt.cancel(&lower);
            THEN("the limit tree is updated")
            {
                REQUIRE(lt.countOrdersInTree == 1);
                REQUIRE(lt.countAt(price) == 0);
                REQUIRE(lt.volumeAt(price) == 0);
                REQUIRE(lt.volumeOfOrdersInTree == qty);
                REQUIRE(lt.best->price == nextPrice);
                REQUIRE(lt.best != nullptr);
                REQUIRE(lt.best->ordersList.front() == &higher);
                REQUIRE(lt.best->ordersList.back() == &higher);
                REQUIRE(nextPrice == lt.lastBestPrice);
            }
        }
        WHEN("the higher sell order is cancelled")
        {
            lt.cancel(&higher);
            THEN("the limit tree is updated")
            {
                REQUIRE(lt.countOrdersInTree == 1);
                REQUIRE(lt.countAt(nextPrice) == 0);
                REQUIRE(lt.volumeAt(nextPrice) == 0);
                REQUIRE(lt.volumeOfOrdersInTree == qty);
                REQUIRE(lt.best->price == price);
                REQUIRE(lt.best != nullptr);
                REQUIRE(lt.best->ordersList.front() == &lower);
                REQUIRE(lt.best->ordersList.back() == &lower);
                REQUIRE(price == lt.lastBestPrice);
            }
        }
    }
}

SCENARIO("Cancel orders from limit tree starting with best last")
{
    Quantity qty = 100;
    Price price = 1000, nextPrice = 1001;
    GIVEN("Limit tree with 2 buy orders")
    {
        auto lt = LimitTree<Side::BUY>();
        Order lower(1, price, qty, Side::BUY);
        Order higher(1, nextPrice, qty, Side::BUY);
        lt.limit(&lower);
        lt.limit(&higher);
        WHEN("the best buy order is cancelled")
        {
            lt.cancel(&higher);
            THEN("the limit tree is updated")
            {
                REQUIRE(lt.countOrdersInTree == 1);
                REQUIRE(lt.countAt(nextPrice) == 0);
                REQUIRE(lt.volumeAt(nextPrice) == 0);
                REQUIRE(lt.volumeOfOrdersInTree == qty);
                REQUIRE(lt.best->price == price);
                REQUIRE(lt.best != nullptr);
                REQUIRE(lt.best->ordersList.front() == &lower);
                REQUIRE(lt.best->ordersList.back() == &lower);
                REQUIRE(price == lt.lastBestPrice);
            }
        }
        WHEN("the lower price buy order is cancelled")
        {
            lt.cancel(&lower);
            THEN("the limit tree is updated")
            {
                REQUIRE(lt.countOrdersInTree == 1);
                REQUIRE(lt.countAt(price) == 0);
                REQUIRE(lt.volumeAt(price) == 0);
                REQUIRE(lt.countAt(nextPrice) == 1);
                REQUIRE(lt.best == higher.limit);
                REQUIRE(lt.best->ordersList.size() == 1);
                REQUIRE(nextPrice == lt.lastBestPrice);
                REQUIRE(lt.best->ordersList.front() == &higher);
                REQUIRE(lt.best->ordersList.back() == &higher);
            }
        }
    }
    GIVEN("Limit tree with 2 sell orders")
    {
        auto lt = LimitTree<Side::SELL>();
        Order lower(1, price, qty, Side::SELL);
        Order higher(1, nextPrice, qty, Side::SELL);
        lt.limit(&higher);
        lt.limit(&lower);
        WHEN("the lower sell order is cancelled")
        {
            lt.cancel(&lower);
            THEN("the limit tree is updated")
            {
                REQUIRE(lt.countOrdersInTree == 1);
                REQUIRE(lt.countAt(price) == 0);
                REQUIRE(lt.volumeAt(price) == 0);
                REQUIRE(lt.volumeOfOrdersInTree == qty);
                REQUIRE(lt.best->price == nextPrice);
                REQUIRE(lt.best != nullptr);
                REQUIRE(lt.best->ordersList.front() == &higher);
                REQUIRE(lt.best->ordersList.back() == &higher);
                REQUIRE(nextPrice == lt.lastBestPrice);
            }
        }
        WHEN("the higher sell order is cancelled")
        {
            lt.cancel(&higher);
            THEN("the limit tree is updated")
            {
                REQUIRE(lt.countOrdersInTree == 1);
                REQUIRE(lt.countAt(nextPrice) == 0);
                REQUIRE(lt.volumeAt(nextPrice) == 0);
                REQUIRE(lt.volumeOfOrdersInTree == qty);
                REQUIRE(lt.best->price == price);
                REQUIRE(lt.best != nullptr);
                REQUIRE(lt.best->ordersList.front() == &lower);
                REQUIRE(lt.best->ordersList.back() == &lower);
                REQUIRE(price == lt.lastBestPrice);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Market()
// ---------------------------------------------------------------------------

SCENARIO("a market order is submitted with no order in the tree")
{
    GIVEN("An empty limit tree")
    {
        LimitTree<Side::BUY> lt;
        Quantity quantity = 100;
        WHEN("a buy market order is submitted")
        {
            Order market{1, 0, quantity, Side::SELL};
            lt.market(&market, [](UID) {});
            THEN("the tree is empty")
            {
                REQUIRE(lt.limits.size() == 0);
                REQUIRE(lt.best == nullptr);
                REQUIRE(lt.countOrdersInTree == 0);
                REQUIRE(lt.volumeOfOrdersInTree == 0);
                REQUIRE(lt.lastBestPrice == 0);
            }
        }
    }
}

SCENARIO("a market order is submitted with perfect match")
{
    GIVEN("An order book with a limit order and a matched market order")
    {
        LimitTree<Side::BUY> lt;
        Quantity quantity = 1000;
        Price price = 100;
        WHEN("a sell market order is matched to a buy limit order")
        {
            Order limit{1, price, quantity, Side::BUY};
            Order market{2, 0, quantity, Side::SELL};
            lt.limit(&limit);
            lt.market(&market, [](UID) {});
            THEN("the limit_fill and market_fill functions should fire")
            {
                REQUIRE(lt.best == nullptr);
                REQUIRE(lt.volumeAt(price) == 0);
                REQUIRE(lt.lastBestPrice == price);
            }
        }
    }
}

SCENARIO("a market order is submitted with partial match")
{
    GIVEN("An order book with a limit order and a smaller market order")
    {
        LimitTree<Side::BUY> lt;
        Quantity qtyLimit = 1000, qtyMarket = 500;
        Price price = 100;
        WHEN("a buy market order is submitted")
        {
            Order limit{1, price, qtyLimit, Side::BUY};
            Order market{2, 0, qtyMarket, Side::SELL};
            lt.limit(&limit);
            lt.market(&market, [](UID) {});
            THEN("the limit_fill and market_fill functions should fire")
            {
                REQUIRE(lt.best != nullptr);
                REQUIRE(qtyLimit - qtyMarket == lt.volumeAt(price));
                REQUIRE(lt.lastBestPrice == price);
            }
        }
    }
}

SCENARIO("a market order is submitted that spans multiple limit orders")
{
    GIVEN("An order book with two limits and a market order that requires both")
    {
        LimitTree<Side::BUY> lt;
        Quantity qtyLimit1 = 40;
        Quantity qtyLimit2 = 60;
        Quantity qtyMarket = 90;
        Price price = 100;

        Order limit1{1, price, qtyLimit1, Side::BUY};
        Order limit2{2, price, qtyLimit2, Side::BUY};
        lt.limit(&limit1);
        lt.limit(&limit2);

        WHEN("a sell market order is submitted")
        {
            Order market{3, 0, qtyMarket, Side::SELL};
            lt.market(&market, [](UID) {});
            THEN("the limit fill & market fill functions should fire")
            {
                REQUIRE(nullptr != lt.best);
                REQUIRE(lt.best->price == 100);
                REQUIRE(price == lt.lastBestPrice);
                REQUIRE(qtyLimit1 + qtyLimit2 - qtyMarket == lt.volumeAt(price));
            }
        }
    }
}


SCENARIO("a market order is submitted that spans multiple limit orders and clears book")
{
    GIVEN("An order book with two limits and a market order that requires both")
    {
        LimitTree<Side::BUY> lt;
        Quantity qtyLimit1 = 40;
        Quantity qtyLimit2 = 60;
        Quantity qtyMarket = 100;
        Price price = 100;

        Order limit1{1, price, qtyLimit1, Side::BUY};
        Order limit2{2, price, qtyLimit2, Side::BUY};
        lt.limit(&limit1);
        lt.limit(&limit2);

        WHEN("a sell market order is submitted")
        {
            Order market{3, 0, qtyMarket, Side::SELL};
            lt.market(&market, [](UID) {});
            THEN("the limit fill & market fill functions should fire")
            {
                REQUIRE(nullptr == lt.best);
                REQUIRE(price == lt.lastBestPrice);
                REQUIRE(0 == lt.volumeAt(price));
            }
        }
    }
}

SCENARIO("a market order is submitted that spans multiple limit prices and clears book")
{
    GIVEN("An order book with two limits and a market order that requires both")
    {
        LimitTree<Side::BUY> lt;
        Quantity qtyLimit = 100;
        Quantity qtyMarket = 2*qtyLimit;
        Price price1 = 100, price2 = 101;

        Order limit1{1, price1, qtyLimit, Side::BUY};
        Order limit2{2, price2, qtyLimit, Side::BUY};
        lt.limit(&limit1);
        lt.limit(&limit2);

        WHEN("a sell market order is submitted")
        {
            Order market{3, 0, qtyMarket, Side::SELL};
            lt.market(&market, [](UID) {});
            THEN("the limit fill & market fill functions should fire")
            {
                REQUIRE(nullptr == lt.best);
                REQUIRE(0 == lt.volumeAt(price1));
                REQUIRE(0 == lt.volumeAt(price2));
                REQUIRE(price1 == lt.lastBestPrice);
            }
        }
    }
}


SCENARIO("a market order is submitted with a limit price")
{
    GIVEN("An order book with two limits and a market with a limit price")
    {
        LimitTree<Side::BUY> lt;
        Quantity qtyLimit1 = 100;
        Quantity qtyLimit2 = 100;
        Quantity qtyMarket = 200;

        Price price1 = 100, price2 = 101;

        Order limit1{1, price1, qtyLimit1, Side::BUY};
        Order limit2{2, price2, qtyLimit2, Side::BUY};
        lt.limit(&limit1);
        lt.limit(&limit2);

        WHEN("a sell market order is submitted")
        {
            Order market{3, price2, qtyMarket, Side::SELL};
            lt.market(&market, [](UID) {});
            THEN("the limit fill & market fill functions should fire")
            {
                REQUIRE(nullptr != lt.best);
                REQUIRE(price1 == lt.best->price);
                REQUIRE(100 == lt.volumeAt(price1));
                REQUIRE(0 == lt.volumeAt(price2));
                REQUIRE(qtyLimit1 == lt.volumeAt(price1));
            }
            THEN("the market order should have shares leftover")
            {
                REQUIRE(qtyMarket - qtyLimit2 == market.quantity);
                REQUIRE(price2 == market.price);
            }
        }
    }
}

SCENARIO("a market order is submitted with a limit price that spans the book")
{
    GIVEN("An order book with two limits and a market with a limit price")
    {
        LimitTree<Side::BUY> lt;
        Quantity qtyLimit1 = 100;
        Quantity qtyLimit2 = 100;
        Quantity qtyLimit3 = 100;
        Quantity qtyMarket = 300;

        Price price1 = 100, price2 = 101, price3 = 102;

        Order limit1{1, price1, qtyLimit1, Side::BUY};
        Order limit2{2, price2, qtyLimit2, Side::BUY};
        Order limit3{3, price3, qtyLimit3, Side::BUY};
        lt.limit(&limit1);
        lt.limit(&limit2);
        lt.limit(&limit3);

        WHEN("a sell market order is submitted")
        {
            Order market{3, price2, qtyMarket, Side::SELL};
            lt.market(&market, [](UID) {});
            THEN("the limit fill & market fill functions should fire")
            {
                REQUIRE(nullptr != lt.best);
                REQUIRE(price1 == lt.best->price);
                REQUIRE(100 == lt.volumeAt(price1));
                REQUIRE(0 == lt.volumeAt(price2));
                REQUIRE(0 == lt.volumeAt(price3));
                REQUIRE(price1 == lt.lastBestPrice);
            }
            THEN("the market order should have shares leftover")
            {
                REQUIRE(qtyMarket - (qtyLimit2 + qtyLimit3) == market.quantity);
                REQUIRE(price2 == market.price);
            }
        }
    }
}



// ---------------------------------------------------------------------------
// cancel()
// ---------------------------------------------------------------------------

