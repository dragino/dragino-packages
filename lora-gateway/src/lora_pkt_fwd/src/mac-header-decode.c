#include <stdio.h>
#include <string.h>

#include "trace.h"
#include "utilities.h"
#include "mac-header-decode.h"

LoRaMacParserStatus_t LoRaMacParserData( LoRaMacMessageData_t* macMsg )
{
    if( ( macMsg == 0 ) || ( macMsg->Buffer == 0 ) )
    {
        return LORAMAC_PARSER_ERROR_NPE;
    }

    uint16_t bufItr = 0;

    macMsg->MHDR.Value = macMsg->Buffer[bufItr++];

    macMsg->FHDR.DevAddr = macMsg->Buffer[bufItr++];
    macMsg->FHDR.DevAddr |= ( ( uint32_t ) macMsg->Buffer[bufItr++] << 8 );
    macMsg->FHDR.DevAddr |= ( ( uint32_t ) macMsg->Buffer[bufItr++] << 16 );
    macMsg->FHDR.DevAddr |= ( ( uint32_t ) macMsg->Buffer[bufItr++] << 24 );

    macMsg->FHDR.FCtrl.Value = macMsg->Buffer[bufItr++];

    macMsg->FHDR.FCnt = macMsg->Buffer[bufItr++];
    macMsg->FHDR.FCnt |= macMsg->Buffer[bufItr++] << 8;

    if( macMsg->FHDR.FCtrl.Bits.FOptsLen <= 15 )
    {
        memcpy1( macMsg->FHDR.FOpts, &macMsg->Buffer[bufItr], macMsg->FHDR.FCtrl.Bits.FOptsLen );
        bufItr = bufItr + macMsg->FHDR.FCtrl.Bits.FOptsLen;
    }
    else
    {
        return LORAMAC_PARSER_FAIL;
    }

    // Initialize anyway with zero.
    macMsg->FPort = 0;
    //macMsg->FRMPayloadSize = 0;

    if( ( macMsg->BufSize - bufItr - LORAMAC_MIC_FIELD_SIZE ) > 0 )
    {
        macMsg->FPort = macMsg->Buffer[bufItr++];

    }

    macMsg->MIC = ( uint32_t ) macMsg->Buffer[( macMsg->BufSize - LORAMAC_MIC_FIELD_SIZE )];
    macMsg->MIC |= ( ( uint32_t ) macMsg->Buffer[( macMsg->BufSize - LORAMAC_MIC_FIELD_SIZE ) + 1] << 8 );
    macMsg->MIC |= ( ( uint32_t ) macMsg->Buffer[( macMsg->BufSize - LORAMAC_MIC_FIELD_SIZE ) + 2] << 16 );
    macMsg->MIC |= ( ( uint32_t ) macMsg->Buffer[( macMsg->BufSize - LORAMAC_MIC_FIELD_SIZE ) + 3] << 24 );

    return LORAMAC_PARSER_SUCCESS;
}

LoRaMacParserStatus_t LoRaMacParserJoinAccept( LoRaMacMessageJoinAccept_t* macMsg )
{
    if( ( macMsg == 0 ) || ( macMsg->Buffer == 0 ) )
    {
        return LORAMAC_PARSER_ERROR_NPE;
    }

    uint16_t bufItr = 0;

    macMsg->MHDR.Value = macMsg->Buffer[bufItr++];

    memcpy1( macMsg->JoinNonce, &macMsg->Buffer[bufItr], 3 );
    bufItr = bufItr + 3;

    memcpy1( macMsg->NetID, &macMsg->Buffer[bufItr], 3 );
    bufItr = bufItr + 3;

    macMsg->DevAddr = ( uint32_t ) macMsg->Buffer[bufItr++];
    macMsg->DevAddr |= ( ( uint32_t ) macMsg->Buffer[bufItr++] << 8 );
    macMsg->DevAddr |= ( ( uint32_t ) macMsg->Buffer[bufItr++] << 16 );
    macMsg->DevAddr |= ( ( uint32_t ) macMsg->Buffer[bufItr++] << 24 );

    macMsg->DLSettings.Value = macMsg->Buffer[bufItr++];

    macMsg->RxDelay = macMsg->Buffer[bufItr++];

    if( ( macMsg->BufSize - LORAMAC_MIC_FIELD_SIZE - bufItr ) == LORAMAC_CF_LIST_FIELD_SIZE )
    {
        memcpy1( macMsg->CFList, &macMsg->Buffer[bufItr], LORAMAC_CF_LIST_FIELD_SIZE );
        bufItr = bufItr + LORAMAC_CF_LIST_FIELD_SIZE;
    }
    else if( ( macMsg->BufSize - LORAMAC_MIC_FIELD_SIZE - bufItr ) > 0 )
    {
        return LORAMAC_PARSER_FAIL;
    }

    macMsg->MIC = ( uint32_t ) macMsg->Buffer[bufItr++];
    macMsg->MIC |= ( ( uint32_t ) macMsg->Buffer[bufItr++] << 8 );
    macMsg->MIC |= ( ( uint32_t ) macMsg->Buffer[bufItr++] << 16 );
    macMsg->MIC |= ( ( uint32_t ) macMsg->Buffer[bufItr++] << 24 );

    return LORAMAC_PARSER_SUCCESS;
}

