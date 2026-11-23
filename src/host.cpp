#include "host.h"

#include <curl/curl.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace examples {
namespace {

std::string str(rust::Str s) { return std::string(s.data(), s.size()); }

const char *method_name(ov::Method m) {
    switch (m) {
    case ov::Method::Get: return "GET";
    case ov::Method::Head: return "HEAD";
    case ov::Method::Post: return "POST";
    case ov::Method::Put: return "PUT";
    case ov::Method::Patch: return "PATCH";
    case ov::Method::Delete: return "DELETE";
    }
    return "GET";
}

ov::TransportErrorKind failure_kind(CURLcode code) {
    switch (code) {
    case CURLE_COULDNT_RESOLVE_HOST:
    case CURLE_COULDNT_RESOLVE_PROXY:
    case CURLE_COULDNT_CONNECT:
    case CURLE_SEND_ERROR:
    case CURLE_RECV_ERROR:
        return ov::TransportErrorKind::Offline;
    case CURLE_OPERATION_TIMEDOUT:
        return ov::TransportErrorKind::Timeout;
    case CURLE_SSL_CONNECT_ERROR:
    case CURLE_PEER_FAILED_VERIFICATION:
    case CURLE_SSL_CERTPROBLEM:
    case CURLE_SSL_CACERT_BADFILE:
        return ov::TransportErrorKind::Tls;
    case CURLE_ABORTED_BY_CALLBACK:
    case CURLE_WRITE_ERROR:
        return ov::TransportErrorKind::Cancelled;
    default:
        return ov::TransportErrorKind::Other;
    }
}

// One request's easy handle and header list. No redirects, no cookie engine (CURLOPT_COOKIEFILE unset).
struct Request {
    CURL *curl = curl_easy_init();
    curl_slist *headers = nullptr;
    std::vector<ov::Header> response_headers;

    explicit Request(const ov::HttpRequest &req) {
        curl_easy_setopt(curl, CURLOPT_URL, std::string(req.url).c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method_name(req.method));
        if (req.method == ov::Method::Head) curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        // curl adds "Expect: 100-continue" and a default content type on its own; the core sets its own headers
        headers = curl_slist_append(headers, "Expect:");
        bool has_content_type = false;
        for (const auto &h : req.headers) {
            std::string name(h.name);
            if (name.size() == 12 && curl_strnequal(name.c_str(), "content-type", 12)) has_content_type = true;
            headers = curl_slist_append(headers, (name + ": " + std::string(h.value)).c_str());
        }
        if (!has_content_type) headers = curl_slist_append(headers, "Content-Type:");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        if (req.has_body) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, reinterpret_cast<const char *>(req.body.data()));
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(req.body.size()));
        }
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &Request::on_header);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
    }

    ~Request() {
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    std::uint16_t status() const {
        long code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        return static_cast<std::uint16_t>(code);
    }

    static size_t on_header(char *data, size_t size, size_t n, void *self) {
        auto *r = static_cast<Request *>(self);
        std::string line(data, size * n);
        if (line.rfind("HTTP/", 0) == 0) r->response_headers.clear();   // a new status line (after a 1xx)
        auto colon = line.find(':');
        if (colon != std::string::npos) {
            auto value = line.substr(colon + 1);
            auto start = value.find_first_not_of(" \t");
            auto end = value.find_last_not_of(" \t\r\n");
            value = start == std::string::npos ? "" : value.substr(start, end - start + 1);
            r->response_headers.push_back(ov::Header{line.substr(0, colon), value});
        }
        return size * n;
    }
};

size_t collect(char *data, size_t size, size_t n, void *out) {
    auto *body = static_cast<rust::Vec<std::uint8_t> *>(out);
    for (size_t i = 0; i < size * n; i++) body->push_back(static_cast<std::uint8_t>(data[i]));
    return size * n;
}

// An open event stream: curl runs on its own thread and hands the bytes over through a queue.
class CurlStream : public ov::ByteStream {
public:
    explicit CurlStream(const ov::HttpRequest &req) : request_(req) {
        curl_easy_setopt(request_.curl, CURLOPT_WRITEFUNCTION, &CurlStream::on_data);
        curl_easy_setopt(request_.curl, CURLOPT_WRITEDATA, this);
        worker_ = std::thread([this] {
            CURLcode code = curl_easy_perform(request_.curl);
            std::lock_guard<std::mutex> lock(mutex_);
            result_ = code;
            done_ = true;
            started_ = true;
            cv_.notify_all();
        });
    }

    ~CurlStream() override {
        close();
        worker_.join();
    }

    // Waits for the status line, or for the request to fail before one came.
    bool wait_started(ov::TransportFailure &failure) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return started_; });
        if (done_ && result_ != CURLE_OK && queue_.empty()) {
            failure.kind = failure_kind(result_);
            failure.message = curl_easy_strerror(result_);
            return false;
        }
        status_ = request_.status();
        return true;
    }

    std::uint16_t status() const override { return status_; }

    bool next_chunk(rust::Vec<std::uint8_t> &chunk, ov::TransportFailure &failure) const override {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty() || done_ || closed_; });
        if (!queue_.empty()) {
            for (auto b : queue_.front()) chunk.push_back(b);
            queue_.pop_front();
            return true;
        }
        if (done_ && !closed_ && result_ != CURLE_OK) {
            failure.kind = failure_kind(result_);
            failure.message = curl_easy_strerror(result_);
        }
        return false;   // the end of the stream
    }

    void close() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        cv_.notify_all();
    }

