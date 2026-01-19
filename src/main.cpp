#include <adapters/ui/web/Server.hpp>

int main() {
    kaos::adapters::ui::web::Server server;
    server.run(8080);
    return 0;
}