LoRaMacParserStatus_t LoRaMacParserJoinReques( LoRaMacMessageJoinRequest_t* macMsg )
{
    if( ( macMsg == 0 ) || ( macMsg->Buffer == 0 ) )
    {
        return LORAMAC_PARSER_ERROR_NPE;
    }

    uint16_t bufItr = 0;

    macMsg->MHDR.Value = macMsg->Buffer[bufItr++];

    memcpy1( macMsg->JoinEUI, &macMsg->Buffer[bufItr], 8 );
    bufItr = bufItr + 8;

    memcpy1( macMsg->DevEUI, &macMsg->Buffer[bufItr], 8 );
    bufItr = bufItr + 8;

    macMsg->DevNonce = ( uint16_t ) macMsg->Buffer[bufItr++];
    macMsg->DevNonce |= ( ( uint16_t ) macMsg->Buffer[bufItr++] << 8 );

    macMsg->MIC = ( uint32_t ) macMsg->Buffer[bufItr++];
    macMsg->MIC |= ( ( uint32_t ) macMsg->Buffer[bufItr++] << 8 );
    macMsg->MIC |= ( ( uint32_t ) macMsg->Buffer[bufItr++] << 16 );
    macMsg->MIC |= ( ( uint32_t ) macMsg->Buffer[bufItr++] << 24 );

    return LORAMAC_PARSER_SUCCESS;
}

