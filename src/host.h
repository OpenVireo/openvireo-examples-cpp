// The core does no I/O: the Host is the platform's part. This one is enough for a command-line
// program: libcurl for HTTP, and memory for the secure store and the offline cache (so every run is a
// new device). A desktop app keeps the store in the OS keychain and the cache on disk.
#pragma once

#include <ovcore.h>

#include <memory>

namespace examples {

// HTTP on libcurl. Two rules apply to every transport: it must not follow redirects, and it must not
// send or store cookies. Every status is a response; only a request that got no answer at all is a
// TransportFailure.
class CurlTransport : public ov::HttpTransport {
public:
    CurlTransport();
    ~CurlTransport() override;
    ov::HttpResponse send(const ov::HttpRequest &req, ov::TransportFailure &failure) const override;
    std::unique_ptr<ov::ByteStream> open_stream(const ov::HttpRequest &req, ov::TransportFailure &failure) const override;
};

// Sessions and trusted-device keys, in memory. A desktop app keeps them in the OS keychain.
class MemoryStore : public ov::SecureStore {
public:
    MemoryStore();
    ~MemoryStore() override;
    bool get(rust::Str key, rust::Vec<std::uint8_t> &value) const override;
    void set(rust::Str key, rust::Slice<const std::uint8_t> value) const override;
    void remove(rust::Str key) const override;

private:
    struct State;
    std::unique_ptr<State> state_;
};

// The offline copies, in memory. A desktop app keeps them on disk so it can open vaults offline.
class MemoryCache : public ov::BlobCache {
public:
    MemoryCache();
    ~MemoryCache() override;
    bool load(rust::Str user_id, rust::Str vault_id, ov::CachedVault &out) const override;
    void save(rust::Str user_id, rust::Str vault_id, const ov::CachedVault &v) const override;
    void remove(rust::Str user_id, rust::Str vault_id) const override;
    rust::Vec<rust::String> list(rust::Str user_id) const override;
    bool load_meta(rust::Str user_id, rust::Str name, rust::Vec<std::uint8_t> &value) const override;
    void save_meta(rust::Str user_id, rust::Str name, rust::Slice<const std::uint8_t> value) const override;
    void clear_user(rust::Str user_id) const override;

private:
    struct State;
    std::unique_ptr<State> state_;
};

class SystemClock : public ov::Clock {
public:
    std::int64_t now_unix_ms() const override;
};

// Warnings and errors to stderr; OV_LOG=debug for more.
class StderrLog : public ov::LogSink {
public:
    void log(ov::LogLevel level, rust::Str target, rust::Str msg) const override;
};

}  // namespace examples
