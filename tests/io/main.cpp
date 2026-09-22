// Test runner for nmeasim::io. Transports need a Qt event loop, so a QCoreApplication is
// created before Catch2 runs the test cases.
#include <QCoreApplication>

#include <catch2/catch_session.hpp>

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    return Catch::Session().run(argc, argv);
}
