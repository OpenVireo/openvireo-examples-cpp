// A first program: sign in, list the vaults, read one field and sign out.
//
// Guide: https://developer.openvireo.com/sdk/#a-first-program
//
//     build/examples first-program "ov://Personal/GitHub/username"
#include "examples.h"
#include "host.h"

#include <cstdio>
#include <memory>

namespace examples {

void first_program(const Args &args) {
    auto email = env("OV_EMAIL");
    auto password = to_bytes(env("OV_PASSWORD"));
    auto secret_key = to_bytes(env("OV_SECRET_KEY"));

    // Calls block: a GUI app runs them on a worker thread. The Host classes (host.h) implement <host.h>.
    auto cancel = ov::new_cancel_token();
    ov::ClientConfig config{"", "My tool", ov::Platform::Cli, false, false, ov::ServerConfig{}};
    auto client = ov::call([&] {
        return ov::new_client(config, std::make_unique<CurlTransport>(), std::make_unique<MemoryStore>(),
                              std::make_unique<MemoryCache>(), std::make_unique<SystemClock>(), std::make_unique<StderrLog>());
    });
    auto account = ov::call([&] { return client->connect(issuer(), *cancel); });
    // A LoginStep that needs a TOTP code when two-step sign-in is on: see hosted_account.cpp
    ov::LoginInput input{email, std::move(password), true, std::move(secret_key), "", true};
    ov::call([&] { return account->login(std::move(input), *cancel); });

    auto vaults = ov::call([&] { return account->vaults(*cancel); });
    for (const auto &info : vaults) std::printf("%s\n", std::string(info.name).c_str());
    if (!vaults.empty() && !args.empty()) {
        auto vault = ov::call([&] { return account->open_vault(vaults[0].id, *cancel); });
        auto value = ov::call([&] { return vault->read_field(args[0]); });
        std::printf("%.*s\n", static_cast<int>(value.size()), reinterpret_cast<const char *>(value.data()));
        ov::wipe(value);
    }
    ov::call([&] { account->sign_out(false, *cancel); });
}

}  // namespace examples
