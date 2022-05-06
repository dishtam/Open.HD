#ifndef WIFI_H
#define WIFI_H

#include <array>
#include <chrono>
#include <vector>

#include "Platform.h"

#include "json.hpp"

#include "openhd-wifi.hpp"
#include "openhd-platform.hpp"


/**
 * Discovery and access to all wifi cards on the system.
 */
class WifiCards {
public:
    WifiCards(PlatformType platform_type, BoardType board_type, CarrierType carrier_type, WiFiHotspotType wifi_hotspot_type);
    virtual ~WifiCards() = default;

    void discover();
    void process_card(std::string interface_name);

    nlohmann::json generate_manifest();

private:
    std::vector<WiFiCard> m_wifi_cards;
    const PlatformType m_platform_type;
    const BoardType m_board_type;
    const CarrierType m_carrier_type;
    WiFiHotspotType m_wifi_hotspot_type;
};

#endif
