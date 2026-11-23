// Service accounts: create one for CI, then read, inject and write secrets with its token.
//
// Guide: https://developer.openvireo.com/sdk/guides/service-accounts/
//
//     OPENVIREO_SERVICE_ACCOUNT_TOKEN=... build/examples service-accounts "ov://Team Infra/CI/token"
//
// create() needs an organisation owner or admin signed in, so the program doesn't call it. Set ROTATE
// to a new value to also write it (needs the write role).
#include "examples.h"

#include <cstdio>
#include <cstdlib>

namespace examples {
namespace {

// Creating one
[[maybe_unused]] void create(const ov::Account &account, const std::string &org_id, const std::string &vault_id,
                             const ov::CancelToken &cancel) {
    rust::Vec<ov::VaultGrantRequest> vaults;
    vaults.push_back(ov::VaultGrantRequest{vault_id, ov::VaultRole::Read});
    ov::NewServiceAccount input{"ci", std::move(vaults), "2027-01-01T00:00:00Z", ""};
    auto created = ov::call([&] { return account.create_service_account(org_id, input, cancel); });
    std::printf("%s\n", text(created.token).c_str());   // show it once; store it as a CI secret

    ov::call([&] { return account.issue_service_account_secret(created.service_account.id, "staging", "", cancel); });
}

}  // namespace

// Reading and writing
void service_accounts(const Args &args) {
    if (args.size() != 1) usage("Usage: service-accounts <ov://Vault/Entry/field>");
    const auto &reference = args[0];
    auto cancel = ov::new_cancel_token();
    auto client = make_client("CI");
    auto sa = ov::call([&] {
        return ov::service_account_from_token(*client, to_bytes(env("OPENVIREO_SERVICE_ACCOUNT_TOKEN")), *cancel);
    });
    auto password = ov::call([&] { return sa->read(reference, *cancel); });
    std::printf("%s: %zu bytes\n", reference.c_str(), password.size());
    ov::wipe(password);

    std::string encoded;
    for (char c : reference) encoded += c == ' ' ? std::string("%20") : std::string(1, c);
    auto injected = ov::call([&] { return sa->inject(to_bytes("TOKEN=" + encoded + "\n"), *cancel); });   // mask injected.secrets
    std::printf("injected %zu secret(s)\n", injected.secrets.size());

    if (const char *rotated = std::getenv("ROTATE"))
        ov::call([&] { sa->write(reference, to_bytes(rotated), false, *cancel); });   // needs the write role
    ov::call([&] { sa->close(); });   // wipes the keys from memory
}

}  // namespace examples
