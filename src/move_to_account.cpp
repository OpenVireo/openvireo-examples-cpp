// Moving a .kdbx file into an account: as a new vault, or merged into a vault you already have.
//
// Guide: https://developer.openvireo.com/sdk/guides/move-to-account/
//
//     build/examples move-to-account Passwords.kdbx            # a new vault named after the file
//     build/examples move-to-account Passwords.kdbx Personal   # merge into the vault "Personal"
//
// The file's password is read from KDBX_PASSWORD.
#include "examples.h"

#include <cstdio>
#include <fstream>
#include <iterator>

namespace examples {
namespace {

rust::Vec<std::uint8_t> read_file(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) usage("Can't read the file");
    return to_bytes(std::string(std::istreambuf_iterator<char>(in), {}));
}

void import_as_new_vault(const ov::Account &account, rust::Vec<std::uint8_t> file_bytes, rust::Vec<std::uint8_t> password,
                         const std::string &name, const ov::CancelToken &cancel) {
    ov::KdbxCredentials creds{true, std::move(password), false, {}, false, {}};
    ov::ImportMode mode{ov::ImportModeKind::NewVault, name, "", ""};   // kind, name, org_id, vault_id
    auto outcome = ov::call([&] { return account.import_kdbx(std::move(file_bytes), std::move(creds), mode, cancel); });
    // outcome.uploaded is false when the first upload failed: sync the vault again later
    std::printf("Imported%s\n", outcome.uploaded ? "" : " (not uploaded yet: sync again later)");
}

void merge_into(const ov::Client &client, const ov::Vault &vault, rust::Vec<std::uint8_t> file_bytes,
                rust::Vec<std::uint8_t> password, const ov::CancelToken &cancel) {
    ov::KdbxCredentials creds{true, std::move(password), false, {}, false, {}};
    auto file = ov::call([&] { return client.open_local(std::move(file_bytes), std::move(creds), cancel); });
    ov::LocalImportMode merge{ov::LocalImportKind::Merge, ""};   // or {Copies, group_id}: vault.import_targets() lists them
    auto summary = ov::call([&] { return vault.import_from(*file, merge); });   // added, updated, unchanged
    ov::call([&] { return vault.sync(true, cancel); });
    std::printf("added %u, updated %u, unchanged %u\n", summary.added, summary.updated, summary.unchanged);
}

}  // namespace

void move_to_account(const Args &args) {
    if (args.empty() || args.size() > 2) usage("Usage: move-to-account <file.kdbx> [vault name]");
    auto cancel = ov::new_cancel_token();
    auto client = make_client();
    auto account = sign_in(*client, *cancel);
    auto file_bytes = read_file(args[0]);
    auto password = to_bytes(env("KDBX_PASSWORD"));
    if (args.size() == 1) {
        auto name = args[0].substr(args[0].find_last_of("/\\") + 1);
        if (name.size() > 5 && name.compare(name.size() - 5, 5, ".kdbx") == 0) name.resize(name.size() - 5);
        import_as_new_vault(*account, std::move(file_bytes), std::move(password), name, *cancel);
    } else {
        for (const auto &info : ov::call([&] { return account->vaults(*cancel); })) {
            if (std::string(info.name) != args[1]) continue;
            merge_into(*client, *ov::call([&] { return account->open_vault(info.id, *cancel); }), std::move(file_bytes),
                       std::move(password), *cancel);
            break;
        }
    }
    ov::call([&] { account->sign_out(false, *cancel); });
}

}  // namespace examples
