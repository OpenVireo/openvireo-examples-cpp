// Share links: share one item, or some text, through a link that needs no account; list and revoke
// links; open one.
//
// Guide: https://developer.openvireo.com/sdk/guides/share-links/
//
//     build/examples share-links list
//     build/examples share-links text "The alarm code is 4321"
//     build/examples share-links open "https://share.openvireo.com/<id>#<key>"
#include "examples.h"

#include <chrono>
#include <cstdio>

namespace examples {
namespace {

// Making a link
[[maybe_unused]] std::string share_entry(const ov::Account &account, const ov::Vault &vault, const std::string &entry_id,
                                         const ov::CancelToken &cancel) {
    auto entry = ov::call([&] { return vault.entry(entry_id); });
    auto choices = ov::call([&] { return ov::shareable_fields(entry); });   // what can be shared, in display order
    std::printf("%zu field(s) can be shared\n", choices.size());

    auto payload = ov::call([&] { return ov::item_share_payload(entry, ov::default_share_fields(), "Sam"); });
    ov::ShareOptions options{7 * 86400, true, 1, false, {}, ov::call([&] { return vault.id(); }), entry_id};   // expires_in, max_opens, passcode, ...
    auto link = ov::call([&] { return account.create_share(std::move(payload), std::move(options), cancel); });
    return text(link.url);   // https://share.openvireo.com/<id>#<key> (bytes: it holds the key)
}

std::string share_text(const ov::Account &account, const std::string &message, const ov::CancelToken &cancel) {
    auto payload = ov::call([&] { return ov::text_share_payload(to_bytes(message), "From the examples", ""); });
    ov::ShareOptions options{3600, false, 0, true, to_bytes("tiger lily 42"), "", ""};
    auto link = ov::call([&] { return account.create_share(std::move(payload), std::move(options), cancel); });
    return text(link.url);
}

// Listing and revoking
void list_shares(const ov::Account &account, const ov::CancelToken &cancel) {
    for (const auto &s : ov::call([&] { return account.list_shares(cancel); }))
        std::printf("%s %s %u\n", std::string(s.id).c_str(), std::string(s.status).c_str(), s.opens);
}

[[maybe_unused]] void revoke(const ov::Account &account, const std::string &share_id, const ov::CancelToken &cancel) {
    ov::call([&] { account.revoke_share(share_id, cancel); });
}

[[maybe_unused]] void org_policy(const ov::Account &account, const std::string &org_id, const ov::CancelToken &cancel) {
    ov::call([&] { return account.set_org_share_links(org_id, ov::ShareLinkPolicy::Managers, cancel); });
}

// Opening a link
void open_link(const ov::Client &client, const std::string &api, const std::string &link, const ov::CancelToken &cancel) {
    // The share id is the last path segment, before the '#key'. (ov::parse_share_link does this from the
    // next core release on; 1.0.0's C++ package doesn't have it yet.)
    auto path = link.substr(0, link.find('#'));
    auto share_id = path.substr(path.find_last_of('/') + 1);
    auto info = ov::call([&] { return client.share_info(api, share_id, cancel); });   // doesn't count as an open
    // Show a "Reveal" button and open only when it's pressed: link previews then can't use up a one-time link.
    prompt("Press Enter to reveal... ");
    ov::OpenShareInput input{to_bytes(link), false, {}, true, info};
    if (info.has_kdf) {
        input.has_passcode = true;
        input.passcode = to_bytes(prompt("Passcode: "));
    }
    auto opened = ov::call([&] { return client.open_share(api, std::move(input), cancel); });

    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    for (const auto &f : opened.payload.fields) {
        // hide f.is_protected values until shown
        auto shown = std::string(f.k) == "totp"
            ? text(ov::call([&] { return ov::totp_code(ov::parse_totp_uri(f.value), now_ms); }).code)
            : text(f.value);
        std::printf("%s: %s\n", std::string(f.label).c_str(), shown.c_str());
    }
}

}  // namespace

void share_links(const Args &args) {
    auto cancel = ov::new_cancel_token();
    auto client = make_client();
    auto account = sign_in(*client, *cancel);
    if (args.size() == 1 && args[0] == "list") {
        list_shares(*account, *cancel);
    } else if (args.size() == 2 && args[0] == "text") {
        std::printf("%s\npasscode: tiger lily 42\n", share_text(*account, args[1], *cancel).c_str());
    } else if (args.size() == 2 && args[0] == "open") {
        auto api = ov::call([&] { return account->server_config(); }).api;   // or the share host's API, without an account
        open_link(*client, std::string(api), args[1], *cancel);
    } else {
        usage("Usage: share-links list | text <text> | open <link>");
    }
    ov::call([&] { account->sign_out(false, *cancel); });
}

}  // namespace examples