void printf_mac_header( LoRaMacMessageData_t* macMsg )
{
    int idx = 1;
    char appeui[17] = {'\0'};
    char deveui[17] = {'\0'};
    char netid[8] = {'\0'};
    char cat[3] = {'\0'};
    uint32_t devaddr;
    uint16_t devnonce;

    if( ( macMsg == 0 ) || ( macMsg->Buffer == 0 ) )
    {
        return;
    }
    
    switch (macMsg->MHDR.Bits.MType) {
        case FRAME_TYPE_DATA_CONFIRMED_UP:
            MSG_DEBUG(DEBUG_PKT_FWD, "PKT_FWD~ DATA_CONF_UP-> {\"DevAddr\": \"%08X\", \"FCtrl\": [\"ADR\": %u, \"ADRACKReq\": %u, \"ACK\": %u, \"RFU\" : \"RFU\", \"FOptsLen\": %u], \"FCnt\": %u, \"FPort\": %u, \"MIC\": \"%08X\"}\n", 
                    macMsg->FHDR.DevAddr, 
                    macMsg->FHDR.FCtrl.Bits.Adr,
                    macMsg->FHDR.FCtrl.Bits.AdrAckReq,
                    macMsg->FHDR.FCtrl.Bits.Ack,
                    macMsg->FHDR.FCtrl.Bits.FOptsLen,
                    macMsg->FHDR.FCnt,
                    macMsg->FPort,
                    macMsg->MIC);
            break;
        case FRAME_TYPE_DATA_UNCONFIRMED_UP: 
            MSG_DEBUG(DEBUG_PKT_FWD, "PKT_FWD~ DATA_UNCONF_UP-> {\"DevAddr\": \"%08X\", \"FCtrl\": [\"ADR\": %u, \"ADRACKReq\": %u, \"ACK\": %u, \"RFU\" : \"RFU\", \"FOptsLen\": %u], \"FCnt\": %u, \"FPort\": %u, \"MIC\": \"%08X\"}\n", 
                    macMsg->FHDR.DevAddr, 
                    macMsg->FHDR.FCtrl.Bits.Adr,
                    macMsg->FHDR.FCtrl.Bits.AdrAckReq,
                    macMsg->FHDR.FCtrl.Bits.Ack,
                    macMsg->FHDR.FCtrl.Bits.FOptsLen,
                    macMsg->FHDR.FCnt,
                    macMsg->FPort,
                    macMsg->MIC);
            break;
        case FRAME_TYPE_DATA_CONFIRMED_DOWN:
            MSG_DEBUG(DEBUG_PKT_FWD, "PKT_FWD~ DATA_CONF_DOWN<- {\"DevAddr\": \"%08X\", \"FCtrl\": [\"ADR\": %u, \"RFU\": \"RFU\", \"ACK\": %u, \"FPending\" : %u, \"FOptsLen\": %u], \"FCnt\": %u, \"FPort\": %u, \"MIC\": \"%08X\"}\n", 
                    macMsg->FHDR.DevAddr, 
                    macMsg->FHDR.FCtrl.Bits.Adr,
                    macMsg->FHDR.FCtrl.Bits.Ack,
                    macMsg->FHDR.FCtrl.Bits.FPending,
                    macMsg->FHDR.FCtrl.Bits.FOptsLen,
                    macMsg->FHDR.FCnt,
                    macMsg->FPort,
                    macMsg->MIC);
            break;
        case FRAME_TYPE_DATA_UNCONFIRMED_DOWN:
            MSG_DEBUG(DEBUG_PKT_FWD, "PKT_FWD~ DATA_UNCONF_DOWN<- {\"DevAddr\": \"%08X\", \"FCtrl\": [\"ADR\": %u, \"RFU\": \"RFU\", \"ACK\": %u, \"FPending\" : %u, \"FOptsLen\": %u], \"FCnt\": %u, \"FPort\": %u, \"MIC\": \"%08X\"}\n", 
                    macMsg->FHDR.DevAddr, 
                    macMsg->FHDR.FCtrl.Bits.Adr,
                    macMsg->FHDR.FCtrl.Bits.Ack,
                    macMsg->FHDR.FCtrl.Bits.FPending,
                    macMsg->FHDR.FCtrl.Bits.FOptsLen,
                    macMsg->FHDR.FCnt,
                    macMsg->FPort,
                    macMsg->MIC);
            break;
        case FRAME_TYPE_JOIN_ACCEPT: 
            for (idx = 4; idx < 4 + 3; idx++) {
                sprintf(cat, "%02X", macMsg->Buffer[idx]);
                strcat(netid, cat);
            }
            devaddr = ( uint32_t ) macMsg->Buffer[idx++];
            devaddr |= ( ( uint32_t ) macMsg->Buffer[idx++] << 8 );
            devaddr |= ( ( uint32_t ) macMsg->Buffer[idx++] << 16 );
            devaddr |= ( ( uint32_t ) macMsg->Buffer[idx++] << 24 );
            MSG_DEBUG(DEBUG_PKT_FWD, "PKT_FWD~ JOIN_ACCEPT+ {\"NetID\": \"%s\", \"DevAddr\": \"%08X\"}\n", netid, devaddr);
            break;
        case FRAME_TYPE_JOIN_REQ: 
            for (idx = 8; idx > 0; idx--) {
                sprintf(cat, "%02X", macMsg->Buffer[idx]);
                strcat(appeui, cat);
            }
            for (idx = 16; idx > 8; idx--) {
                sprintf(cat, "%02X", macMsg->Buffer[idx]);
                strcat(deveui, cat);
            }

            devnonce = (uint16_t)macMsg->Buffer[idx++];
            devnonce |= ((uint16_t)macMsg->Buffer[idx++] << 8);

            MSG_DEBUG(DEBUG_PKT_FWD, "PKT_FWD~ JOIN_REQ+ {\"AppEUI\":,\"%s\", \"DevEUI\":,\"%s\", \"DevNonce\": \"%u\"}\n", appeui, deveui, devnonce);
            break;
        default:
            break;
    }
}

