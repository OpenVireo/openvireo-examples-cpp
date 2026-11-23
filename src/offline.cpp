// Offline: edit from the cached copy while the servers can't be reached, and merge the edits in when
// back online.
//
// Guide: https://developer.openvireo.com/sdk/guides/offline/
//
// To unlock offline after a restart, the Host needs a persistent SecureStore (the OS keychain) and
// BlobCache (files). The examples' Host keeps both in memory, so this program signs in (online) and
// then edits, keeping the edit if the network drops.
//
//     build/examples offline "ov://Home/Wifi/password" "new value"
#include "examples.h"

#include <cstdio>

namespace examples {

void offline(const Args &args) {
    if (args.size() != 2) usage("Usage: offline <ov://Vault/Entry/field> <value>");
    const auto &reference = args[0];
    auto cancel = ov::new_cancel_token();
    auto client = make_client("Laptop", true);   // offline = true; it needs a Host with a persistent BlobCache
    auto account = sign_in(*client, *cancel);    // with one: connect, then account->unlock(std::move(password), {}, *cancel)
    auto vault_name = reference.substr(5, reference.find('/', 5) - 5);
    std::string vault_id;
    for (const auto &info : ov::call([&] { return account->vaults(*cancel); }))
        if (std::string(info.name) == vault_name) vault_id = std::string(info.id);
    if (vault_id.empty()) usage("No vault by that name");
    auto vault = ov::call([&] { return account->open_vault(vault_id, *cancel); });   // offline: the local copy
    std::printf("offline: %s\n", ov::call([&] { return account->is_offline(); }) ? "true" : "false");

    try {
        ov::call([&] { return vault->write_field(reference, to_bytes(args[1]), false, *cancel); });
    } catch (const ov::Error &e) {
        if (e.code() != "network_error") throw;   // offline: the edit is kept and merges in later
        std::printf("Saved locally; it uploads when you're back online.\n");
    }

    if (ov::call([&] { return account->is_offline(); })) {
        auto result = ov::call([&] { return account->reconnect(*cancel); });   // result.dropped: revoked vaults
        std::printf("reconnected; dropped %zu vault(s)\n", result.dropped.size());
        ov::call([&] { return vault->sync(true, *cancel); });   // uploads the kept edit
    }
    ov::call([&] { account->sign_out(false, *cancel); });
}

}  // namespace examples
