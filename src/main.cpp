#include "first_app.h"

#include "horizon_utils.h"

int main()
{
    horizon::FirstApp app{};
    try
    {
        app.run();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}