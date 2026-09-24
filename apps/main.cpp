#include <aoip/rpi4.hpp>
#include <aoip/service.hpp>
int main(int argc, char** argv) {
  if (argc == 2 && std::strcmp(argv[1], "--version") == 0) {
    std::puts("PiAoIP 2.1.0 / Pi4-AoIP"); return 0;
  }
  if (argc == 2 && std::strcmp(argv[1], "--check") == 0) {
    const auto host = aoip::rpi4::inspect_host();
    aoip::rpi4::Endpoint endpoint; std::string error;
    const bool wired = aoip::rpi4::find_wired_endpoint(std::getenv("PIAOIP_INTERFACE"), endpoint, error);
    std::printf("model=%s\nkernel=%s\nrealtime=%u\nisolated=%s\nnetwork_cpus=0,1\neffects_cpus=2,3\n",
      host.model.c_str(), host.kernel.c_str(), host.realtime_kernel, host.isolated_cpus.c_str());
    if (wired) std::printf("interface=%s\naddress=%s\n", endpoint.interface_name.c_str(), endpoint.ipv4.c_str());
    else std::fprintf(stderr, "%s\n", error.c_str());
    return host.raspberry_pi_4 && host.realtime_kernel && wired ? 0 : 2;
  }
  if (argc > 2 && (!std::strcmp(argv[2], "configure-me") || !std::strcmp(argv[2], "auto"))) {
    std::fprintf(stderr, "Configure Windows Ethernet IPv4: sudo piaoip-configure --interface eth0 --peer WINDOWS_IP --restart\n");
    return 2;
  }
  if (argc != 10 && argc != 11) return aoip::peer::run_service(argc, argv);
  std::string error;
  if (!aoip::rpi4::initialize(error)) { std::fprintf(stderr, "%s\n", error.c_str()); return 2; }
  aoip::rpi4::Endpoint endpoint; std::string previous;
  while (!aoip::rpi4::find_wired_endpoint(std::getenv("PIAOIP_INTERFACE"), endpoint, error)) {
    if (error != previous) std::fprintf(stderr, "%s\n", error.c_str());
    previous = error;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  std::printf("Pi4 audio interface=%s address=%s peer=%s; CPU0/CPU1 only\n",
              endpoint.interface_name.c_str(), endpoint.ipv4.c_str(), argv[2]);
  std::fflush(stdout);
  return aoip::peer::run_service(argc, argv, {endpoint.ipv4.c_str()});
}
