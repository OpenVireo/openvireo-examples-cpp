// Runs one example: build/examples <example> [args]
#include "examples.h"

#include <cstdio>
#include <exception>
#include <string>
#include <utility>

int main(int argc, char **argv) {
    using namespace examples;
    const std::pair<const char *, void (*)(const Args &)> all[] = {
        {"first-program", first_program},
        {"hosted-account", hosted_account},
        {"vaults-and-sharing", vaults_and_sharing},
        {"events", events},
        {"offline", offline},
        {"move-to-account", move_to_account},
        {"recovery", recovery},
        {"self-hosted-sync", self_hosted_sync},
        {"service-accounts", service_accounts},
        {"share-links", share_links},
        {"check", check},
    };
    if (argc > 1) {
        Args args(argv + 2, argv + argc);
        for (const auto &e : all) {
            if (std::string(argv[1]) != e.first) continue;
            try {
                e.second(args);
                return 0;
            } catch (const ov::Error &err) {
                std::fprintf(stderr, "error: %s: %s\n", err.code().c_str(), err.what());
            } catch (const std::exception &err) {
                std::fprintf(stderr, "error: %s\n", err.what());
            }
            return 1;
        }
    }
    std::fprintf(stderr, "Usage: build/examples <example> [args]\nExamples:");
    for (const auto &e : all) std::fprintf(stderr, " %s", e.first);
    std::fprintf(stderr, "\n");
    return 2;
}
