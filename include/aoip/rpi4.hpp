#pragma once
#include <aoip/peer.hpp>
#include <string>

namespace aoip::rpi4 {
struct HostStatus {
  std::string model, kernel, isolated_cpus;
  bool raspberry_pi_4 = false, realtime_kernel = false;
};
struct Endpoint { std::string interface_name, ipv4; };
HostStatus inspect_host();
// Validate Pi 4 / PREEMPT_RT, then install the CPU0/CPU1 policy before threads start.
bool initialize(std::string& error);
bool configure_thread(peer::ThreadRole role);
// Empty interface selects the sole active wired IPv4 interface. Never selects Wi-Fi.
bool find_wired_endpoint(const char* interface_name, Endpoint&, std::string& error);
}
