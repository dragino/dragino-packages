/*!
 * \file      mac-header-decode.h
 *
 * \brief     LoRa MAC layer header type definitions
 *
 */
#ifndef __LORAMAC_HEADER_DECODE_H__
#define __LORAMAC_HEADER_DECODE_H__

#include <stdint.h>

/*! Frame header (FHDR) maximum field size */
#define LORAMAC_FHDR_MAX_FIELD_SIZE             22

/*! FHDR Device address field size */
#define LORAMAC_FHDR_DEV_ADD_FIELD_SIZE         4

/*! FHDR Frame control field size */
#define LORAMAC_FHDR_F_CTRL_FIELD_SIZE          1

/*! FHDR Frame control field size */
#define LORAMAC_FHDR_F_CNT_FIELD_SIZE           2

/*! FOpts maximum field size */
#define LORAMAC_FHDR_F_OPTS_MAX_FIELD_SIZE      15

/*! MIC field size */
#define LORAMAC_MIC_FIELD_SIZE                  4

/*! CFList field size */
#define LORAMAC_CF_LIST_FIELD_SIZE              16

typedef struct {
    uint32_t devaddr;
    uint8_t appskey[16];
    uint8_t nwkskey[16];
    char devaddr_str[17];
    char appskey_str[33];
    char nwkskey_str[33];
} devinfo_s;

/*!
 * LoRaMAC header field definition (MHDR field)
 *
 * LoRaWAN Specification V1.0.2, chapter 4.2
 */
typedef union uLoRaMacHeader
{
    /*!
     * Byte-access to the bits
     */
    uint8_t Value;
    /*!
     * Structure containing single access to header bits
     */
    struct sMacHeaderBits
    {
        /*!
         * Message type
         */
        uint8_t MType           : 3;
        /*!
         * RFU
         */
        uint8_t RFU             : 3;
        /*!
         * Major version
         */
        uint8_t Major           : 2;
    }Bits;
}LoRaMacHeader_t;

/*!
 * LoRaMAC frame control field definition (FCtrl)
 *
 * LoRaWAN Specification V1.0.2, chapter 4.3.1
 */
typedef union uLoRaMacFrameCtrl
{
    /*!
     * Byte-access to the bits
     */
    uint8_t Value;
    /*!
     * Structure containing single access to bits
     */
    struct sCtrlBits
    {
        /*!
         * ADR control in frame header
         */
        uint8_t Adr             : 1;
        /*!
         * ADR acknowledgment request bit
         */
        uint8_t AdrAckReq       : 1;
        /*!
         * Message acknowledge bit
         */
        uint8_t Ack             : 1;
        /*!
         * Frame pending bit
         */
        uint8_t FPending        : 1;
        /*!
         * Frame options length
         */
        uint8_t FOptsLen        : 4;
    }Bits;
}LoRaMacFrameCtrl_t;

/*!
 * LoRaMac Frame header (FHDR)
 *
 * LoRaWAN Specification V1.0.2, chapter 4.3.1
 */
typedef struct sLoRaMacFrameHeader
{
    /*!
     * Device address
     */
    uint32_t DevAddr;
    /*!
     * Frame control field
     */
    LoRaMacFrameCtrl_t FCtrl;
    /*!
     * Frame counter
     */
    uint16_t FCnt;
    /*!
     * FOpts field may transport  MAC commands (opt. 0-15 Bytes)
     */
    uint8_t FOpts[LORAMAC_FHDR_F_OPTS_MAX_FIELD_SIZE];
}LoRaMacFrameHeader_t;

/*! \} addtogroup LORAMAC */

/*!
 * LoRaMAC frame types
 *
 * LoRaWAN Specification V1.0.2, chapter 4.2.1, table 1
 */
typedef enum eLoRaMacFrameType
{
    /*!
     * LoRaMAC join request frame
     */
    FRAME_TYPE_JOIN_REQ              = 0x00,
    /*!
     * LoRaMAC join accept frame
     */
    FRAME_TYPE_JOIN_ACCEPT           = 0x01,
    /*!
     * LoRaMAC unconfirmed up-link frame
     */
    FRAME_TYPE_DATA_UNCONFIRMED_UP   = 0x02,
    /*!
     * LoRaMAC unconfirmed down-link frame
     */
    FRAME_TYPE_DATA_UNCONFIRMED_DOWN = 0x03,
    /*!
     * LoRaMAC confirmed up-link frame
     */
    FRAME_TYPE_DATA_CONFIRMED_UP     = 0x04,
    /*!
     * LoRaMAC confirmed down-link frame
     */
    FRAME_TYPE_DATA_CONFIRMED_DOWN   = 0x05,
    /*!
     * LoRaMAC proprietary frame
     */
    FRAME_TYPE_PROPRIETARY           = 0x07,
}LoRaMacFrameType_t;

