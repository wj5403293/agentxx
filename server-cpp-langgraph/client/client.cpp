#include "agent/deepagent.h"
#include "util/log.h"
#include "util/string_util.h"
#include <fstream>
#include <iostream>
#include <map>

using namespace std;

int main(int argn, char **argv) {
#if IS_LINUX_D
  agentxx::logxx::signalError(argv[0]);
#endif
  std::cout << "======= Test Start =======" << std::endl;
  agentxx::DeepAgent agent{};
  agent.init();
  agent.runCli();
  std::cout << "======= Test Done =======" << std::endl;
  return 0;
}