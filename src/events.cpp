// Live events: react when another device changes a vault, or when access changes.
//
// Guide: https://developer.openvireo.com/sdk/guides/events/
//
// Running it signs in, opens the first vault and prints events until the session ends (Ctrl-C to
// stop). Edit something in another OpenVireo app to see one.
//
//     build/examples events
#include "examples.h"

#include <cstdio>

namespace examples {
namespace {

void render(const rust::Vec<ov::VaultInfo> &vaults) {
    std::printf("Vaults now:");
    for (const auto &v : vaults) std::printf(" %s", std::string(v.name).c_str());
    std::printf("\n");
}

void watch(const ov::Account &account, const ov::Vault &vault, const ov::CancelToken &cancel) {
    auto stream = ov::call([&] { return account.subscribe(cancel); });
    for (;;) {
        auto event = ov::call([&] { return stream->next(cancel); });   // blocks
        if (event.kind == ov::AccountEventKind::Closed || event.kind == ov::AccountEventKind::SessionEnded) break;
        if (event.kind == ov::AccountEventKind::VaultChanged) {
            std::printf("Vault changed: %s\n", std::string(event.id).c_str());
            if (event.id == ov::call([&] { return vault.id(); }) && event.etag != ov::call([&] { return vault.etag(); }))
                ov::call([&] { return vault.sync(false, cancel); });   // another device uploaded this vault
        }
        if (event.kind == ov::AccountEventKind::VaultsChanged) render(event.vaults);   // access changed
    }
    ov::call([&] { stream->close(); });   // from another thread, ends a blocked next()
}

}  // namespace

void events(const Args &) {
    auto cancel = ov::new_cancel_token();
    auto client = make_client();
    auto account = sign_in(*client, *cancel);
    auto vaults = ov::call([&] { return account->vaults(*cancel); });
    if (vaults.empty()) {
        std::printf("No vaults to watch.\n");
    } else {
        std::printf("Watching for changes (Ctrl-C to stop)...\n");
        watch(*account, *ov::call([&] { return account->open_vault(vaults[0].id, *cancel); }), *cancel);
    }
    ov::call([&] { account->sign_out(false, *cancel); });
}

}  // namespace examples
