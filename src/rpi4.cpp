#include <aoip/rpi4.hpp>
#include <fstream>
#include <net/if.h>
#include <sys/stat.h>
#include <sys/utsname.h>

namespace aoip::rpi4 {
namespace {
std::string read_line(const char* path) {
  std::ifstream input(path);
  std::string result;
  std::getline(input, result, '\0');
  while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) result.pop_back();
  return result;
}
bool exists(const std::string& path) {
  struct stat info{};
  return stat(path.c_str(), &info) == 0;
}
}

HostStatus inspect_host() {
  HostStatus result;
  result.model = read_line("/proc/device-tree/model");
  result.raspberry_pi_4 = result.model.rfind("Raspberry Pi 4 Model B", 0) == 0;
  utsname info{};
  if (uname(&info) == 0) {
    result.kernel = info.release;
    result.realtime_kernel = std::strstr(info.version, "PREEMPT_RT") != nullptr;
  }
  result.realtime_kernel = result.realtime_kernel || read_line("/sys/kernel/realtime") == "1";
  result.isolated_cpus = read_line("/sys/devices/system/cpu/isolated");
  return result;
}

bool initialize(std::string& error) {
  auto host = inspect_host();
  if (!host.raspberry_pi_4) { error = "This package requires Raspberry Pi 4 Model B"; return false; }
  if (!host.realtime_kernel) { error = "PREEMPT_RT kernel required; the AoIP package does not replace the kernel"; return false; }
  if (sysconf(_SC_NPROCESSORS_ONLN) < 4) { error = "Four online Pi 4 CPUs are required"; return false; }
  // A process-wide guard also covers startup work before individual role pinning.
  cpu_set_t allowed; CPU_ZERO(&allowed); CPU_SET(0, &allowed); CPU_SET(1, &allowed);
  int status = pthread_setaffinity_np(pthread_self(), sizeof(allowed), &allowed);
  if (status) { error = std::string("Cannot restrict Pi AoIP to CPU0/CPU1: ") + std::strerror(status); return false; }
  peer::set_thread_setup(configure_thread);
  return true;
}

bool configure_thread(peer::ThreadRole role) {
  const unsigned cpu = role == peer::ThreadRole::receive ? 0 : 1;
  const bool realtime = role == peer::ThreadRole::transmit || role == peer::ThreadRole::receive;
  cpu_set_t mask; CPU_ZERO(&mask); CPU_SET(cpu, &mask);
  int status = pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask);
  if (status) { std::fprintf(stderr, "Pi4 CPU%u affinity: %s\n", cpu, std::strerror(status)); return false; }
  sched_param parameters{}; parameters.sched_priority = realtime ? 70 : 0;
  status = pthread_setschedparam(pthread_self(), realtime ? SCHED_FIFO : SCHED_OTHER, &parameters);
  if (status) { std::fprintf(stderr, "Pi4 RT permissions: %s\n", std::strerror(status)); return false; }
  return true;
}

bool find_wired_endpoint(const char* interface_name, Endpoint& result, std::string& error) {
  ifaddrs* list = nullptr;
  if (getifaddrs(&list)) { error = std::string("Cannot enumerate network interfaces: ") + std::strerror(errno); return false; }
  std::vector<Endpoint> candidates;
  for (auto* item = list; item; item = item->ifa_next) {
    if (!item->ifa_addr || item->ifa_addr->sa_family != AF_INET ||
        !(item->ifa_flags & IFF_UP) || !(item->ifa_flags & IFF_RUNNING) || (item->ifa_flags & IFF_LOOPBACK)) continue;
    if (interface_name && *interface_name && std::strcmp(interface_name, item->ifa_name)) continue;
    std::string base = std::string("/sys/class/net/") + item->ifa_name;
    if (exists(base + "/wireless") || exists(base + "/phy80211") ||
        read_line((base + "/type").c_str()) != "1" || !exists(base + "/device")) continue;
    char ipv4[INET_ADDRSTRLEN]{};
    auto* address = reinterpret_cast<sockaddr_in*>(item->ifa_addr);
    if (!inet_ntop(AF_INET, &address->sin_addr, ipv4, sizeof(ipv4))) continue;
    candidates.push_back({item->ifa_name, ipv4});
  }
  freeifaddrs(list);
  if (candidates.empty()) { error = "Waiting for wired IPv4: check Ethernet link, address and PIAOIP_INTERFACE"; return false; }
  if (candidates.size() != 1) { error = "Multiple wired IPv4 addresses: select PIAOIP_INTERFACE and use one IPv4 address"; return false; }
  result = candidates.front();
  return true;
}
}