private:
    static size_t on_data(char *data, size_t size, size_t n, void *self) {
        auto *s = static_cast<CurlStream *>(self);
        std::lock_guard<std::mutex> lock(s->mutex_);
        if (s->closed_) return 0;   // aborts the transfer
        s->queue_.emplace_back(data, data + size * n);
        s->started_ = true;
        s->cv_.notify_all();
        return size * n;
    }

    Request request_;
    std::thread worker_;
    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;
    mutable std::deque<std::vector<std::uint8_t>> queue_;
    mutable bool closed_ = false;
    bool started_ = false;
    bool done_ = false;
    CURLcode result_ = CURLE_OK;
    std::uint16_t status_ = 0;
};

}  // namespace

CurlTransport::CurlTransport() { curl_global_init(CURL_GLOBAL_DEFAULT); }
CurlTransport::~CurlTransport() { curl_global_cleanup(); }

ov::HttpResponse CurlTransport::send(const ov::HttpRequest &req, ov::TransportFailure &failure) const {
    Request request(req);
    ov::HttpResponse response;
    curl_easy_setopt(request.curl, CURLOPT_WRITEFUNCTION, &collect);
    curl_easy_setopt(request.curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(request.curl, CURLOPT_TIMEOUT, 120L);
    CURLcode code = curl_easy_perform(request.curl);
    if (code != CURLE_OK) {
        failure.kind = failure_kind(code);
        failure.message = curl_easy_strerror(code);
        return response;
    }
    response.status = request.status();
    for (auto &h : request.response_headers) response.headers.push_back(std::move(h));
    return response;
}

std::unique_ptr<ov::ByteStream> CurlTransport::open_stream(const ov::HttpRequest &req, ov::TransportFailure &failure) const {
    auto stream = std::make_unique<CurlStream>(req);
    if (!stream->wait_started(failure)) return nullptr;
    return stream;
}

struct MemoryStore::State {
    std::mutex mutex;
    std::map<std::string, std::vector<std::uint8_t>> values;
};

MemoryStore::MemoryStore() : state_(std::make_unique<State>()) {}
MemoryStore::~MemoryStore() = default;

bool MemoryStore::get(rust::Str key, rust::Vec<std::uint8_t> &value) const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    auto it = state_->values.find(str(key));
    if (it == state_->values.end()) return false;
    for (auto b : it->second) value.push_back(b);
    return true;
}

void MemoryStore::set(rust::Str key, rust::Slice<const std::uint8_t> value) const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->values[str(key)] = std::vector<std::uint8_t>(value.begin(), value.end());
}

void MemoryStore::remove(rust::Str key) const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->values.erase(str(key));
}

struct MemoryCache::State {
    std::mutex mutex;
    std::map<std::string, std::map<std::string, ov::CachedVault>> vaults;
    std::map<std::string, std::map<std::string, std::vector<std::uint8_t>>> meta;
};

MemoryCache::MemoryCache() : state_(std::make_unique<State>()) {}
MemoryCache::~MemoryCache() = default;

bool MemoryCache::load(rust::Str user_id, rust::Str vault_id, ov::CachedVault &out) const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    auto &user = state_->vaults[str(user_id)];
    auto it = user.find(str(vault_id));
    if (it == user.end()) return false;
    out = it->second;
    return true;
}

void MemoryCache::save(rust::Str user_id, rust::Str vault_id, const ov::CachedVault &v) const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->vaults[str(user_id)][str(vault_id)] = v;
}

void MemoryCache::remove(rust::Str user_id, rust::Str vault_id) const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->vaults[str(user_id)].erase(str(vault_id));
}

rust::Vec<rust::String> MemoryCache::list(rust::Str user_id) const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    rust::Vec<rust::String> ids;
    for (const auto &kv : state_->vaults[str(user_id)]) ids.push_back(rust::String(kv.first));
    return ids;
}

bool MemoryCache::load_meta(rust::Str user_id, rust::Str name, rust::Vec<std::uint8_t> &value) const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    auto &user = state_->meta[str(user_id)];
    auto it = user.find(str(name));
    if (it == user.end()) return false;
    for (auto b : it->second) value.push_back(b);
    return true;
}

void MemoryCache::save_meta(rust::Str user_id, rust::Str name, rust::Slice<const std::uint8_t> value) const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->meta[str(user_id)][str(name)] = std::vector<std::uint8_t>(value.begin(), value.end());
}

void MemoryCache::clear_user(rust::Str user_id) const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->vaults.erase(str(user_id));
    state_->meta.erase(str(user_id));
}

std::int64_t SystemClock::now_unix_ms() const {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

void StderrLog::log(ov::LogLevel level, rust::Str target, rust::Str msg) const {
    static const bool verbose = [] {
        const char *v = std::getenv("OV_LOG");
        return v && std::string(v) == "debug";
    }();
    if (level != ov::LogLevel::Error && level != ov::LogLevel::Warn && !verbose) return;
    std::fprintf(stderr, "[%d] %s: %s\n", static_cast<int>(level), str(target).c_str(), str(msg).c_str());
}

}  // namespace examples
