// Hosted accounts: connecting, signing up and in, locking, the account itself, devices and deletion.
//
// Guide: https://developer.openvireo.com/sdk/guides/hosted-account/
//
// Running it signs in, prints the account and its devices, locks and unlocks, and signs out. The
// functions that change the account (sign_up, change_password, delete_account, ...) are here to read
// and copy; the program doesn't call them.
//
//     build/examples hosted-account
#include "examples.h"
#include "host.h"

#include <cstdio>
#include <memory>

namespace examples {
namespace {

// Connecting
rust::Box<ov::Account> connect(const ov::Client &client, const ov::CancelToken &cancel) {
    return ov::call([&] { return client.connect(issuer(), cancel); });
}

void show_emergency_kit(const rust::Vec<std::uint8_t> &secret_key) {
    std::printf("Your Secret Key (keep it safe): %s\n", text(secret_key).c_str());
}

// Signing up
[[maybe_unused]] void sign_up(const ov::Account &account, const std::string &email, rust::Vec<std::uint8_t> password,
                              const ov::CancelToken &cancel) {
    ov::RegisterInput input{email, std::move(password), ov::KdfOverrides{}, true};
    auto created = ov::call([&] { return account.register_account(std::move(input), cancel); });
    show_emergency_kit(created.secret_key);   // the only time the Secret Key is shown; the user must keep it
}

// Signing in
void sign_in(const ov::Account &account, const std::string &email, rust::Vec<std::uint8_t> password,
             rust::Vec<std::uint8_t> secret_key, const ov::CancelToken &cancel) {
    // has_secret_key = false (and an empty secret_key) once this device trusts the account
    bool has_secret_key = !secret_key.empty();
    ov::LoginInput input{email, std::move(password), has_secret_key, std::move(secret_key), "", true};
    auto step = ov::call([&] { return account.login(std::move(input), cancel); });
    while (step->needs_totp()) {   // two-factor authentication is on
        auto challenge = ov::call([&] { return step->challenge(); });
        step = ov::call([&] { return account.submit_totp(*challenge, ask_for_code(), cancel); });
    }
}

// Unlocking and locking
void lock_and_unlock(const ov::Account &account, rust::Vec<std::uint8_t> password, const ov::CancelToken &cancel) {
    ov::call([&] { account.lock(); });   // wipes the keys from memory; the session stays
    if (ov::call([&] { return account.is_signed_in() && !account.is_unlocked(); }))
        ov::call([&] { return account.unlock(std::move(password), {}, cancel); });   // empty: the kept Secret Key
}

// Staying unlocked across a restart
[[maybe_unused]] void stay_unlocked(const ov::Account &account, const ov::Account &reloaded, const ov::CancelToken &cancel) {
    auto secret = ov::call([&] { return account.unlocked_keyset_secret(); });   // keep it in the OS keychain
    // after a restart, on a connected (signed-in, locked) account
    ov::call([&] { reloaded.resume_unlocked(std::move(secret), cancel); });
}

[[maybe_unused]] void watch_for_sign_out(const ov::Account &account, const ov::CancelToken &cancel) {
    auto reason = ov::call([&] { return account.signed_out(cancel); });   // blocks until the session ends
    std::printf("Signed out (%d)\n", static_cast<int>(reason));
}

// The account itself
void show_account(const ov::Account &account) {
    std::printf("email: %s\n", std::string(ov::call([&] { return account.email(); })).c_str());
    std::printf("user id: %s\n", std::string(ov::call([&] { return account.user_id(); })).c_str());
    // this account's public key fingerprint, for others to compare
    std::printf("fingerprint: %s\n", std::string(ov::call([&] { return account.fingerprint(); })).c_str());
}

[[maybe_unused]] void change_password(const ov::Account &account, rust::Vec<std::uint8_t> current, rust::Vec<std::uint8_t> next,
                                      const ov::CancelToken &cancel) {
    ov::call([&] { return account.change_password(std::move(current), std::move(next), ov::KdfOverrides{}, cancel); });   // signs others out
}

[[maybe_unused]] void change_email(const ov::Account &account, const std::string &new_email, rust::Vec<std::uint8_t> password,
                                   const ov::CancelToken &cancel) {
    ov::call([&] { account.request_email_change(new_email, cancel); });   // mails a code to the new address
    auto code = prompt("Code sent to " + new_email + ": ");
    ov::call([&] { account.change_email(new_email, code, std::move(password), cancel); });
}

[[maybe_unused]] std::string account_website_link(const ov::Account &account, const ov::CancelToken &cancel) {
    return text(ov::call([&] { return account.web_session_url("/settings", cancel); }));   // one-time link (60 s)
}

// Devices
void list_devices(const ov::Account &account, const ov::CancelToken &cancel) {
    for (const auto &d : ov::call([&] { return account.devices(cancel); }))   // name, platform, last_seen_at, current, ...
        std::printf("%s %s %s%s\n", std::string(d.name).c_str(), std::string(d.platform).c_str(),
                    std::string(d.last_seen_at).c_str(), d.current ? " (this device)" : "");
}

[[maybe_unused]] void revoke_device(const ov::Account &account, const std::string &name, const ov::CancelToken &cancel) {
    for (const auto &d : ov::call([&] { return account.devices(cancel); }))
        if (!d.current && std::string(d.name) == name) ov::call([&] { account.revoke_device(d.id, cancel); });
}

// Deleting the account
[[maybe_unused]] void delete_account(const ov::Account &account, rust::Vec<std::uint8_t> password, const std::string &typed_email,
                                     const std::string &heir, const ov::CancelToken &cancel) {
    auto preview = ov::call([&] { return account.deletion_preview(cancel); });
    // preview.erase_on: when it would be erased; preview.vaults: id, name, shared_with
    std::printf("would be erased on %s\n", std::string(preview.erase_on).c_str());
    if (!heir.empty() && !preview.vaults.empty() && !preview.vaults[0].shared_with.empty())
        ov::call([&] { account.transfer_vault(preview.vaults[0].id, heir, cancel); });

    rust::Vec<std::uint8_t> again;
    for (auto b : password) again.push_back(b);
    auto scheduled_for = ov::call([&] { return account.delete_account(std::move(password), typed_email, cancel); });
    std::printf("scheduled for %s\n", std::string(scheduled_for).c_str());
    // This device is signed out and its offline cache is gone. The Secret Key stays, so after signing in again:
    if (!ov::call([&] { return account.deletion_scheduled_for(); }).empty())
        ov::call([&] { account.restore_account(std::move(again), cancel); });
}

}  // namespace

void hosted_account(const Args &) {
    auto cancel = ov::new_cancel_token();
    auto client = make_client("Linux desktop");
    auto account = connect(*client, *cancel);
    sign_in(*account, env("OV_EMAIL"), to_bytes(env("OV_PASSWORD")), to_bytes(env("OV_SECRET_KEY")), *cancel);
    show_account(*account);
    list_devices(*account, *cancel);
    lock_and_unlock(*account, to_bytes(env("OV_PASSWORD")), *cancel);
    ov::call([&] { account->sign_out(false, *cancel); });   // true also forgets the trusted Secret Key
}

}  // namespace examples
