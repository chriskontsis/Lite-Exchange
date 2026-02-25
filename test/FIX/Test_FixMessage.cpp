#include <gtest/gtest.h>
#include "../../src/fix/FixMessage.hpp"

using namespace fix;


TEST(FixMessageParser, LimitBuy)
{
    auto req = FixMessage::parse("35=D|11=1001|54=1|38=100|40=2|44=5000|55=AAPL|");
    EXPECT_EQ(req.type, MsgType::NEW_LIMIT_ORDER);
    EXPECT_EQ(req.side, LOB::Side::BUY);
    EXPECT_EQ(req.uid, 1001u);
    EXPECT_EQ(req.quantity, 100u);
    EXPECT_EQ(req.price, 5000u);
    EXPECT_EQ(std::string_view(req.symbol, 4), "AAPL");
}

TEST(FixMessageParser, MarketSell)
{
    auto req = FixMessage::parse("35=D|11=2002|54=2|38=50|40=1|55-MSFT|");
    EXPECT_EQ(req.type, MsgType::NEW_MARKET_ORDER);
    EXPECT_EQ(req.side, LOB::Side::SELL);
    EXPECT_EQ(req.uid, 2002u);
    EXPECT_EQ(req.quantity, 50u);
    EXPECT_EQ(req.price, 0u);
}

TEST(FixMessageParser, Cancel)
{
    auto req = FixMessage::parse("35=F|11=1001|55=AAPL|");
    EXPECT_EQ(req.type, MsgType::CANCEL_ORDER);
    EXPECT_EQ(req.uid, 1001u);
}

TEST(FixMessageParser, MalfornedReturnsUnknown)
{
    auto req = FixMessage::parse("garbage|john=snow|no=tags|");
    EXPECT_EQ(req.type, MsgType::UNKNOWN);
}