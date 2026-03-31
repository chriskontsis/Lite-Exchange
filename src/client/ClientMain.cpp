#include <chrono>
#include <thread>

#include "../fix/FixMessageBuilder.hpp"
#include "FixClient.hpp"

int main()
{
  fix::FixClient client("127.0.0.1", 12345);
  client.send(fix::FixMessageBuilder::limit<LOB::Side::BUY>(1, 10, 100, "AAPL"));
  client.send(fix::FixMessageBuilder::limit<LOB::Side::SELL>(2, 10, 100, "AAPL"));
  client.send(fix::FixMessageBuilder::market<LOB::Side::BUY>(3, 5, "AAPL"));
  client.send(fix::FixMessageBuilder::cancel(4, "AAPL"));

  std::this_thread::sleep_for(std::chrono::seconds(1));
  return 0;
}