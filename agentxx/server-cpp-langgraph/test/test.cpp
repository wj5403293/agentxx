#include <fstream>
#include <iostream>
#include <map>

using namespace std;

void test() {
}

int main(int argn, char** argv) {
#if IS_LINUX_D
    agentxx::logxx::signalError(argv[0]);
#endif
    std::cout << "======= Test Start =======" << std::endl;
    test();
    std::cout << "======= Test Done =======" << std::endl;
    std::cout << ">>>";
    int num = 0;
    cin >> num;
    return 0;
}