int filter_by_mac(LoRaMacMessageData_t* macMsg, uint8_t fport, uint32_t devaddr, uint8_t len) {
    if( ( macMsg == 0 ) || ( macMsg->Buffer == 0 ) )
    {
        return -1;
    }
    switch (macMsg->MHDR.Bits.MType) {
        case FRAME_TYPE_DATA_CONFIRMED_UP:
            MSG_DEBUG(DEBUG_PKT_FWD, "DATA_CONF_UP: {\"DevAddr\": \"%08X\", \"FCtrl\": [\"ADR\": %u, \"ADRACKReq\": %u, \"ACK\": %u, \"RFU\" : \"RFU\", \"FOptsLen\": %u], \"FCnt\": %u, \"FPort\": %u, \"MIC\": \"%08X\"}\n", 
                    macMsg->FHDR.DevAddr, 
                    macMsg->FHDR.FCtrl.Bits.Adr,
                    macMsg->FHDR.FCtrl.Bits.AdrAckReq,
                    macMsg->FHDR.FCtrl.Bits.Ack,
                    macMsg->FHDR.FCtrl.Bits.FOptsLen,
                    macMsg->FHDR.FCnt,
                    macMsg->FPort,
                    macMsg->MIC);
            if (fport != 0 && (macMsg->FPort != fport))
                return -1;
            if (devaddr != 0 && ((macMsg->FHDR.DevAddr >> (32-len)) != devaddr ))
                return -2;
            break;
        case FRAME_TYPE_DATA_UNCONFIRMED_UP: 
            MSG_DEBUG(DEBUG_PKT_FWD, "DATA_UNCONF_UP:{\"DevAddr\": \"%08X\", \"FCtrl\": [\"ADR\": %u, \"ADRACKReq\": %u, \"ACK\": %u, \"RFU\" : \"RFU\", \"FOptsLen\": %u], \"FCnt\": %u, \"FPort\": %u, \"MIC\": \"%08X\"}\n", 
                    macMsg->FHDR.DevAddr, 
                    macMsg->FHDR.FCtrl.Bits.Adr,
                    macMsg->FHDR.FCtrl.Bits.AdrAckReq,
                    macMsg->FHDR.FCtrl.Bits.Ack,
                    macMsg->FHDR.FCtrl.Bits.FOptsLen,
                    macMsg->FHDR.FCnt,
                    macMsg->FPort,
                    macMsg->MIC);
            if (fport != 0 && (macMsg->FPort != fport))
                return -1;
            if (devaddr != 0 && ((macMsg->FHDR.DevAddr >> (32-len)) != devaddr ))
                return -2;
            break;
        case FRAME_TYPE_DATA_CONFIRMED_DOWN:
            MSG_DEBUG(DEBUG_PKT_FWD, "DATA_CONF_DOWN:{\"DevAddr\": \"%08X\", \"FCtrl\": [\"ADR\": %u, \"RFU\": \"RFU\", \"ACK\": %u, \"FPending\" : %u, \"FOptsLen\": %u], \"FCnt\": %u, \"FPort\": %u, \"MIC\": \"%08X\"}\n", 
                    macMsg->FHDR.DevAddr, 
                    macMsg->FHDR.FCtrl.Bits.Adr,
                    macMsg->FHDR.FCtrl.Bits.Ack,
                    macMsg->FHDR.FCtrl.Bits.FPending,
                    macMsg->FHDR.FCtrl.Bits.FOptsLen,
                    macMsg->FHDR.FCnt,
                    macMsg->FPort,
                    macMsg->MIC);
            break;
        case FRAME_TYPE_DATA_UNCONFIRMED_DOWN:
            MSG_DEBUG(DEBUG_PKT_FWD, "DATA_UNCONF_DOWN:{\"DevAddr\": \"%08X\", \"FCtrl\": [\"ADR\": %u, \"RFU\": \"RFU\", \"ACK\": %u, \"FPending\" : %u, \"FOptsLen\": %u], \"FCnt\": %u, \"FPort\": %u, \"MIC\": \"%08X\"}\n", 
                    macMsg->FHDR.DevAddr, 
                    macMsg->FHDR.FCtrl.Bits.Adr,
                    macMsg->FHDR.FCtrl.Bits.Ack,
                    macMsg->FHDR.FCtrl.Bits.FPending,
                    macMsg->FHDR.FCtrl.Bits.FOptsLen,
                    macMsg->FHDR.FCnt,
                    macMsg->FPort,
                    macMsg->MIC);
            break;
        case FRAME_TYPE_JOIN_ACCEPT: 
            MSG_DEBUG(DEBUG_PKT_FWD, "JOIN_ACCEPT:{Message ...}\n");
            break;
        case FRAME_TYPE_JOIN_REQ: 
            MSG_DEBUG(DEBUG_PKT_FWD, "JOIN_REQ:{Message ...}\n");
            break;
        default:
            break;
    }
    return 0;
}
