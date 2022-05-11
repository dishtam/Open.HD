#include <cstdio>
#include <stdio.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem.hpp>

#include <boost/regex.hpp>

#include "json.hpp"
#include "inja.hpp"

#include "openhd-ethernet.hpp"
#include "openhd-settings.hpp"
#include "openhd-log.hpp"
#include "openhd-util.hpp"

#include "EthernetCards.h"


EthernetCards::EthernetCards(bool is_air, std::string unit_id): m_is_air(is_air), m_unit_id(unit_id) {}


void EthernetCards::configure() {
    std::cout << "Ethernet::configure()" << std::endl;

    process_manifest();


    /*
     * Get the local settings, if there are any
     */
    std::vector<std::map<std::string, std::string> > settings;

    try {
        const std::string settings_path = findOrCreateSettingsDirectory(m_is_air);
        std::string settings_file = settings_path + "/ethernet.conf";
        std::cerr << "settings_file: " << settings_file << std::endl;
        settings = read_config(settings_file);
    } catch (std::exception &ex) {
        std::cerr << "Ethernet settings load error: " << ex.what() << std::endl;
    }

    /*
     * Now use the settings to override the detected hardware configuration in each Ethernet in m_ethernet_cards
     *
     */
    std::vector<EthernetCard> save_cards;

    for (auto card : m_ethernet_cards) {
        std::map<std::string, std::string> setting_map;

        for (auto & settings_for_card : settings) {

            if (settings_for_card.count("mac") == 1 && settings_for_card["mac"] == card.mac) {
                setting_map = settings_for_card;
                break;
            }
        }

        if (setting_map.count("use_for")) card.use_for = setting_map["use_for"];
        if (setting_map.count("ip")) card.ip = setting_map["ip"];
        if (setting_map.count("gateway")) card.gateway = setting_map["gateway"];

        
        save_cards.push_back(card);

            /*
        * And now save the complete set of ethernet cards back to the settings file, ensuring that all hardware
        * ends up in the file automatically but users can change it as needed
        */
        try {
            const std::string settings_path = findOrCreateSettingsDirectory(m_is_air);
            std::string settings_file = settings_path + "/ethernet.conf";
            save_settings(save_cards, settings_file);
        } catch (std::exception &ex) {
            ohd_log(STATUS_LEVEL_EMERGENCY, "Ethernet settings save failed");
        }

        process_card(card);

    }
}

void EthernetCards::process_manifest() {
    try {
        std::ifstream f("/tmp/ethernet_manifest");
        nlohmann::json j;
        f >> j;

        m_ethernet_hotspot_type = string_to_ethernet_hotspot_type(j["hotspot"]);

        for (auto _card : j["cards"]) {

            EthernetCard card;
            card.type = string_to_ethernet_card_type(_card["type"]);
            card.name = _card["name"];
            card.mac = _card["mac"];

            m_ethernet_cards.push_back(card);

        }
    } catch (std::exception &ex) {
        // don't do anything, but send an error message to the user through the status service
        ohd_log(STATUS_LEVEL_EMERGENCY, "Ethernet manifest processing failed");
            
        std::cerr << "Ethernet::process_manifest: " << ex.what() << std::endl;
        return;
    }
}


void EthernetCards::process_card(EthernetCard &card) {
    if (card.use_for == "static") {
        setup_static(card);
        return;
    }

    if (card.use_for == "hotspot") {
        setup_hotspot(card);
        return;
    }

    if (card.use_for == "client") {
        setup_client(card);
        return;
    }

    if (m_is_air) {
        setup_static(card);
    } else {        
        setup_client(card);
    }
}


