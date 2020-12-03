#include <stdio.h>

#include "fwd.h"
#include "loragw_hal.h"
#include "mac-header-decode.h"

LoRaMacParserStatus_t LoRaMacParserData( LoRaMacMessageData_t* macMsg )
{
    if( ( macMsg == 0 ) || ( macMsg->Buffer == 0 ) )
    {
        return LORAMAC_PARSER_ERROR_NPE;
    }

    uint16_t bufItr = 0;

    macMsg->FPort = 0;
    macMsg->FRMPayloadSize = 0;

    macMsg->MHDR.Value = macMsg->Buffer[bufItr++];

    switch (macMsg->MHDR.Bits.MType) {
        case FRAME_TYPE_JOIN_ACCEPT: 
        case FRAME_TYPE_JOIN_REQ: 
            return LORAMAC_PARSER_SUCCESS;
        default:
            break;
    }

    macMsg->FHDR.DevAddr = macMsg->Buffer[bufItr++];
    macMsg->FHDR.DevAddr |= ( ( uint32_t ) macMsg->Buffer[bufItr++] << 8 );
    macMsg->FHDR.DevAddr |= ( ( uint32_t ) macMsg->Buffer[bufItr++] << 16 );
    macMsg->FHDR.DevAddr |= ( ( uint32_t ) macMsg->Buffer[bufItr++] << 24 );

    macMsg->FHDR.FCtrl.Value = macMsg->Buffer[bufItr++];

    macMsg->FHDR.FCnt = macMsg->Buffer[bufItr++];
    macMsg->FHDR.FCnt |= macMsg->Buffer[bufItr++] << 8;

    if( macMsg->FHDR.FCtrl.Bits.FOptsLen <= 15 )
    {
        lgw_memcpy( macMsg->FHDR.FOpts, &macMsg->Buffer[bufItr], macMsg->FHDR.FCtrl.Bits.FOptsLen );
        bufItr = bufItr + macMsg->FHDR.FCtrl.Bits.FOptsLen;
    } else {
        return LORAMAC_PARSER_FAIL;
    }

    // Initialize anyway with zero.

    if( ( macMsg->BufSize - bufItr - LORAMAC_MIC_FIELD_SIZE ) > 0 ) {
        macMsg->FPort = macMsg->Buffer[bufItr++];

        macMsg->FRMPayloadSize = ( macMsg->BufSize - bufItr - LORAMAC_MIC_FIELD_SIZE );
        //lgw_memcpy( macMsg->FRMPayload, &macMsg->Buffer[bufItr], macMsg->FRMPayloadSize );
        macMsg->FRMPayload = macMsg->Buffer + bufItr;
        bufItr = bufItr + macMsg->FRMPayloadSize;
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

    lgw_memcpy( macMsg->JoinNonce, &macMsg->Buffer[bufItr], 3 );
    bufItr = bufItr + 3;

    lgw_memcpy( macMsg->NetID, &macMsg->Buffer[bufItr], 3 );
    bufItr = bufItr + 3;

    macMsg->DevAddr = ( uint32_t ) macMsg->Buffer[bufItr++];
    macMsg->DevAddr |= ( ( uint32_t ) macMsg->Buffer[bufItr++] << 8 );
    macMsg->DevAddr |= ( ( uint32_t ) macMsg->Buffer[bufItr++] << 16 );
    macMsg->DevAddr |= ( ( uint32_t ) macMsg->Buffer[bufItr++] << 24 );

    macMsg->DLSettings.Value = macMsg->Buffer[bufItr++];
    
    macMsg->RxDelay = macMsg->Buffer[bufItr++];

    if( ( macMsg->BufSize - LORAMAC_MIC_FIELD_SIZE - bufItr ) == LORAMAC_CF_LIST_FIELD_SIZE )
    {
        lgw_memcpy( macMsg->CFList, &macMsg->Buffer[bufItr], LORAMAC_CF_LIST_FIELD_SIZE );
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

void decode_mac_pkt_up(LoRaMacMessageData_t* macMsg, void* pkt)
{

    if( ( macMsg == 0 ) || ( macMsg->Buffer == 0 ) ) {
        return;
    }

    int idx = 1;
    char appeui[17] = {'\0'};
    char deveui[17] = {'\0'};
    char cat[3] = {'\0'};
    char pdtype[17] = {'\0'};
    char content[256] = {'\0'};
    char payloadhex[480] = {'\0'};
    uint32_t devaddr;

	struct lgw_pkt_rx_s* p = (struct lgw_pkt_rx_s*)pkt;

    char dr[20] = {'\0'};

    switch (p->datarate) {
        case DR_LORA_SF5:
            strcpy(dr, "SF5 ");
            break;
        case DR_LORA_SF6:
            strcpy(dr, "SF6 ");
            break;
        case DR_LORA_SF7:
            strcpy(dr, "SF7 ");
            break;
        case DR_LORA_SF8:
            strcpy(dr, "SF8 ");
            break;
        case DR_LORA_SF9:
            strcpy(dr, "SF9 ");
            break;
        case DR_LORA_SF10:
            strcpy(dr, "SF10 ");
            break;
        case DR_LORA_SF11:
            strcpy(dr, "SF11 ");
            break;
        case DR_LORA_SF12:
            strcpy(dr, "SF12 ");
            break;
        default:
            break;
    }
    switch (p->bandwidth) {
        case BW_125KHZ:
            strcat(dr, "BW125KHz ");
            break;
        case BW_250KHZ:
            strcat(dr, "BW250KHz ");
            break;
        case BW_500KHZ:
            strcat(dr, "BW500KHz ");
            break;
        default:
            break;

    }

    /* Packet ECC coding rate, 11-13 useful chars */
    switch (p->coderate) {
        case CR_LORA_4_5:
            strcat(dr, "CR4/5");
            break;
        case CR_LORA_4_6:
            strcat(dr, "CR4/6");
            break;
        case CR_LORA_4_7:
            strcat(dr, "CR4/7");
            break;
        case CR_LORA_4_8:
            strcat(dr, "CR4/8");
            break;
        case 0: /* treat the CR0 case (mostly false sync) */
            strcat(dr, "CR off");
            break;
        default:
            break;
    }

    memset(payloadhex, 0, sizeof(payloadhex));
    for (idx = 0; idx < macMsg->FRMPayloadSize; idx++) {
        sprintf(cat, "%02X", macMsg->FRMPayload[idx]);
        strcat(payloadhex, cat);
    }
    
    switch (macMsg->MHDR.Bits.MType) {
        case FRAME_TYPE_DATA_CONFIRMED_UP:
            snprintf(content, sizeof(content), "CONF_UP:{\"ADDR\":\"%08X\", \"Size\":%d, \"Rssi\":%f/%f, \"snr\":%f, \"FCtrl\":[\"ADR\":%u,\"ACK\":%u, \"FPending\":%u, \"FOptsLen\":%u], \"FCnt\":%u, \"FPort\":%u, \"MIC\":\"%08X\"}", macMsg->FHDR.DevAddr, p->size, p->rssic, p->rssis, p->snr, macMsg->FHDR.FCtrl.Bits.Adr, macMsg->FHDR.FCtrl.Bits.Ack, macMsg->FHDR.FCtrl.Bits.FPending, macMsg->FHDR.FCtrl.Bits.FOptsLen, macMsg->FHDR.FCnt, macMsg->FPort, macMsg->MIC);
            sprintf(pdtype, "DATA_CONF_UP");
            break;
        case FRAME_TYPE_DATA_UNCONFIRMED_UP: 
            snprintf(content, sizeof(content), "UNCONF_UP:{\"ADDR\":\"%08X\", \"Size\":%d, \"Rssi\":%f/%f, \"snr\":%f, \"FCtrl\":[\"ADR\":%u,\"ACK\":%u, \"FPending\":%u, \"FOptsLen\":%u], \"FCnt\":%u, \"FPort\":%u, \"MIC\":\"%08X\"}", macMsg->FHDR.DevAddr, p->size, p->rssic, p->rssis, p->snr, macMsg->FHDR.FCtrl.Bits.Adr, macMsg->FHDR.FCtrl.Bits.Ack, macMsg->FHDR.FCtrl.Bits.FPending, macMsg->FHDR.FCtrl.Bits.FOptsLen, macMsg->FHDR.FCnt, macMsg->FPort, macMsg->MIC);
            sprintf(pdtype, "DATA_UNCONF_UP");
            break;
        case FRAME_TYPE_JOIN_REQ: 
            for (idx = 8; idx > 0; idx--) {  //APPEUI
                sprintf(cat, "%02X", macMsg->Buffer[idx]);
                strcat(appeui, cat);
            }
            for (idx = 16; idx > 8; idx--) { //DEVEUI
                sprintf(cat, "%02X", macMsg->Buffer[idx]);
                strcat(deveui, cat);
            }

            snprintf(content, sizeof(content), "JOIN_REQ:{\"Size\":%d, \"Rssi\":%f/%f, \"snr\":%f, \"AppEUI\":\"%s\", \"DevEUI\":\"%s\"}", p->size, p->rssic, p->rssis, p->snr, appeui, deveui);
            for (idx = 0; idx < p->size; idx++) {
                sprintf(cat, "%02X", p->payload[idx]);
                strcat(payloadhex, cat);
            }
            sprintf(pdtype, "JOIN_REQ_UP");

            break;
        default:
            break;
    }
    lgw_db_putpkt(pdtype, (double)p->freq_hz/1e6, dr, macMsg->FHDR.FCnt, NULL, content, payloadhex);
    lgw_log(LOG_PKT, "%s\n", content);
}

void decode_mac_pkt_down(LoRaMacMessageData_t* macMsg, void* pkt)
{

    if( ( macMsg == 0 ) || ( macMsg->Buffer == 0 ) ) {
        return;
    }

    int idx = 1;
    char dr[20] = {'\0'};
    char cat[3] = {'\0'};
    char pdtype[17] = {'\0'};
    char content[256] = {'\0'};
    char payloadhex[480] = {'\0'};

	struct lgw_pkt_tx_s* p = (struct lgw_pkt_tx_s*)pkt;

    LoRaMacMessageJoinAccept_t joinMsg;
    
    switch (p->datarate) {
        case DR_LORA_SF5:
            strcpy(dr, "SF5 ");
            break;
        case DR_LORA_SF6:
            strcpy(dr, "SF6 ");
            break;
        case DR_LORA_SF7:
            strcpy(dr, "SF7 ");
            break;
        case DR_LORA_SF8:
            strcpy(dr, "SF8 ");
            break;
        case DR_LORA_SF9:
            strcpy(dr, "SF9 ");
            break;
        case DR_LORA_SF10:
            strcpy(dr, "SF10 ");
            break;
        case DR_LORA_SF11:
            strcpy(dr, "SF11 ");
            break;
        case DR_LORA_SF12:
            strcpy(dr, "SF12 ");
            break;
        default:
            break;
    }
    switch (p->bandwidth) {
        case BW_125KHZ:
            strcat(dr, "BW125KHz ");
            break;
        case BW_250KHZ:
            strcat(dr, "BW250KHz ");
            break;
        case BW_500KHZ:
            strcat(dr, "BW500KHz ");
            break;
        default:
            break;

    }

    /* Packet ECC coding rate, 11-13 useful chars */
    switch (p->coderate) {
        case CR_LORA_4_5:
            strcat(dr, "CR4/5");
            break;
        case CR_LORA_4_6:
            strcat(dr, "CR4/6");
            break;
        case CR_LORA_4_7:
            strcat(dr, "CR4/7");
            break;
        case CR_LORA_4_8:
            strcat(dr, "CR4/8");
            break;
        case 0: /* treat the CR0 case (mostly false sync) */
            strcat(dr, "CR off");
            break;
        default:
            break;
    }

    memset(payloadhex, 0, sizeof(payloadhex));
    for (idx = 0; idx < macMsg->FRMPayloadSize; idx++) {
        sprintf(cat, "%02X", macMsg->FRMPayload[idx]);
        strcat(payloadhex, cat);
    }

    switch (macMsg->MHDR.Bits.MType) {
        case FRAME_TYPE_DATA_CONFIRMED_DOWN:
            snprintf(content, sizeof(content), "CONF_DOWN:{\"ADDR\":\"%08X\", \"Size\":%d, \"InvertPol\":%s, \"FCtrl\":[\"ADR\":%u,\"ACK\":%u, \"FPending\":%u, \"FOptsLen\":%u], \"FCnt\":%u, \"FPort\":%u, \"MIC\":\"%08X\"}", macMsg->FHDR.DevAddr, p->size, p->invert_pol ? "true":"false", macMsg->FHDR.FCtrl.Bits.Adr, macMsg->FHDR.FCtrl.Bits.Ack, macMsg->FHDR.FCtrl.Bits.FPending, macMsg->FHDR.FCtrl.Bits.FOptsLen, macMsg->FHDR.FCnt, macMsg->FPort, macMsg->MIC);
            sprintf(pdtype, "DATA_CONF_DOWN");
            break;
        case FRAME_TYPE_DATA_UNCONFIRMED_DOWN:
            snprintf(content, sizeof(content), "UNCONF_DOWN:{\"ADDR\":\"%08X\", \"Size\":%d, \"InvertPol\":%s, \"FCtrl\":[\"ADR\":%u,\"ACK\":%u, \"FPending\":%u, \"FOptsLen\":%u], \"FCnt\":%u, \"FPort\":%u, \"MIC\":\"%08X\"}", macMsg->FHDR.DevAddr, p->size, p->invert_pol ? "true":"false", macMsg->FHDR.FCtrl.Bits.Adr, macMsg->FHDR.FCtrl.Bits.Ack, macMsg->FHDR.FCtrl.Bits.FPending, macMsg->FHDR.FCtrl.Bits.FOptsLen, macMsg->FHDR.FCnt, macMsg->FPort, macMsg->MIC);
            sprintf(pdtype, "DATA_UNCONF_DOWN");
            break;
        case FRAME_TYPE_JOIN_ACCEPT: 
            joinMsg.Buffer = p->payload;
            joinMsg.BufSize = p->size;
            if (LoRaMacParserJoinAccept(&joinMsg) != LORAMAC_PARSER_SUCCESS)
                break;
            snprintf(content, sizeof(content), "JOIN_ACCEPT:{\"Size\":%d, \"NetID\":\"%02X%02X%02X\", \"DevAddr\":\"%08X\", \"DLSettings\":{\"RX1DRoffset\":%u, \"RX2DR\":%u}}", p->size, joinMsg.NetID[0], joinMsg.NetID[1], joinMsg.NetID[2], joinMsg.DevAddr, joinMsg.DLSettings.Bits.RX1DRoffset, joinMsg.DLSettings.Bits.RX2DataRate);
            if( ( macMsg->BufSize - LORAMAC_MIC_FIELD_SIZE - 12 ) == LORAMAC_CF_LIST_FIELD_SIZE ) {  //CFLIST
                snprintf(payloadhex, sizeof(payloadhex), "CFLIST:");
                for (idx = 0; idx < LORAMAC_CF_LIST_FIELD_SIZE; idx++) {
                    sprintf(cat, "%02X", joinMsg.CFList[idx]);
                    strcat(payloadhex, cat);
                }
            }
            sprintf(pdtype, "JOIN_ACCEPT_DOWN");
            break;
        default:
            break;
    }

    lgw_db_putpkt(pdtype, (double)p->freq_hz/1e6, dr, macMsg->FHDR.FCnt, NULL, content, payloadhex);
    lgw_log(LOG_PKT, "%s\n", content);
}

