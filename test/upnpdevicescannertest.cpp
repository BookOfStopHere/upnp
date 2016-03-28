#include <catch.hpp>

#include "utils/log.h"
#include "upnp/upnpdevicescanner.h"
#include "upnp/upnpuv.h"

namespace upnp
{
namespace test
{

using namespace utils;
using namespace std::chrono_literals;

TEST_CASE("Device discover Client", "[Scan]")
{
    uv::Loop loop;
    DeviceScanner scanner(loop, { DeviceType::MediaServer, DeviceType::MediaRenderer });

    scanner.DeviceDiscoveredEvent.connect([&] (std::shared_ptr<Device> dev) {
        log::info("Discovered: {}", dev->m_udn);
        scanner.stop();
    }, &loop);

    scanner.start();
    scanner.refresh();

    loop.run(uv::RunMode::Default);
}

}
}
