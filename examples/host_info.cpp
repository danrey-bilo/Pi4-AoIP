#include <aoip/rpi4.hpp>
#include <iostream>

int main() {
  const auto host = aoip::rpi4::inspect_host();
  std::cout << "model=" << host.model << "\nkernel=" << host.kernel
            << "\nPREEMPT_RT=" << host.realtime_kernel << '\n';
  aoip::rpi4::Endpoint endpoint;
  std::string error;
  if (aoip::rpi4::find_wired_endpoint(nullptr, endpoint, error))
    std::cout << "wired=" << endpoint.interface_name << " " << endpoint.ipv4 << '\n';
  else
    std::cout << error << '\n';
  // Read-only example: no affinity changes, sockets, service restart or PCM.
  return 0;
}