typedef struct sLoRaMacMessageData
{
    /*!
     * Serialized message buffer
     */
    uint8_t* Buffer;
    /*!
     * Size of serialized message buffer
     */
    uint8_t BufSize;
    /*!
     * MAC header
     */
    LoRaMacHeader_t MHDR;
    /*!
     * Frame header (FHDR)
     */
    LoRaMacFrameHeader_t FHDR;
    /*!
     * Port field (opt.)
     */
    uint8_t FPort;
    /*!
     * Frame payload may contain MAC commands or data (opt.)
     */
    uint8_t* FRMPayload;
    /*!
     * Size of frame payload (not included in LoRaMac messages) 
     */
    uint8_t FRMPayloadSize;
    /*!
     * Message integrity code (MIC)
     */
    uint32_t MIC;
}LoRaMacMessageData_t;

/*!
 * LoRaMac message type enumerator
 */
typedef enum eLoRaMacMessageType
{
    /*!
     * Join-request message
     */
    LORAMAC_MSG_TYPE_JOIN_REQUEST,
    /*!
     * Rejoin-request type 1 message
     */
    LORAMAC_MSG_TYPE_RE_JOIN_1,
    /*!
     * Rejoin-request type 1 message
     */
    LORAMAC_MSG_TYPE_RE_JOIN_0_2,
    /*!
     * Join-accept message
     */
    LORAMAC_MSG_TYPE_JOIN_ACCEPT,
    /*!
     * Data MAC messages
     */
    LORAMAC_MSG_TYPE_DATA,
    /*!
     * Undefined message type
     */
    LORAMAC_MSG_TYPE_UNDEF,
}LoRaMacMessageType_t;

/*!
 * LoRaMAC field definition of DLSettings
 *
 * LoRaWAN Specification V1.0.2, chapter 5.4
 */
typedef union uLoRaMacDLSettings
{   
    /*!
     * Byte-access to the bits
     */
    uint8_t Value;
    /*!
     * Structure containing single access to header bits
     */
    struct sDLSettingsBits
    {   
        /*!
         * Data rate of a downlink using the second receive window
         */
        uint8_t RX2DataRate     : 4;
        /*!
         * Offset between up and downlink datarate of first reception slot
         */
        uint8_t RX1DRoffset     : 3;
        /*!
         * Indicates network server LoRaWAN implementation version 1.1 or later.
         */
        uint8_t OptNeg          : 1;
    }Bits;
}LoRaMacDLSettings_t;

/*!
 * LoRaMac type for Join-accept message
 */
typedef struct sLoRaMacMessageJoinAccept
{
    /*!
     * Serialized message buffer
     */
    uint8_t* Buffer;
    /*!
     * Size of serialized message buffer
     */
    uint8_t BufSize;
    /*!
     * MAC header
     */
    LoRaMacHeader_t MHDR;
    /*!
     *  Server Nonce ( 3 bytes )
     */
    uint8_t JoinNonce[3];
    /*!
     * Network ID ( 3 bytes )
     */
    uint8_t NetID[3];
    /*!
     * Device address
     */
    uint32_t DevAddr;
    /*!
     * Device address
     */
    LoRaMacDLSettings_t DLSettings;
    /*!
     * Delay between TX and RX
     */
    uint8_t RxDelay;
    /*!
     * List of channel frequencies (opt.)
     */
    uint8_t CFList[16];
    /*!
     * Message integrity code (MIC)
     */
    uint32_t MIC;
}LoRaMacMessageJoinAccept_t;


/*!
 * LoRaMac Parser Status
 */
typedef enum eLoRaMacParserStatus
{
    /*!
     * No error occurred
     */
    LORAMAC_PARSER_SUCCESS = 0,
    /*!
     * Failure during parsing occurred
     */
    LORAMAC_PARSER_FAIL,
    /*!
     * Null pointer exception
     */
    LORAMAC_PARSER_ERROR_NPE,
    /*!
     * Undefined Error occurred
     */
    LORAMAC_PARSER_ERROR,
}LoRaMacParserStatus_t;

LoRaMacParserStatus_t LoRaMacParserData( LoRaMacMessageData_t* macMsg );

void decode_mac_pkt_up(LoRaMacMessageData_t* macMsg, void* pkt);
void decode_mac_pkt_down(LoRaMacMessageData_t* macMsg, void* pkt);

#endif // __LORAMAC_HEADER_DECODE_H_
