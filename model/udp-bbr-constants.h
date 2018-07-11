/**
 * @file
 * Global constants for udp-bbr ns3 module.
 *
 * @version 0.1.0
 * @author dd
 */


#ifndef UDP_BBR_CONSTANTS_H
#define UDP_BBR_CONSTANTS_H

#include <stdint.h>

const uint32_t DEFAULT_PACKET_SIZE = 1400;
const uint32_t DEFAULT_PAYLOAD_SIZE = 1400;
const uint32_t IPV4_HEADER_SIZE = 20;
const uint32_t UDP_HEADER_SIZE = 8;
const uint32_t IPV4_UDP_OVERHEAD = IPV4_HEADER_SIZE + UDP_HEADER_SIZE;

const uint8_t pic_type_real = 1;
const uint8_t pic_type_fake = 0;

// syncodec parameters
const uint32_t SYNCODEC_DEFAULT_FPS = 30;
enum SyncodecType {
    SYNCODEC_TYPE_PERFECT = 0,
    SYNCODEC_TYPE_FIXFPS,
    SYNCODEC_TYPE_STATS,
    SYNCODEC_TYPE_TRACE,
    SYNCODEC_TYPE_SHARING,
    SYNCODEC_TYPE_HYBRID
};

/**
 * Parameters for the rate shaping buffer as specified in draft-ietf-rmcat-nada
 * These are the default values according to the draft
 * The rate shaping buffer is currently implemented in the sender ns3
 * application (#ns3::RmcatSender ). For other congestion controllers
 * that do not need the rate shaping buffer, you can disable it by
 * setting USE_BUFFER to false.
 */
const bool USE_BUFFER = true;
const float BETA_V = 1e-5;
const float BETA_S = 1e-5;
const uint32_t MAX_QUEUE_SIZE_SANITY = 80 * 1000 * 1000; //bytes

/* topology parameters */
const uint32_t T_MAX_S = 500;  // maximum simulation duration  in seconds
const uint32_t T_TCP_LOG = 1;  // whether to log TCP flows

/* Default topology setting parameters */
const uint32_t WIFI_TOPO_MACQUEUE_MAXNPKTS = 1000;
const uint32_t WIFI_TOPO_ARPCACHE_ALIVE_TIMEOUT = 24 * 60 * 60; // 24 hours
const float WIFI_TOPO_2_4GHZ_PATHLOSS_EXPONENT = 3.0f;
const float WIFI_TOPO_2_4GHZ_PATHLOSS_REFLOSS = 40.0459f;
const uint32_t WIFI_TOPO_CHANNEL_WIDTH = 20;   // default channel width: 20MHz

#endif /* UDP_BBR_CONSTANTS_H */
