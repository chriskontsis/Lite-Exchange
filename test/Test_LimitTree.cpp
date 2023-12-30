#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "../src/LimitTree.hpp"

using namespace LOB;


// ---------------------------------------------------------------------------
// MARK: constructor
// ---------------------------------------------------------------------------

SCENARIO("should initialize LimitTree") {
    WHEN("a buy limit tree is constructed") {
        THEN("the initial parameters are correct") {
            LimitTree<Side::BUY> tree;
            REQUIRE(tree.limits.size() == 0);
            REQUIRE(tree.best == nullptr);
        }
    }
    WHEN("a sell limit tree is constructed") {
        THEN("the initial parameters are correct") {
            LimitTree<Side::SELL> tree;
            REQUIRE(tree.limits.size() == 0);
            REQUIRE(tree.best == nullptr);
        }
    }
}