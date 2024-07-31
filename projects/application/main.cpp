#include "app.hpp"

int main() {
    app_t *app = new app_t;
    try {
        app->run();
    } catch (std::exception e) {
        throw e;
    }

    delete app;

    return 0;
}