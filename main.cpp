#include "server.h"
#include <algorithm>
int main(int argc, char* argv[])
{
    //Validataing args
    bool global = false;
    if (argc > 1)
    {
        std::string data(argv[1]);
        std::transform(data.begin(), data.end(), data.begin(), ::tolower);
        if (data == "global") global = true;
    }

    std::cout << "Launching " << (global ? "global..." : "local...") << std::endl;
    server serv;
    serv.set_global_addr(global);
    serv.init();
    serv.start();
    std::string s;

    while (true)
    {
        std::cin >> s;
        if (s.compare("shutdown") == 0)
        {
            serv.shutdown();
            break;
        }
        if (s.compare("clients") == 0)
        {
            std::cout << serv.clients_count() << std::endl;
            continue;
        }
        std::cout << "Unknown command";
    }
    return 0;
}