void EthernetCards::setup_hotspot(EthernetCard &card) {
    std::cout << "Ethernet::setup_hotspot()" << std::endl;

    bool success = false;

    if (m_hotspot_configured) {
        std::cout << "Ethernet::setup_hotspot: already configured with another card" << std::endl;
        return;
    }

    std::stringstream message1;

    message1 << "Setting up ethernet hotspot on " << card.name << std::endl;
    ohd_log(STATUS_LEVEL_INFO, message1.str());
    
    card.use_for = "hotspot";

    {
        // todo: allow the interface address to be configured. this requires changing the dnsmasq config file though, not
        //       just the interface address.
        std::vector<std::string> args { 
            card.name, m_ethernet_hotspot_address, "up",
        };

        success = run_command("ifconfig", args);

        if (!success) {
            ohd_log(STATUS_LEVEL_WARNING, "Failed to enable ethenet hotspot interface");
            return;
        }
    }

    // TODO we should not just call scripts from code
    {
        std::vector<std::string> args { 
            "/usr/local/share/wifibroadcast-scripts/ethernet_hotspot.sh", card.name, m_ethernet_hotspot_address
        };

        success = run_command("/bin/bash", args);

        if (!success) {
            ohd_log(STATUS_LEVEL_WARNING, "Failed to enable dnsmasq on eth hotspot");
            return;
        }
    }

    m_hotspot_configured = true;

    ohd_log(STATUS_LEVEL_INFO, "Ethernet hotspot running");
}



void EthernetCards::setup_static(EthernetCard &card) {
    std::cout << "Ethernet::setup_static()" << std::endl;

    bool success = false;

    std::stringstream message1;

    message1 << "Setting up ethernet static interface " << card.name << std::endl;
    ohd_log(STATUS_LEVEL_INFO, message1.str());
    
    card.use_for = "static";

    if (card.ip.empty()) {
        card.ip = "192.168.3.1/24";
    }

    {
        std::vector<std::string> args { 
            card.name, card.ip, "up",
        };

        success = run_command("ifconfig", args);

        if (!success) {
            ohd_log(STATUS_LEVEL_WARNING, "Failed to enable ethenet interface");
            return;
        }
    }

    if (!card.gateway.empty()) {
        {
            std::vector<std::string> args { 
                "route", "add", "default", "via", card.gateway, "dev", card.name
            };

            success = run_command("ip", args);

            if (!success) {
                ohd_log(STATUS_LEVEL_WARNING, "Failed to enable default route on ethernet card");
                return;
            }
        }
    }

    std::stringstream message2;

    message2 << "Ethernet static interface " << card.name << " running" << std::endl;
    ohd_log(STATUS_LEVEL_INFO, message2.str());
}


void EthernetCards::setup_client(EthernetCard &card) {
    std::cout << "Ethernet::setup_client()" << std::endl;

    bool success = false;

    std::stringstream message1;

    message1 << "Setting up ethernet LAN interface " << card.name << std::endl;
    ohd_log(STATUS_LEVEL_INFO, message1.str());
    
    card.use_for = "client";

    {
        std::vector<std::string> args { 
            "-i", card.name, "--no-ntp"
        };

        success = run_command("pump", args);

        if (!success) {
            ohd_log(STATUS_LEVEL_WARNING, "Failed to enable ethenet interface");
            return;
        }
    }

    std::stringstream message2;

    message2 << "Ethernet LAN interface " << card.name << " running" << std::endl;
    ohd_log(STATUS_LEVEL_INFO, message2.str());
}


void EthernetCards::save_settings(const std::vector<EthernetCard>& cards, std::string settings_file) {
    inja::Environment env;

    // load the ethernet card template, we format it once for each card and write that to the file
    std::ifstream template_file("/usr/local/share/openhd/ethernetcard.template");
    std::string template_s((std::istreambuf_iterator<char>(template_file)),
                            std::istreambuf_iterator<char>());


    std::ofstream out(settings_file);

    // now fill in the template params
    for (auto & card : cards) {
        nlohmann::json data;

        data["type"] = ethernet_card_type_to_string(card.type);
        data["mac"] = card.mac;
        data["name"] = card.name;
        data["vendor"] = card.vendor;
        data["use_for"] = card.use_for;
        data["ip"] = card.ip;
        data["gateway"] = card.gateway;

        inja::Template temp = env.parse(template_s.c_str());
        std::string rendered = env.render(temp, data);

        // and write this card to the settings file
        out << rendered;
        out << "\n\n";
    }
    
    out.close();
}
