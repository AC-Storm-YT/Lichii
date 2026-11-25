// SPDX-FileCopyrightText: Copyright 2026 Lichii contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <array>
#include <optional>
#include <string>
#include <variant>

#include <catch2/catch_test_macros.hpp>

#include "common/scope_exit.h"
#include "common/settings.h"
#include "core/internal_network/local_redirect.h"
#include "core/internal_network/network.h"
#include "core/internal_network/sockets.h"

namespace LocalRedirect = Network::LocalRedirect;

namespace {

constexpr Network::IPv4Address Loopback{127, 0, 0, 1};
constexpr Network::SockAddrIn Public{Network::Domain::INET, {192, 0, 2, 7}, 443};

void SetBlocked(bool blocked) {
    Settings::values.block_game_network.SetValue(blocked);
    LocalRedirect::Configure();
}

Network::SockAddrIn SockName(Network::Socket& socket) {
    const auto [name, error] = socket.GetSockName();
    REQUIRE(error == Network::Errno::SUCCESS);
    return name;
}

} // Anonymous namespace

TEST_CASE("Blocked game network: off by default, and what it turns on", "[core][network]") {
    REQUIRE_FALSE(Settings::values.block_game_network.GetDefault());
    const bool nextendo = Settings::values.nextendo_enabled.GetValue();
    SCOPE_EXIT {
        Settings::values.nextendo_enabled.SetValue(nextendo);
        SetBlocked(false);
    };
    Settings::values.nextendo_enabled.SetValue(true);

    SetBlocked(false);
    CHECK_FALSE(LocalRedirect::Isolated());
    CHECK_FALSE(LocalRedirect::LoopbackOnly());
    CHECK(LocalRedirect::Allowed(Public));
    CHECK(LocalRedirect::OnlinePlayAllowed());

    // Without the host redirect: loopback only, and no online services for the game.
    SetBlocked(true);
    CHECK(LocalRedirect::Isolated());
    CHECK(LocalRedirect::LoopbackOnly());
    CHECK_FALSE(LocalRedirect::Allowed(Public));
    CHECK(LocalRedirect::Allowed({Network::Domain::INET, Loopback, 80}));
    CHECK_FALSE(LocalRedirect::OnlinePlayAllowed());

    // Host names don't reach the host's resolver; localhost and numeric loopback still resolve.
    CHECK(std::holds_alternative<Network::GetAddrInfoError>(
        Network::GetAddressInfo("example.com", std::nullopt)));
    CHECK(std::holds_alternative<Network::GetAddrInfoError>(
        Network::GetAddressInfo("192.0.2.7", std::nullopt)));
    CHECK_FALSE(std::holds_alternative<Network::GetAddrInfoError>(
        Network::GetAddressInfo("localhost", std::nullopt)));

    SetBlocked(false);
    CHECK_FALSE(LocalRedirect::Isolated());
    CHECK(LocalRedirect::Allowed(Public));
}

TEST_CASE("Blocked game network: sockets stay on loopback", "[core][network]") {
    Network::NetworkInstance network_instance;
    SetBlocked(true);
    SCOPE_EXIT {
        SetBlocked(false);
    };

    SECTION("a wildcard bind lands on loopback, another address is refused") {
        Network::Socket socket;
        REQUIRE(socket.Initialize(Network::Domain::INET, Network::Type::DGRAM,
                                  Network::Protocol::UDP) == Network::Errno::SUCCESS);
        CHECK(socket.Bind({Network::Domain::INET, {192, 0, 2, 7}, 0}) != Network::Errno::SUCCESS);
        REQUIRE(socket.Bind({Network::Domain::INET, {0, 0, 0, 0}, 0}) == Network::Errno::SUCCESS);
        CHECK(SockName(socket).ip == Loopback);
    }

    SECTION("an unbound datagram socket is bound to loopback by its first send") {
        Network::Socket receiver;
        REQUIRE(receiver.Initialize(Network::Domain::INET, Network::Type::DGRAM,
                                    Network::Protocol::UDP) == Network::Errno::SUCCESS);
        REQUIRE(receiver.Bind({Network::Domain::INET, Loopback, 0}) == Network::Errno::SUCCESS);
        const Network::SockAddrIn receiver_addr = SockName(receiver);

        Network::Socket sender;
        REQUIRE(sender.Initialize(Network::Domain::INET, Network::Type::DGRAM,
                                  Network::Protocol::UDP) == Network::Errno::SUCCESS);
        constexpr std::array<u8, 4> message{'p', 'i', 'n', 'g'};
        CHECK(sender.SendTo(0, message, &Public).second != Network::Errno::SUCCESS);
        const auto [sent, send_error] = sender.SendTo(0, message, &receiver_addr);
        REQUIRE(send_error == Network::Errno::SUCCESS);
        CHECK(sent == static_cast<s32>(message.size()));
        const Network::SockAddrIn sender_addr = SockName(sender);
        CHECK(sender_addr.ip == Loopback);
        CHECK(sender_addr.portno != 0);

        std::array<u8, 16> buffer{};
        Network::SockAddrIn from{};
        const auto [received, receive_error] = receiver.RecvFrom(0, buffer, &from);
        REQUIRE(receive_error == Network::Errno::SUCCESS);
        CHECK(received == static_cast<s32>(message.size()));
        CHECK(from.ip == Loopback);
    }

    SECTION("an unbound listener listens on loopback, and outside connections are refused") {
        Network::Socket listener;
        REQUIRE(listener.Initialize(Network::Domain::INET, Network::Type::STREAM,
                                    Network::Protocol::TCP) == Network::Errno::SUCCESS);
        REQUIRE(listener.Listen(1) == Network::Errno::SUCCESS);
        CHECK(SockName(listener).ip == Loopback);

        Network::Socket client;
        REQUIRE(client.Initialize(Network::Domain::INET, Network::Type::STREAM,
                                  Network::Protocol::TCP) == Network::Errno::SUCCESS);
        CHECK(client.Connect(Public) != Network::Errno::SUCCESS);
    }
}
