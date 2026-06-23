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
#include <vector>

#include "containers/IContainerEngine.hpp"
#include "perturbations/internal/PacketFloodDetail.hpp"

namespace chaos::orchestrator::perturbations {
namespace {

constexpr auto kDefaultIface = "eth0";

#pragma pack(push, 1)
struct EthernetHeader {
    std::array<uint8_t, 6> dst;
    std::array<uint8_t, 6> src;
    uint16_t ether_type;
};
static_assert(sizeof(EthernetHeader) == 14);

struct IpHeader {
    uint8_t ver_ihl;
    uint8_t dscp_ecn;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_frag_off;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    std::array<uint8_t, 4> src;
    std::array<uint8_t, 4> dst;
};
static_assert(sizeof(IpHeader) == 20);

struct TcpHeader {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint16_t data_offset_and_flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent_ptr;
};
static_assert(sizeof(TcpHeader) == 20);
#pragma pack(pop)

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

}  // namespace chaos::orchestrator::perturbations

namespace chaos::orchestrator::perturbations::detail {

ScopedNamespaceGuard::ScopedNamespaceGuard(int pid) {
    const std::string ns_path = fmt::format("/proc/{}/ns/net", pid);
    originalNsFd_ = open(ns_path.c_str(), O_RDONLY);
    if (originalNsFd_ < 0) {
        throw std::system_error(errno, std::generic_category(),
                                fmt::format("Failed to open network namespace for PID {}", pid));
    }
}

ScopedNamespaceGuard::~ScopedNamespaceGuard() {
    if (originalNsFd_ >= 0) {
        if (setns(originalNsFd_, CLONE_NEWNET) < 0) {
            SPDLOG_ERROR("ScopedNamespaceGuard: failed to restore original network namespace: {}",
                         std::strerror(errno));
        }
        close(originalNsFd_);
    }
}

uint32_t pseudoHeaderChecksum(uint32_t src_ip, uint32_t dst_ip, uint16_t tcp_len) {
    uint32_t sum = 0;
    sum += (src_ip >> 16) & 0xFFFF;
    sum += src_ip & 0xFFFF;
    sum += (dst_ip >> 16) & 0xFFFF;
    sum += dst_ip & 0xFFFF;
    sum += IPPROTO_TCP;
    sum += htons(tcp_len);
    return sum;
}

uint16_t segmentChecksum(const std::vector<uint8_t>& data, uint32_t pseudo_sum) {
    uint32_t sum = pseudo_sum;
    for (size_t i = 0; i < data.size(); i += 2) {
        uint16_t word = static_cast<uint16_t>(data[i]);
        if (i + 1 < data.size()) {
            word = static_cast<uint16_t>(word << 8 | data[i + 1]);
        }
        sum += word;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return static_cast<uint16_t>(~sum);
}

}  // namespace chaos::orchestrator::perturbations::detail

namespace chaos::orchestrator::perturbations {

PacketFloodPerturbation::PacketFloodPerturbation(std::shared_ptr<containers::IContainerEngine> engine,
                                                 std::string target_id, const manifests::Perturbation& spec)
    : engine_(std::move(engine)), target_id_(std::move(target_id)) {
    iface_ = [&] {
        auto iter = spec.parameters.find("iface");
        return (iter != spec.parameters.end()) ? iter->second : std::string(kDefaultIface);
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
    int container_pid = engine_->getContainerPid(target_id_);
    if (container_pid <= 0) {
        throw std::system_error(EINVAL, std::generic_category(), "Invalid container PID");
    }
    UniqueFd netns_fd(engine_->getContainerNetnsFd(target_id_));
    detail::ScopedNamespaceGuard nsGuard(getpid());

    if (setns(netns_fd.get(), CLONE_NEWNET) < 0) {
        throw std::system_error(errno, std::generic_category(),
                                fmt::format("Failed to enter container '{}' network namespace", target_id_));
    }
    netns_fd.reset();

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

    // nsGuard destructor restores original network namespace.
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
        flood_thread_ = std::jthread([this](const std::stop_token& stop_token) { floodLoop(stop_token); });
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

void PacketFloodPerturbation::floodLoop(const std::stop_token& stop) const {
    constexpr size_t min_pkt = sizeof(EthernetHeader) + sizeof(IpHeader) + sizeof(TcpHeader);
    const size_t frame_size = std::max(static_cast<size_t>(packet_size_), min_pkt);
    const size_t payload_len = frame_size - min_pkt;

    std::array<uint8_t, 4> dest_ip{};
    inet_pton(AF_INET, containerIp_.c_str(), dest_ip.data());

    std::vector<uint8_t> frame(frame_size);
    std::random_device random_device;
    std::mt19937 gen(random_device());
    std::uniform_int_distribution<int> byte_dist{0, 255};
    std::uniform_int_distribution<uint16_t> word_dist{0, 65535};
    auto rng = [&] { return static_cast<uint8_t>(byte_dist(gen)); };

    auto* eth_header = reinterpret_cast<EthernetHeader*>(frame.data());
    auto* ip_header = reinterpret_cast<IpHeader*>(frame.data() + sizeof(EthernetHeader));
    auto* tcp_header = reinterpret_cast<TcpHeader*>(frame.data() + sizeof(EthernetHeader) + sizeof(IpHeader));
    auto* payload = frame.data() + min_pkt;

    eth_header->dst = localMac_;
    eth_header->ether_type = htons(0x0800);

    ip_header->ver_ihl = 0x45;
    ip_header->dscp_ecn = 0;
    ip_header->total_len = htons(static_cast<uint16_t>(frame_size - sizeof(EthernetHeader)));
    ip_header->flags_frag_off = htons(0x4000);
    ip_header->ttl = 64;
    ip_header->protocol = 6;
    ip_header->dst = dest_ip;

    tcp_header->dst_port = htons(8000);
    tcp_header->ack_num = 0;
    tcp_header->data_offset_and_flags = htons(static_cast<uint16_t>(5 << 12 | 0x02));
    tcp_header->window = htons(65535);
    tcp_header->urgent_ptr = 0;
    tcp_header->checksum = 0;

    struct sockaddr_ll dst {};
    dst.sll_family = AF_PACKET;
    dst.sll_ifindex = ifindex_;
    dst.sll_protocol = htons(ETH_P_ALL);

    while (!stop.stop_requested()) {
        for (int i = 0; i < rate_; ++i) {
            if (stop.stop_requested()) return;

            std::ranges::generate(eth_header->src, rng);
            std::ranges::generate(ip_header->src, rng);
            ip_header->id = word_dist(gen);
            tcp_header->src_port = word_dist(gen);
            tcp_header->seq_num = word_dist(gen);

            std::ranges::generate(std::span(payload, payload_len), rng);

            ip_header->checksum = 0;
            ip_header->checksum = ipChecksum(
                std::span<const uint16_t>(reinterpret_cast<const uint16_t*>(ip_header), sizeof(IpHeader) / 2));

            tcp_header->checksum = 0;
            uint32_t tcp_src_ip =
                (static_cast<uint32_t>(ip_header->src[0]) << 24) | (static_cast<uint32_t>(ip_header->src[1]) << 16) |
                (static_cast<uint32_t>(ip_header->src[2]) << 8) | static_cast<uint32_t>(ip_header->src[3]);
            uint32_t tcp_dst_ip = (static_cast<uint32_t>(dest_ip[0]) << 24) | (static_cast<uint32_t>(dest_ip[1]) << 16) |
                                  (static_cast<uint32_t>(dest_ip[2]) << 8) | static_cast<uint32_t>(dest_ip[3]);
            uint16_t tcp_data_len = static_cast<uint16_t>(sizeof(TcpHeader) + payload_len);
            uint32_t pseudo_sum = detail::pseudoHeaderChecksum(tcp_src_ip, tcp_dst_ip, tcp_data_len);
            {
                std::vector<uint8_t> tcp_seg_data(tcp_data_len);
                std::memcpy(tcp_seg_data.data(), tcp_header, tcp_data_len);
                tcp_header->checksum = detail::segmentChecksum(tcp_seg_data, pseudo_sum);
            }

            sendto(raw_sd_, frame.data(), frame.size(), 0, reinterpret_cast<struct sockaddr*>(&dst), sizeof(dst));
        }
        if (stop.stop_requested()) {
            return;
        }
    }
}

}  // namespace chaos::orchestrator::perturbations
