#include "common.h"

#include "host.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>

namespace examples {

std::string issuer() {
    const char *v = std::getenv("OV_ISSUER");
    return v && *v ? v : "https://accounts.openvireo.com";
}

std::string env(const char *name) {
    const char *v = std::getenv(name);
    if (!v || !*v) {
        std::fprintf(stderr, "Set %s to run this example.\n", name);
        std::exit(1);
    }
    return v;
}

rust::Vec<std::uint8_t> to_bytes(const std::string &s) {
    rust::Vec<std::uint8_t> out;
    out.reserve(s.size());
    for (char c : s) out.push_back(static_cast<std::uint8_t>(c));
    return out;
}

std::string text(const rust::Vec<std::uint8_t> &bytes) {
    return std::string(reinterpret_cast<const char *>(bytes.data()), bytes.size());
}

void usage(const char *text) {
    std::fprintf(stderr, "%s\n", text);
    std::exit(2);
}

std::string prompt(const std::string &text) {
    std::cout << text << std::flush;
    std::string line;
    std::getline(std::cin, line);
    return line;
}

std::string ask_for_code() { return prompt("Authenticator code: "); }

rust::Box<ov::Client> make_client(const std::string &device_name, bool offline) {
    ov::ClientConfig config{"", device_name, ov::Platform::Cli, offline, false, ov::ServerConfig{}};
    return ov::call([&] {
        return ov::new_client(config, std::make_unique<CurlTransport>(), std::make_unique<MemoryStore>(),
                              std::make_unique<MemoryCache>(), std::make_unique<SystemClock>(), std::make_unique<StderrLog>());
    });
}

rust::Box<ov::Account> sign_in(const ov::Client &client, const ov::CancelToken &cancel) {
    auto account = ov::call([&] { return client.connect(issuer(), cancel); });
    const char *secret_key = std::getenv("OV_SECRET_KEY");
    bool has_secret_key = secret_key && *secret_key;
    ov::LoginInput input{env("OV_EMAIL"), to_bytes(env("OV_PASSWORD")), has_secret_key,
                         has_secret_key ? to_bytes(secret_key) : rust::Vec<std::uint8_t>{}, "", true};
    auto step = ov::call([&] { return account->login(std::move(input), cancel); });
    while (step->needs_totp()) {
        auto challenge = ov::call([&] { return step->challenge(); });
        step = ov::call([&] { return account->submit_totp(*challenge, ask_for_code(), cancel); });
    }
    return account;
}

void check(const Args &) {
    auto cancel = ov::new_cancel_token();
    auto client = make_client();
    auto account = ov::call([&] { return client->connect(issuer(), *cancel); });
    auto config = ov::call([&] { return account->server_config(); });
    std::printf("Connected to %s (API %s); signed in: %s\n", issuer().c_str(), std::string(config.api).c_str(),
                ov::call([&] { return account->is_signed_in(); }) ? "true" : "false");
}

}  // namespace examples
