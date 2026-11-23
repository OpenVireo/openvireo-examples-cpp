// Vaults: listing and opening, reading and writing fields, editing entries, creating, sharing,
// revoking and organisations.
//
// Guide: https://developer.openvireo.com/sdk/guides/vaults-and-sharing/
//
// Running it signs in, lists the vaults and the entries of the first one, and reads a field if you
// pass an ov:// reference. Nothing is written unless you pass --write, which adds an "OpenVireo
// example" entry to the first vault you can write to. The sharing and organisation functions are here
// to read and copy; the program doesn't call them.
//
//     build/examples vaults-and-sharing ["ov://Vault/Entry/field"] [--write]
#include "examples.h"

#include <algorithm>
#include <cstdio>
#include <stdexcept>

namespace examples {
namespace {

// Listing and opening
void list_and_open(const ov::Account &account, const ov::CancelToken &cancel, const std::string &reference) {
    auto vaults = ov::call([&] { return account.vaults(cancel); });   // id, name, role, org_id, ...
    for (const auto &info : vaults) std::printf("%s  %s\n", std::string(info.name).c_str(), std::string(info.id).c_str());
    if (vaults.empty()) return;

    auto vault = ov::call([&] { return account.open_vault(vaults[0].id, cancel); });   // downloads and decrypts it
    std::printf("Entries in %s at %s\n", std::string(ov::call([&] { return vault->name(); })).c_str(),
                std::string(ov::call([&] { return vault->etag(); })).c_str());   // the server version this copy is based on
    for (const auto &e : ov::call([&] { return vault->entries(false); }))   // vault->entry(id) gives the full record
        std::printf("  %s\n", std::string(e.title).c_str());
    if (!reference.empty()) {
        auto value = ov::call([&] { return vault->read_field(reference); });
        std::printf("%s = %s\n", reference.c_str(), text(value).c_str());
        ov::wipe(value);
    }
}

// Reading and writing fields
[[maybe_unused]] void read_and_write(const ov::Vault &vault, rust::Vec<std::uint8_t> key, const ov::CancelToken &cancel) {
    auto value = ov::call([&] { return vault.read_field("ov://Team/GitHub/password"); });
    ov::wipe(value);

    // edits and syncs; create = true creates the entry if needed
    ov::call([&] { return vault.write_field("ov://Team/GitHub/password", to_bytes("new value"), false, cancel); });
    ov::call([&] { return vault.write_field("ov://Team/Apps/Stripe/api_key", std::move(key), true, cancel); });
}

// Editing the database directly
void edit_entries(const ov::Vault &vault, const ov::CancelToken &cancel) {
    // set_fields is the short way to write fields by reference (the entry is created here if missing)
    auto reference = "ov://" + std::string(ov::call([&] { return vault.name(); })) + "/OpenVireo example";
    ov::call([&] {
        rust::Vec<ov::FieldInput> fields;
        fields.push_back(ov::FieldInput{"UserName", to_bytes("svc"), false, false});
        fields.push_back(ov::FieldInput{"token", to_bytes("..."), true, true});   // has_protected, is_protected
        return vault.set_fields(reference, std::move(fields), ov::CreateMode::CreateIfMissing);
    });

    auto result = ov::call([&] { return vault.sync(true, cancel); });
    // result.outcome: Uploaded, Pulled (only downloaded), Created or Unchanged; result.conflicts, result.changes
    std::printf("sync: outcome %d, %zu conflict(s)\n", static_cast<int>(result.outcome), result.conflicts.size());
}

// Creating vaults
[[maybe_unused]] void create_vaults(const ov::Account &account, const std::string &org_id, const ov::CancelToken &cancel) {
    ov::call([&] { return account.create_vault("Personal", "", cancel); });   // "": no org
    ov::call([&] { return account.create_vault("Team Infra", org_id, cancel); });   // admins get manage
}

void confirm_with_bob(const rust::String &fingerprint) {
    if (prompt("Does Bob's app show " + std::string(fingerprint) + "? [y/N] ") != "y")
        throw std::runtime_error("fingerprint not confirmed");
}

// Sharing
[[maybe_unused]] void share_by_email(const ov::Account &account, const std::string &vault_id, const ov::CancelToken &cancel) {
    ov::Grantee bob{ov::GranteeKind::Email, "bob@example.com"};
    auto grant = ov::call([&] { return account.share_vault(vault_id, bob, ov::VaultRole::Write, cancel); });
    confirm_with_bob(grant.fingerprint);   // compare it with what Bob's app shows, out of band

    ov::call([&] { account.change_role(vault_id, grant.principal, ov::VaultRole::Read, cancel); });
}

[[maybe_unused]] void share_with_verified_keys(const ov::Account &account, const std::string &vault_id, const ov::CancelToken &cancel) {
    auto bob = ov::call([&] { return account.look_up("bob@example.com", cancel); });
    const auto &k = bob.public_keys;
    auto slice = [](const rust::Vec<std::uint8_t> &v) { return rust::Slice<const std::uint8_t>(v.data(), v.size()); };
    if (!ov::verify_public_keys(slice(k.enc_pk), slice(k.sign_pk), slice(k.sig))) throw std::runtime_error("Bob's keys don't verify");
    confirm_with_bob(k.fingerprint);
    ov::Grantee grantee{ov::GranteeKind::Principal, bob.principal};
    ov::call([&] { return account.share_vault_with_keys(vault_id, grantee, ov::VaultRole::Write, k, cancel); });
}

// Revoking and rotating
[[maybe_unused]] void revoke_and_rotate(const ov::Account &account, const std::string &vault_id, const std::string &bob_principal,
                                        const ov::CancelToken &cancel) {
    ov::call([&] {
        rust::Vec<rust::String> principals;
        principals.push_back(bob_principal);
        return account.revoke(vault_id, std::move(principals), cancel);   // and rotates the vault key
    });
    ov::call([&] { return account.rotate_vault_key(vault_id, {}, cancel); });   // a new key without removing anyone
}

// Organisations
[[maybe_unused]] void organisations(const ov::Account &account, const std::string &vault_id, const ov::CancelToken &cancel) {
    auto org = ov::call([&] { return account.create_org("Acme", cancel); });
    ov::call([&] { return account.invite(org.id, "carol@example.com", ov::OrgRole::Member, cancel); });
    // Carol gets an email, and her app calls account->accept_invitation(token, *cancel)

    auto members = ov::call([&] { return account.org_members(org.id, cancel); });
    auto it = std::find_if(members.begin(), members.end(), [](const ov::Member &m) {
        return std::string(m.email) == "carol@example.com" && m.status == ov::MemberStatus::Accepted;
    });
    if (it == members.end()) return;
    const auto &carol = *it;
    ov::call([&] {
        rust::Vec<ov::VaultGrantRequest> grants;
        grants.push_back(ov::VaultGrantRequest{vault_id, ov::VaultRole::Read});
        return account.confirm_member(org.id, carol, std::move(grants), cancel);
    });

    ov::call([&] { return account.set_member_role(org.id, carol, ov::OrgRole::Admin, cancel); });   // manage everywhere
    auto report = ov::call([&] { return account.remove_member(org.id, carol.user_id, cancel); });  // rotates her vaults
    std::printf("rotated %zu, failed %zu\n", report.rotated.size(), report.failed.size());
}

}  // namespace

void vaults_and_sharing(const Args &args) {
    auto cancel = ov::new_cancel_token();
    auto client = make_client();
    auto account = sign_in(*client, *cancel);
    std::string reference;
    for (const auto &a : args)
        if (a.rfind("ov://", 0) == 0) reference = a;
    list_and_open(*account, *cancel, reference);
    if (std::find(args.begin(), args.end(), "--write") != args.end()) {
        for (const auto &info : ov::call([&] { return account->vaults(*cancel); })) {
            if (info.role == ov::VaultRole::Read) continue;
            edit_entries(*ov::call([&] { return account->open_vault(info.id, *cancel); }), *cancel);
            break;
        }
    }
    ov::call([&] { account->sign_out(false, *cancel); });
}

}  // namespace examples
