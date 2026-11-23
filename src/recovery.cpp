// Account recovery: resetting with a recovery link, and restoring the old vaults afterwards.
//
// Guide: https://developer.openvireo.com/sdk/guides/recovery/
//
//     build/examples recovery info <token>   # what a reset would do; works signed out
//     build/examples recovery status         # after a reset: is there an archive to restore?
//
// reset() and restore() change the account, so they only run when you call them yourself.
#include "examples.h"

#include <cstdio>

namespace examples {
namespace {

// Resetting
void info(const ov::Account &account, rust::Vec<std::uint8_t> token, const ov::CancelToken &cancel) {
    auto info = ov::call([&] { return account.recover_info(std::move(token), cancel); });   // works signed out
    std::printf("email %s, expires %s\n", std::string(info.email).c_str(), std::string(info.expires_at).c_str());
}

[[maybe_unused]] void reset(const ov::Account &account, rust::Vec<std::uint8_t> token, rust::Vec<std::uint8_t> password,
                            const ov::CancelToken &cancel) {
    auto created = ov::call([&] { return account.recover(std::move(token), std::move(password), ov::KdfOverrides{}, true, cancel); });
    std::printf("Your new Secret Key (keep it safe): %s\n", text(created.secret_key).c_str());   // signed in and unlocked
}

// Restoring the old vaults
[[maybe_unused]] void restore(const ov::Account &account, rust::Vec<std::uint8_t> old_password, rust::Vec<std::uint8_t> old_secret_key,
                              const ov::CancelToken &cancel) {
    auto status = ov::call([&] { return account.recovery_status(cancel); });
    if (status.archived && !status.locked)
        ov::call([&] { return account.restore_from_archive(std::move(old_password), std::move(old_secret_key), cancel); });
}

}  // namespace

void recovery(const Args &args) {
    auto cancel = ov::new_cancel_token();
    auto client = make_client();
    if (args.size() == 2 && args[0] == "info") {
        info(*ov::call([&] { return client->connect(issuer(), *cancel); }), to_bytes(args[1]), *cancel);
    } else if (args.size() == 1 && args[0] == "status") {
        auto account = sign_in(*client, *cancel);
        auto status = ov::call([&] { return account->recovery_status(*cancel); });
        std::printf("archived: %s, locked: %s\n", status.archived ? "yes" : "no", status.locked ? "yes" : "no");
        ov::call([&] { account->sign_out(false, *cancel); });
    } else {
        usage("Usage: recovery info <token> | recovery status");
    }
}

}  // namespace examples
