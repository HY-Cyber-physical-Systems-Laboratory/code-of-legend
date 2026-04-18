#include <drogon/drogon.h>
#include <iostream>

int main()
{
    auto &app = drogon::app();
    app.loadConfigFile("../config/config.json");

    app.registerBeginningAdvice([] {
        std::cout << "Code of Legend running on http://0.0.0.0:"
                  << drogon::app().getListeners()[0].toPort() << "\n";
    });

    app.run();
    return 0;
}
