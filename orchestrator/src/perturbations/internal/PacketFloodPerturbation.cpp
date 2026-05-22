#include "PacketFloodPerturbation.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <fmt/format.h>
#include <linux/if_ether.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <sched.h>
#include <spdlog/spdlog.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstddef>
#include <cstring>
#include <random>
#include <span>
#include <system_error>
#include <utility>

#include "containers/IContainerEngine.hpp"

namespace chaos::orchestrator::perturbations {
namespace {

#pragma pack(push, 1)
struct EthernetHeader {
    std::array<uint8_t, 6> dst;
    std::array<uint8_t, 6> src;
    uint16_t etherType;
};
static_assert(sizeof(EthernetHeader) == 14);

struct IpHeader {
    uint8_t verIhl;
    uint8_t dscpEcn;
    uint16_t totalLen;
    uint16_t id;
    uint16_t flagsFragOff;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    std::array<uint8_t, 4> src;
    std::array<uint8_t, 4> dst;
};
static_assert(sizeof(IpHeader) == 20);

struct TcpHeader {
    uint16_t srcPort;
    uint16_t dstPort;
    uint32_t seqNum;
    uint32_t ackNum;
    uint16_t dataOffsetAndFlags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgentPtr;
};
static_assert(sizeof(TcpHeader) == 20);
#pragma pack(pop)

struct UniqueFd {
    int fd = -1;
    explicit UniqueFd(int descriptor) noexcept : fd(descriptor) {}
    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;
    UniqueFd(UniqueFd&&) = delete;
    UniqueFd& operator=(UniqueFd&&) = delete;
    ~UniqueFd() { reset(); }
    void reset() {
        if (fd >= 0) {
            close(fd);
            fd = -1;
        }
    }
    [[nodiscard]] int get() const noexcept { return fd; }
};

}  // namespace

PacketFloodPerturbation::PacketFloodPerturbation(std::shared_ptr<containers::IContainerEngine> engine,
                                                 std::string target_id, const manifests::Perturbation& spec)
    : engine_(std::move(engine)), target_id_(std::move(target_id)) {
    iface_ = [&] {
        auto iter = spec.parameters.find("iface");
        return (iter != spec.parameters.end()) ? iter->second : std::string("eth0");
    }();
    rate_ = [&] {
        auto iter = spec.parameters.find("rate");
        if (iter == spec.parameters.end()) {
            return 1000;
        }
        int val = 0;
        std::from_chars(iter->second.data(), iter->second.data() + iter->second.size(), val);
        return val;
    }();
    packet_size_ = [&] {
        auto iter = spec.parameters.find("packet_size");
        if (iter == spec.parameters.end()) {
            return 128;
        }
        int val = 0;
        std::from_chars(iter->second.data(), iter->second.data() + iter->second.size(), val);
        return val;
    }();
}

void PacketFloodPerturbation::setupSocketInNetns() {
    UniqueFd netnsFd(engine_->getContainerNetnsFd(target_id_));
    UniqueFd origNetnsFd(open("/proc/self/ns/net", O_RDONLY));
    if (origNetnsFd.get() < 0) {
        throw std::system_error(errno, std::generic_category(), "Failed to save current network namespace");
    }

    if (setns(netnsFd.get(), CLONE_NEWNET) < 0) {
        throw std::system_error(errno, std::generic_category(),
                                fmt::format("Failed to enter container '{}' network namespace", target_id_));
    }
    netnsFd.reset();

    raw_sd_ = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (raw_sd_ < 0) {
        throw std::system_error(errno, std::generic_category(), "Failed to create raw socket");
    }

    struct ifreq ifr {};
    iface_.copy(ifr.ifr_name, sizeof(ifr.ifr_name) - 1);
    ifr.ifr_name[iface_.size() < sizeof(ifr.ifr_name) ? iface_.size() : sizeof(ifr.ifr_name) - 1] = '\0';
    if (ioctl(raw_sd_, SIOCGIFINDEX, &ifr) < 0) {
        close(raw_sd_);
        raw_sd_ = -1;
        throw std::system_error(errno, std::generic_category(),
                                fmt::format("Failed to get interface index for '{}'", iface_));
    }
    ifindex_ = ifr.ifr_ifindex;

    if (ioctl(raw_sd_, SIOCGIFHWADDR, &ifr) < 0) {
        close(raw_sd_);
        raw_sd_ = -1;
        throw std::system_error(errno, std::generic_category(),
                                fmt::format("Failed to get MAC address for '{}'", iface_));
    }
    std::memcpy(localMac_.data(), ifr.ifr_hwaddr.sa_data, localMac_.size());

    struct sockaddr_ll sll {};
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifindex_;
    sll.sll_protocol = htons(ETH_P_ALL);
    if (bind(raw_sd_, reinterpret_cast<struct sockaddr*>(&sll), sizeof(sll)) < 0) {
        close(raw_sd_);
        raw_sd_ = -1;
        throw std::system_error(errno, std::generic_category(), "Failed to bind raw socket to interface");
    }

    if (setns(origNetnsFd.get(), CLONE_NEWNET) < 0) {
        close(raw_sd_);
        raw_sd_ = -1;
        throw std::system_error(errno, std::generic_category(), "Failed to restore original network namespace");
    }
    origNetnsFd.reset();
}

void PacketFloodPerturbation::apply() {
    if (bool expected = false; !hasBeenApplied_.compare_exchange_strong(expected, true)) {
        SPDLOG_WARN("Packet Flood already applied to target {}, skipping", target_id_);
        return;
    }

    if (target_id_.empty()) {
        throw std::invalid_argument("Target ID is empty");
    }

    containerIp_ = engine_->getContainerIp(target_id_);

    try {
        setupSocketInNetns();
        flood_thread_ = std::jthread([this](const std::stop_token& stopToken) { floodLoop(stopToken); });
        SPDLOG_INFO("Packet Flood started on target {} iface={} rate={} size={} ip={}", target_id_, iface_, rate_,
                    packet_size_, containerIp_);
    } catch (...) {
        if (raw_sd_ >= 0) {
            close(raw_sd_);
            raw_sd_ = -1;
        }
        hasBeenApplied_.store(false);
        throw;
    }
}

void PacketFloodPerturbation::revert() {
    if (bool expected = true; !hasBeenApplied_.compare_exchange_strong(expected, false)) {
        SPDLOG_WARN("Packet Flood was not applied, skipping revert");
        return;
    }

    if (flood_thread_.joinable()) {
        flood_thread_.request_stop();
        flood_thread_.join();
    }
    if (raw_sd_ >= 0) {
        close(raw_sd_);
        raw_sd_ = -1;
    }
    SPDLOG_INFO("Packet Flood reverted on target {}", target_id_);
}

namespace {

uint16_t ipChecksum(const std::span<const uint16_t> words) {
    uint32_t sum = 0;
    for (const auto word : words) {
        sum += word;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return static_cast<uint16_t>(~sum);
}

uint16_t tcpChecksum(const std::span<const uint8_t> tcpData, const std::array<uint8_t, 4>& srcIp,
                     const std::array<uint8_t, 4>& dstIp) {
    const size_t tcpLenBytes = tcpData.size();
    uint32_t sum = 0;

    const auto to16 = [](const std::array<uint8_t, 4>& ip) {
        return static_cast<uint32_t>(static_cast<uint16_t>(ip[0]) << 8 | ip[1]) |
               (static_cast<uint32_t>(static_cast<uint16_t>(ip[2]) << 8 | ip[3]) << 16);
    };
    const uint32_t srcVal = to16(srcIp);
    const uint32_t dstVal = to16(dstIp);
    sum += srcVal & 0xFFFF;
    sum += (srcVal >> 16) & 0xFFFF;
    sum += dstVal & 0xFFFF;
    sum += (dstVal >> 16) & 0xFFFF;
    sum += static_cast<uint16_t>(IPPROTO_TCP);
    sum += htons(static_cast<uint16_t>(tcpLenBytes));

    for (size_t i = 0; i < tcpLenBytes; i += 2) {
        uint16_t word = static_cast<uint16_t>(tcpData[i]);
        if (i + 1 < tcpLenBytes) {
            word = static_cast<uint16_t>(word << 8 | tcpData[i + 1]);
        }
        sum += word;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return static_cast<uint16_t>(~sum);
}

}  // namespace

void PacketFloodPerturbation::floodLoop(const std::stop_token& stop) const {
    constexpr size_t minPkt = sizeof(EthernetHeader) + sizeof(IpHeader) + sizeof(TcpHeader);
    const size_t frameSize = std::max(static_cast<size_t>(packet_size_), minPkt);
    const size_t payloadLen = frameSize - minPkt;

    std::array<uint8_t, 4> destIp{};
    inet_pton(AF_INET, containerIp_.c_str(), destIp.data());

    std::vector<uint8_t> frame(frameSize);
    std::random_device randomDevice;
    std::mt19937 gen(randomDevice());
    std::uniform_int_distribution<int> byteDist{0, 255};
    std::uniform_int_distribution<uint16_t> wordDist{0, 65535};
    auto rng = [&] { return static_cast<uint8_t>(byteDist(gen)); };

    auto* ethHeader = reinterpret_cast<EthernetHeader*>(frame.data());
    auto* ipHeader = reinterpret_cast<IpHeader*>(frame.data() + sizeof(EthernetHeader));
    auto* tcpHeader = reinterpret_cast<TcpHeader*>(frame.data() + sizeof(EthernetHeader) + sizeof(IpHeader));
    auto* payload = frame.data() + minPkt;

    ethHeader->dst = localMac_;
    ethHeader->etherType = htons(0x0800);

    ipHeader->verIhl = 0x45;
    ipHeader->dscpEcn = 0;
    ipHeader->totalLen = htons(static_cast<uint16_t>(frameSize - sizeof(EthernetHeader)));
    ipHeader->flagsFragOff = htons(0x4000);
    ipHeader->ttl = 64;
    ipHeader->protocol = 6;
    ipHeader->dst = destIp;

    tcpHeader->dstPort = htons(8000);
    tcpHeader->ackNum = 0;
    tcpHeader->dataOffsetAndFlags = htons(static_cast<uint16_t>(5 << 12 | 0x02));
    tcpHeader->window = htons(65535);
    tcpHeader->urgentPtr = 0;
    tcpHeader->checksum = 0;

    struct sockaddr_ll dst {};
    dst.sll_family = AF_PACKET;
    dst.sll_ifindex = ifindex_;
    dst.sll_protocol = htons(ETH_P_ALL);

    while (!stop.stop_requested()) {
        for (int i = 0; i < rate_; ++i) {
            if (stop.stop_requested()) return;

            std::ranges::generate(ethHeader->src, rng);
            std::ranges::generate(ipHeader->src, rng);
            ipHeader->id = wordDist(gen);
            tcpHeader->srcPort = wordDist(gen);
            tcpHeader->seqNum = wordDist(gen);

            std::ranges::generate(std::span(payload, payloadLen), rng);

            ipHeader->checksum = 0;
            ipHeader->checksum = ipChecksum(
                std::span<const uint16_t>(reinterpret_cast<const uint16_t*>(ipHeader), sizeof(IpHeader) / 2));

            tcpHeader->checksum = 0;
            tcpHeader->checksum = tcpChecksum(
                std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(tcpHeader), sizeof(TcpHeader) + payloadLen),
                ipHeader->src, destIp);

            sendto(raw_sd_, frame.data(), frame.size(), 0, reinterpret_cast<struct sockaddr*>(&dst), sizeof(dst));
        }
        if (stop.stop_requested()) {
            return;
        }
    }
}

}  // namespace chaos::orchestrator::perturbations
