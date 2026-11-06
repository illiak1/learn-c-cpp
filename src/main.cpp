// main.cpp - entry point: parses options and runs the application.
#include "app/app.h"

// Program entry.
int main(int argc, char** argv) {
    app::App application(app::parseArgs(argc, argv));
    return application.run();
}
