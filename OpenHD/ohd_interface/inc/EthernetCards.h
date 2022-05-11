#ifndef ETHERNET_H
#define ETHERNET_H

#include <array>
#include <chrono>
#include <vector>

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "json.hpp"

#include "openhd-ethernet.hpp"

/**
 * Provides access to the discovered ethernet cards on the system.
 * There should only be one instance of this class in the whole OpenHD project.
 * TODO: I think it would be a good idea to go back to a "discover in intervalls -> do stuff"
 * like in EZ-wifibroadcast here. Because:
 * 1) For ethernet hotspot: Users might connect / disconnect a device via USB during run time.
 * 2) For wifi hotspot: The behaviour of "setting up a wifi hotspot if and only if" there is a internal
 * wifi card / the user connected cards for both the 2.4 and 5.8 ghz band was quite error resilient
 * 3) On wifi hotspot, the user might connect / disconnect multiple devices at run time.
 * Aka we could probably get rid of ethernet, and have a global "Hotspots" class.
 */
class EthernetCards {
public:
    EthernetCards(bool is_air, std::string unit_id);
    virtual ~EthernetCards() = default;
    void process_manifest();
    void configure();
    void setup_hotspot(EthernetCard &card);
    static void setup_static(EthernetCard &card);
    static void setup_client(EthernetCard &card);
    static void save_settings(const std::vector<EthernetCard>& cards, std::string settings_file);
private:
    void process_card(EthernetCard &card);
private:
    const bool m_is_air = false;
    const std::string m_unit_id;
    bool m_hotspot_configured = false;
    // todo: read from settings file once new settings system merged
    std::string m_ethernet_hotspot_address = "192.168.3.1";
    std::vector<EthernetCard> m_ethernet_cards;
    EthernetHotspotType m_ethernet_hotspot_type;
};

#endif
