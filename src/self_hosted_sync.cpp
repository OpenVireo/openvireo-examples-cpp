// A self-hosted sync server: pairing a device, syncing a .kdbx file, and the other calls.
//
// Guide: https://developer.openvireo.com/sdk/guides/self-hosted-sync/
//
//     build/examples self-hosted-sync https://sync.example.org ABCD-EFGH personal Passwords.kdbx
//
// Pairs with the server, syncs the file (password from KDBX_PASSWORD; the merged file is written back
// to the same path) and shows the other calls. The examples' Host keeps the pairing in memory; with a
// persistent SecureStore, self_hosted_restore finds it again next run.
#include "examples.h"

#include <cstdio>
#include <fstream>
#include <iterator>

namespace examples {

void self_hosted_sync(const Args &args) {
    if (args.size() != 4) usage("Usage: self-hosted-sync <server url> <pairing code> <database id> <file.kdbx>");
    const auto &url = args[0];
    auto cancel = ov::new_cancel_token();
    auto client = make_client("Desktop");

    // Pairing: next time, with the same persistent store, self_hosted_restore finds it again
    auto restored = ov::call([&] { return ov::self_hosted_restore(*client, url); });
    auto server = restored->is_paired()
        ? ov::call([&] { return restored->take(); })
        : ov::call([&] { return ov::self_hosted_pair(*client, url, args[1], "Desktop", *cancel); });

    // Syncing a database
    std::ifstream in(args[3], std::ios::binary);
    if (!in) usage("Can't read the file");
    auto file_bytes = to_bytes(std::string(std::istreambuf_iterator<char>(in), {}));
    in.close();
    ov::KdbxCredentials creds{true, to_bytes(env("KDBX_PASSWORD")), false, {}, false, {}};
    auto result = ov::call([&] { return server->sync_database(args[2], std::move(file_bytes), std::move(creds), *cancel); });
    // result.kdbx is the merged file: save it locally and reopen it (result.report says what changed)
    std::ofstream(args[3], std::ios::binary).write(reinterpret_cast<const char *>(result.kdbx.data()),
                                                   static_cast<std::streamsize>(result.kdbx.size()));

    // Other calls
    std::printf("%s\n", std::string(ov::call([&] { return server->health(*cancel); })).c_str());   // '{"status":"ok"}'
    for (const auto &db : ov::call([&] { return server->list_databases(*cancel); }))   // id, revision, etag, size, updated_at
        std::printf("%s r%llu %llu bytes %s\n", std::string(db.id).c_str(), static_cast<unsigned long long>(db.revision),
                    static_cast<unsigned long long>(db.size), std::string(db.updated_at).c_str());
    auto code = ov::call([&] { return server->create_pairing_code(*cancel); }).code;   // for the next device
    std::printf("pairing code for the next device: %s\n", std::string(code).c_str());
    auto stream = ov::call([&] { return server->events(*cancel); });   // next() gives DatabaseChanged
    ov::call([&] { stream->close(); });
}

}  // namespace examples
