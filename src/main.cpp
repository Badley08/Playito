// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/application.h"

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try {
        playito::Application application(argc, argv);
        return application.run();
    } catch (const std::exception& error) {
        std::cerr << "Playito: " << error.what() << '\n';
        return 1;
    }
}
