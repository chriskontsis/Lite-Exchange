#ifndef FIX_CLIENT_AUTOMATION
#define FIX_CLIENT_AUTOMATION

#include <cstdint>
#include <string>
#include <thread>
#include <vector>

class FixClientAutomation
{
public:
private:
  std::vector<std::thread> clients_;
  uint8_t                  num_clients_ = 0;
  std::string              orders_file_;
  std::string              comp_id_base_;
  std::string              target_comp_id_;
};

#endif
