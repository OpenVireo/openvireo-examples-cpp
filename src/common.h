// Shared setup for the examples: a client, and an account signed in from OV_EMAIL, OV_PASSWORD and
// OV_SECRET_KEY (OV_ISSUER for a server other than https://accounts.openvireo.com).
//
// Every core call blocks: a GUI app makes them on a worker thread. ov::call() turns a failure into an
// ov::Error with the code, message, status and details.
#pragma once

#include <ovcore.h>

#include <string>
#include <vector>

namespace examples {

using Args = std::vector<std::string>;

// The account server: OV_ISSUER, or the hosted one.
std::string issuer();

// An environment variable; exits with a message when it isn't set.
std::string env(const char *name);

rust::Vec<std::uint8_t> to_bytes(const std::string &s);
std::string text(const rust::Vec<std::uint8_t> &bytes);

[[noreturn]] void usage(const char *text);
std::string prompt(const std::string &text);
std::string ask_for_code();

// A client on the examples' Host (host.h).
rust::Box<ov::Client> make_client(const std::string &device_name = "OpenVireo examples", bool offline = false);

// Connects and signs in, asking for a TOTP code when two-step sign-in is on.
rust::Box<ov::Account> sign_in(const ov::Client &client, const ov::CancelToken &cancel);

// Loads the core and reaches the account server, without signing in.
void check(const Args &args);

}  // namespace examples
