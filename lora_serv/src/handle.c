/*
  ____  ____      _    ____ ___ _   _  ___  
  |  _ \|  _ \    / \  / ___|_ _| \ | |/ _ \ 
  | | | | |_) |  / _ \| |  _ | ||  \| | | | |
  | |_| |  _ <  / ___ \ |_| || || |\  | |_| |
  |____/|_| \_\/_/   \_\____|___|_| \_|\___/ 

Description:


License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: skerlan

*/

#include "handle.h"

/*keys just for testing*/
#define DEFAULT_APPEUI  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
#define DEFALUT_DEVEUI	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }

/*stream direction*/
#define UP                                     0
#define DOWN                                   1

#define JOIN_ACC_SIZE                          17

/* ----------------------------------------------------------------- */
/* ------------------ PRIVATE function ----------------------------- */

/*reverse memory copy*/
static void revercpy( uint8_t *dst, const uint8_t *src, int size );
/*transform the array of uint8_t to hexadecimal string*/
static void i8_to_hexstr(uint8_t* uint, char* str, int size);

/*prepare the frame payload and compute the session keys*/
static void as_prepare_frame(uint8_t *frame_payload, uint16_t devNonce, uint8_t* appKey, uint32_t devAddr, uint8_t *nwkSKey, uint8_t *appSKey);

/*prepare the frame of command downstream*/
static void nc_prepare_frame(uint8_t type, uint32_t devAddr, uint32_t downcnt, const uint8_t* payload, const uint8_t* nwkSKey, int payload_size, uint8_t* frame, int* frame_size);

/* -------------------------------------------------------------------------------- */
/*!
 * LoRaMAC header field definition (MHDR field)
 * Copy from LoRaMac.h
 * LoRaWAN Specification V1.0, chapter 4.2
 */
typedef union uLoRaMacHeader {
    /*!
     * Byte-access to the bits
     */
    uint8_t Value;
    /*!
     * Structure containing single access to header bits
     */
    struct sHdrBits
    {
        /*!
         * Major version
         */
        uint8_t Major           : 2;
        /*!
         * RFU
         */
        uint8_t RFU             : 3;
        /*!
         * Message type
         */
        uint8_t MType           : 3;
    }Bits;
}LoRaMacHeader_t;

/*!
 * LoRaMAC frame control field definition (FCtrl)
 * Copy from LoRaMAC.h
 * LoRaWAN Specification V1.0, chapter 4.3.1
 */
typedef union uLoRaMacFrameCtrl {
    /*!
     * Byte-access to the bits
     */
    uint8_t Value;
    struct sCtrlBits
    {
        /*!
         * Frame options length
         */
        uint8_t FOptsLen        : 4;
        /*!
         * Frame pending bit
         */
        uint8_t FPending        : 1;
        /*!
         * Message acknowledge bit
         */
        uint8_t Ack             : 1;
        /*!
         * ADR acknowledgment request bit
         */
        uint8_t AdrAckReq       : 1;
        /*!
         * ADR control in frame header
         */
        uint8_t Adr             : 1;
    }Bits;
}LoRaMacFrameCtrl_t;

/*!
 * MAC command DataRate_TxPower definition
 * LoRaWAN Specification V1.0, chapter 5.2
 */
typedef union uMacCommandDataRateTxPower{
	uint8_t Value;
	struct sDataRateTxPower{
		uint8_t TxPower             :4;
		uint8_t DataRate            :4;
	}Bits;
}MacCmdDataRateTxPower_t;

/*!
 * MAC command Redundancy definition
 * LoRaWAN Specification V1.0, chapter 5.2
 */
typedef union uMacCommandRedundancy{
	uint8_t Value;
	struct sRedundancy{
		uint8_t NbRep               :4;
		uint8_t chMaskCntl          :3;
		uint8_t RFU                 :1;
	}Bits;
}MacCmdRedundancy_t;

/*!
 * MAC command DLSettings definition
 * LoRaWAN Specification V1.0, chapter 5.4
 */
typedef union uMacCommandDLSettings{
	uint8_t Value;
	struct sDLSettings{
		uint8_t RX2DataRate         :4;
		uint8_t RX1DRoffset         :3;
		uint8_t RFU                 :1;
	}Bits;
}MacCmdDLSettings_t;

/*!
 * MAC command DLSettings definition
 * LoRaWAN Specification V1.0, chapter 5.4
 */
typedef union uMacCommandDrRange{
	uint8_t Value;
	struct sDrRange{
		uint8_t MinDR         :4;
		uint8_t MaxDR         :4;
	}Bits;
}MacCmdDrRange_t;

/*!
 * MAC command Settings definition
 * LoRaWAN Specification V1.0, chapter 5.7
 */
typedef union uMacCommandSettings{
	uint8_t Value;
	struct sSettings{
		uint8_t Del           :4;
		uint8_t RFU           :4;
	}Bits;
}MacCmdSettings_t;

typedef union uMacCommandADRStatus{
	uint8_t Value;
	struct sADRStatus{
		uint8_t ChannelMaskAck :1;
		uint8_t DatarateAck    :1;
		uint8_t PowerAck       :1;
		uint8_t RFU            :5;
	}Bits;
}MacCmdADRStatus_t;

typedef union uMacCommandRXStatus{
	uint8_t Value;
	struct sRXStatus{
		uint8_t ChannelAck     :1;
		uint8_t RX2DatarateAck :1;
		uint8_t RX1DRoffsetAck :1;
		uint8_t RFU            :5;
	}Bits;
}MacCmdRXStatus_t;

typedef union uMacCommandChannelStatus{
	uint8_t Value;
	struct sChannelStatus{
		uint8_t ChannelFreqAck      :1;
		uint8_t DatarateRangeAck    :1;
		uint8_t RFU                 :6;
	}Bits;
}MacCmdChannelStatus_t;

typedef union uMacCommandMargin{
	uint8_t Value;
	struct sMargin{
		uint8_t Margin  :6;
		uint8_t RFU     :2;
	}Bits;
}MacCmdMargin_t;

/*!
 * LoRaMAC frame types
 * Copy from LoRaMAC.h
 * LoRaWAN Specification V1.0, chapter 4.2.1, table 1
 */
typedef enum eLoRaMacFrameType {
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
     * LoRaMAC RFU frame
     */
    FRAME_TYPE_RFU                   = 0x06,
    /*!
     * LoRaMAC proprietary frame
     */
    FRAME_TYPE_PROPRIETARY           = 0x07,
}LoRaMacFrameType_t;

/*!
 * LoRaMAC DLSettings field definition
 * LoRaWAN Specification V1.0, chapter 5.4
 */
typedef union uLoRaMacDLSettings{
	uint8_t Value;
	struct sDLSBits{
		uint8_t RX2DataRate      : 4;
		uint8_t RX1DRoffset      : 3;
		uint8_t RFU              : 1;
	}Bits;
}LoRaMacDLSettings_t;

/*!
 * LoRaMAC DLSettings field definition
 * LoRaWAN Specification V1.0, chapter 6.2.5
 */
typedef union uLoRaMacRxDelay{
	uint8_t Value;
	struct sRxDBits{
		uint8_t Del              : 4;
		uint8_t RFU              : 4;
	}Bits;
}LoRaMacRxDelay_t;

/*
//session key and app keys
uint8_t nwkSKey[16]={ 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C };
uint8_t appSKey[16]={ 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C };
uint8_t appKey[16]= { 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C };
*/

/* --------------------------------------------------------------------------- */
/* ------- handle message process -------------------------------------------- */

void ns_msg_handle(struct jsondata* result, struct metadata* meta, uint8_t* payload) {

	uint8_t nwkSKey[16];
	uint8_t appSKey[16];
	uint8_t appKey[16];

	int size = meta->size; 	/*the length of payload*/
	LoRaMacHeader_t macHdr; /*the MAC header*/
	/* json args for parsing data to json string*/
	JSON_Value *root_value;
	JSON_Object * root_object;

	char* str = NULL;

        char outstr[MAXSTRSIZE];

        int outint = 0;

	/*typical fields for join request message*/
	uint8_t appEui[8];
	uint8_t devEui[8];
	uint16_t devNonce = 0;
	/*store the hexadecimal format of eui*/
	char  appEui_hex[17];
	char  devEui_hex[17];

	/*typical fields for confirmed/unconfirmed message*/
	uint32_t devAddr=0;
	LoRaMacFrameCtrl_t fCtrl;
	uint32_t fopts_len;/*the size of foptions field*/
	uint8_t adr;       /*indicate if ADR is permitted*/
	uint16_t upCnt = 0;
	uint8_t fopts[15];
	uint8_t fport;
	uint8_t fpayload[LORAMAC_FRAME_MAXPAYLOAD];/*the frame payload*/
	char fpayload_b64[341];
	char fopts_b64[21];
	char* maccmd;
	int  encmd; /*indicate whether the MAC command is encrypted */
	int size_b64; /*the size of the fpayload_b64*/

	uint32_t mic = 0;
	uint32_t cal_mic;/*the MIC value calculated by the payload*/
	int i;

	/*varibles used to distinguish the repeated message*/
	unsigned int pre_timestamp = 0;
	unsigned int pre_devNonce = 0;
	unsigned int pre_upCnt = 0;
	unsigned int pre_rfchannel = 2;
	unsigned int pre_tmst_max;
	unsigned int pre_tmst_min;

	/*analyse the type of the message*/
	macHdr.Value = payload[0];
	switch (macHdr.Bits.MType) {
		case FRAME_TYPE_JOIN_REQ: {     /*join request message*/
			revercpy(appEui, payload + 1, 8);
			revercpy(devEui, payload + 9, 8);
			i8_to_hexstr(appEui, appEui_hex, 8);
			i8_to_hexstr(devEui, devEui_hex, 8);
			devNonce |= (uint16_t)payload[17];
			devNonce |= ((uint16_t)payload[18])<<8;
			mic |= (uint32_t)payload[19];
			mic |= ((uint32_t)payload[20])<<8;
			mic |= ((uint32_t)payload[21])<<16;
			mic |= ((uint32_t)payload[22])<<24;

			/*judge whether it is a repeated message: select joinnonce from devs where deveui = ?*/
                        /* select joinnonce from devs where deveui = ? */
			if (db_lookup_int(cntx->selectjoinnonce, devNonce, *outint)) {
				MSG("WARNING: [up] have same nonce, Drop\n");
				/*The device has not registered in the network server */
				result->to = IGNORE;
				break;
			} 

                        /* select appid from devs where deveui = ? */
			db_lookup_int(cntx->selectappidbydeveui, devEui_hex, *outint); 
                        if (outint == 0) {
				MSG("WARNING: [up] The device has not registered a app\n");
				/*The device has not registered in the network server */
				result->to = IGNORE;
				break;
			}
			LoRaMacJoinComputeMic(payload, 23 - 4, appKey, &cal_mic);
			/*if mic is wrong,the join request will be ignored*/
			if (mic != cal_mic) {
				MSG("WARNING: [up] join request payload mic is wrong,just ignore it\n");
				result->to = IGNORE;
			} else {
				result->to = APPLICATION_SERVER;
				root_value = json_value_init_object();
				root_object = json_value_get_object(root_value);
				json_object_dotset_string(root_object, "join.gwaddr", meta->gwaddr);
				json_object_dotset_string(root_object, "join.appeui", appEui_hex);
				json_object_dotset_string(root_object, "join.deveui", devEui_hex);
				json_object_dotset_number(root_object, "join.request.devnonce", devNonce);
				/*assign the device address by rand(),just for experiment*/
				srand(time(NULL));
				devAddr = rand();
				/*update the devaddr filed in the database*/
				if (update_db_by_deveui_uint(db, "devaddr", "nsdevinfo", devEui_hex, (unsigned int)devAddr) == FAILED) {
					MSG("WARNING: [up] update the database failed\n");
					result->to = IGNORE;
					json_value_free(root_value);
					break;
				}
				/* update the gwaddr filed in the table transarg
				 * reset other fileds to default value
                                 * datarate and frequency default 8695250
				 */
				if (update_db_by_deveui(db, "gwaddr", "transarg", devEui_hex, meta->gwaddr, 1) == FAILED ||
				   update_db_by_deveui_uint(db, "rx1datarate", "transarg", devEui_hex, 0) == FAILED ||
				   update_db_by_deveui_uint(db, "rx2datarate", "transarg", devEui_hex, 0) == FAILED ||
				   update_db_by_deveui_uint(db, "rx2frequency", "transarg", devEui_hex, 8695250) == FAILED ||
				   update_db_by_deveui_uint(db, "delay", "transarg", devEui_hex, 1) == FAILED ||
				   update_db_by_deveui_uint(db, "upcnt", "lastappinfo", devEui_hex, 0) == FAILED ||
				   update_db_by_deveui_uint(db, "downcnt", "nsdevinfo", devEui_hex, 1) == FAILED) {
					MSG("WARNING: [up] update the database failed\n");
					result->to = IGNORE;
					json_value_free(root_value);
					break;
				}
				result->join = true;
				strcpy(result->deveui_hex, devEui_hex);
				json_object_dotset_number(root_object, "join.request.devaddr", devAddr);
				str = json_serialize_to_string_pretty(root_value);
				strcpy(result->json_string_as, str);
				json_free_serialized_string(str);
				json_value_free(root_value);
			}
			break;
		}
		/*unconfirmed message*/
		case FRAME_TYPE_DATA_UNCONFIRMED_UP:/*fall through,just handle like confirmed message*/
		/*confirmed message*/
		case FRAME_TYPE_DATA_CONFIRMED_UP: {
			result->join = false;
			devAddr |= (uint32_t)payload[1];
			devAddr |= ((uint32_t)payload[2])<<8;
			devAddr |= ((uint32_t)payload[3])<<16;
			devAddr |= ((uint32_t)payload[4])<<24;
			fCtrl.Value = payload[5];
			fopts_len = fCtrl.Bits.FOptsLen;
			adr = fCtrl.Bits.Adr;
			upCnt |= (uint16_t)payload[6];
			upCnt |= ((uint16_t)payload[7])<<8;
			fport = payload[8 + fopts_len];
			memcpy(fpayload, payload + 9 + fopts_len, size - 13 - fopts_len);
			mic |= (uint32_t)payload[size - 4];
			mic |= ((uint32_t)payload[size - 3])<<8;
			mic |= ((uint32_t)payload[size - 2])<<16;
			mic |= ((uint32_t)payload[size - 1])<<24;

			/*query the deveui according to the devaddr*/
			if (query_db_by_addr_str(db, "deveui", "nsdevinfo", devAddr, devEui_hex) == FAILED){
				MSG("WARNING: [up] query the database failed\n");
				/*The device has not joined in the LoRaWAN */
				result->to = IGNORE;
				break;
			}
			/*judge whether it is a repeated message*/
			if (query_db_by_deveui_uint(db, "tmst", "lastappinfo", devEui_hex, &pre_timestamp) == FAILED ||
			   query_db_by_deveui_uint(db, "upcnt", "lastappinfo", devEui_hex, &pre_upCnt) == FAILED ||
			   query_db_by_deveui_uint(db, "rfch", "lastappinfo", devEui_hex, &pre_rfchannel) == FAILED) {
				MSG("WARNING: [up] query the database failed\n");
				result->to = IGNORE;
				break;
			} else {
				if (pre_timestamp < RANGE)
					pre_tmst_min = 0;
				else
					pre_tmst_min = pre_timestamp - RANGE;
				pre_tmst_max = pre_timestamp + RANGE;
				if (upCnt == (uint16_t)pre_upCnt && meta->rfch == (uint8_t)pre_rfchannel) {
					/*it is a repeated message,just ignore*/
					MSG("INFO: [up] reduplicated unconfirmed message\n");
					result->to = IGNORE;
					break;
				} else {
					if(update_db_by_deveui_uint(db, "tmst", "lastappinfo", devEui_hex, meta->tmst) == FAILED ||
					   update_db_by_deveui_uint(db, "upcnt", "lastappinfo", devEui_hex, upCnt) == FAILED ||
					   update_db_by_deveui_uint(db, "rfch", "lastappinfo", devEui_hex, meta->rfch) == FAILED){
						MSG("WARNING: [up] update the database failed\n");
						result->to = IGNORE;
						break;
					}
				}
			}
			if (query_db_by_addr(db, "nwkskey", "nsdevinfo", devAddr, nwkSKey, 16) == FAILED){
				MSG("WARNING: [up] query the database failed\n");
				/*The device has not joined in the LoRaWAN */
				result->to = IGNORE;
				break;
			}
			LoRaMacComputeMic(payload, meta->size - 4, nwkSKey, devAddr, UP, (uint32_t)upCnt, &cal_mic);
			if(cal_mic != mic){
				MSG("WARNING: [up] push data payload mic is wrong,just ignore it\n");
				result->to = IGNORE;
				break;
			}
			size_b64 = bin_to_b64(fpayload, size - 13 - fopts_len, fpayload_b64, MAX_NB_B64);
			/*when the message contains both MAC command and userdata*/
			MSG("len:%d\n", fopts_len);
			if (fopts_len > 0) {
				memcpy(fopts, payload + 8, fopts_len);
				if (fport == 0) {
					MSG("WARNING: [up] push data payload fport is wrong,just ignore it\n");
					result->to = IGNORE;
					break;
				}
				result->to = BOTH;
				size_b64 = bin_to_b64(fopts, fopts_len, fopts_b64, 21);
				maccmd = malloc(size_b64 + 1);
				memcpy(maccmd, fopts_b64, size_b64 + 1);
				encmd = 0;
			} else { /*when the message contains only MAC command */
				if (fport == 0) {
					result->to = NETWORK_CONTROLLER;
					encmd = 1;
					maccmd = malloc(size_b64 + 1);
					memcpy(maccmd, fpayload_b64, size_b64 + 1);
				} else {/*when the message contains only user data*/
					result->to = APPLICATION_SERVER;
				}
			}
			/*update the gwaddr filed in the table transarg*/
			if (update_db_by_deveui(db, "gwaddr", "transarg", devEui_hex, meta->gwaddr, 1) == FAILED) {
				MSG("WARNING: [up] update the database failed\n");
				result->to = IGNORE;
				break;
			}
			/*packet the json string which will send to the application server*/
			if (result->to == APPLICATION_SERVER || result->to == BOTH) {
				result->devaddr = devAddr;
				root_value = json_value_init_object();
				root_object = json_value_get_object(root_value);
				json_object_dotset_string(root_object, "app.gwaddr", meta->gwaddr);
				json_object_dotset_string(root_object, "app.deveui", devEui_hex);
				json_object_dotset_number(root_object, "app.devaddr", devAddr);
				json_object_dotset_string(root_object, "app.dir", "up");
				json_object_dotset_number(root_object, "app.userdata.seqno", upCnt);
				json_object_dotset_number(root_object, "app.userdata.port", fport);
				json_object_dotset_string(root_object, "app.userdata.payload", fpayload_b64);
				json_object_dotset_number(root_object, "app.userdata.devx.freq", meta->freq);
				json_object_dotset_string(root_object, "app.userdata.devx.modu", meta->modu);
				if (strcmp(meta->modu, "LORA") == 0) {
					json_object_dotset_string(root_object, "app.userdata.devx.datr", meta->datrl);
				} else {
					json_object_dotset_number(root_object, "app.userdata.devx.datr", meta->datrf);
				}
				json_object_dotset_string(root_object, "app.userdata.devx.codr", meta->codr);
				json_object_dotset_boolean(root_object, "app.userdata.devx.adr", adr);
				json_object_dotset_string(root_object, "app.userdata.gwrx.time", meta->time);
				json_object_dotset_number(root_object, "app.userdata.gwrx.chan", meta->chan);
				json_object_dotset_number(root_object, "app.userdata.gwrx.rfch", meta->rfch);
				json_object_dotset_number(root_object, "app.userdata.gwrx.rssi", meta->rssi);
				json_object_dotset_number(root_object, "app.userdata.gwrx.lsnr", meta->lsnr);
				str = json_serialize_to_string_pretty(root_value);
				strcpy(result->json_string_as, str);
				json_free_serialized_string(str);
				json_value_free(root_value);
			}

			/*packet the json string that will send to the network controller */
			if (result->to == NETWORK_CONTROLLER || result->to == BOTH) {
				result->devaddr = devAddr;
				root_value = json_value_init_object();
				root_object = json_value_get_object(root_value);
				json_object_dotset_string(root_object, "app.gwaddr", meta->gwaddr);
				json_object_dotset_string(root_object, "app.deveui", devEui_hex);
				json_object_dotset_number(root_object, "app.devaddr", devAddr);
				json_object_dotset_string(root_object, "app.dir", "up");
				json_object_dotset_number(root_object, "app.seqno", upCnt);
				json_object_dotset_number(root_object, "app.devx.freq", meta->freq);
				json_object_dotset_string(root_object, "app.devx.modu", meta->modu);
				if (strcmp(meta->modu, "LORA") == 0) {
					json_object_dotset_string(root_object, "app.devx.datr", meta->datrl);
				} else
					json_object_dotset_number(root_object, "app.devx.datr", meta->datrf);
				json_object_dotset_string(root_object, "app.devx.codr", meta->codr);
				json_object_dotset_boolean(root_object, "app.devx.adr", adr);
				json_object_dotset_string(root_object, "app.gwrx.time", meta->time);
				json_object_dotset_number(root_object, "app.gwrx.chan", meta->chan);
				json_object_dotset_number(root_object, "app.gwrx.rfch", meta->rfch);
				json_object_dotset_number(root_object, "app.gwrx.rssi", meta->rssi);
				json_object_dotset_number(root_object, "app.gwrx.lsnr", meta->lsnr);
				json_object_dotset_string(root_object, "app.maccmd.command", maccmd);
				json_object_dotset_boolean(root_object, "app.maccmd.isencrypt", encmd);
				str = json_serialize_to_string_pretty(root_value);
				strcpy(result->json_string_nc, str);
				json_free_serialized_string(str);
				json_value_free(root_value);
				free(maccmd);
			}
			break;
		}
		/*proprietary message*/
		case FRAME_TYPE_PROPRIETARY: {
			memcpy(fpayload, payload + 1, size-1);
			break;
		}
	}

	if (result->to == APPLICATION_SERVER || result->to == BOTH) {
		MSG("###########Message that will be transfered to the application server#############\n");
		MSG("%s\n", result->json_string_as);
	}

	if (result->to == NETWORK_CONTROLLER || result->to == BOTH) {
		MSG("###########Message that will be transfered to the network controller#############\n");
		MSG("%s\n", result->json_string_nc);
	}

        sqlite3_close(db);
}

bool serialize_msg_to_gw(const char* data, int size, const char* deveui_hex, char* json_data, char* gwaddr, uint32_t tmst, int delay) {
	JSON_Value  *root_val_x = NULL;
	JSON_Object *root_obj_x = NULL;
	unsigned int rx1_dr;
	unsigned int rx2_dr;
	unsigned int rx2_freq;
	char dr[16];
	double freq;
	struct timespec time;/*storing local timestamp*/
	char* json_str = NULL;

	if (query_db_by_deveui_str(db, "gwaddr", "transarg", deveui_hex, gwaddr) == FAILED) {
		MSG("WARNING: [down] query the database failed\n");
                sqlite3_close(db);
		return false;
	}
	if(query_db_by_deveui_uint(db, "rx1datarate", "transarg", deveui_hex, &rx1_dr) == FAILED ||
		query_db_by_deveui_uint(db, "rx2datarate", "transarg", deveui_hex, &rx2_dr) == FAILED ||
		query_db_by_deveui_uint(db, "rx2freq", "transarg", deveui_hex, &rx2_freq) == FAILED){
		MSG("WARNING: [down] query the database failed\n");
                sqlite3_close(db);
		return false;
	}

	switch (rx2_dr) {
		case 0:{
			strcpy(dr, "SF12BW125");
			break;
		}
		case 1:{
			strcpy(dr, "SF11BW125");
			break;
		}
		case 2:{
			strcpy(dr, "SF10BW125");
			break;
		}
		case 3:{
			strcpy(dr, "SF9BW125");
			break;
		}
		case 4:{
			strcpy(dr, "SF8BW125");
			break;
		}
		case 5:{
			strcpy(dr, "SF7BW125");
			break;
		}
		case 6:{
			strcpy(dr, "SF7BW250");
			break;
		}
		case 7:{
			strcpy(dr, "FSK");
			break;
		}
		default:{
			strcpy(dr, "SF7BW125");
		}
	}
	freq = (double)rx2_freq / (double)10000;
	MSG("%s %.4f\n", dr, freq);
	clock_gettime(CLOCK_REALTIME, &time);
	root_val_x = json_value_init_object();
	root_obj_x = json_value_get_object(root_val_x);
	if (delay == NO_DELAY) {
		json_object_dotset_boolean(root_obj_x, "txpk.imme", true);
	} else {
		json_object_dotset_number(root_obj_x, "txpk.tmst", tmst+delay);
	}
	json_object_dotset_number(root_obj_x, "txpk.freq", freq);
	json_object_dotset_number(root_obj_x, "txpk.rfch", 0);
	json_object_dotset_number(root_obj_x, "txpk.powe", 14);
	json_object_dotset_string(root_obj_x, "txpk.modu", "LORA");
	json_object_dotset_string(root_obj_x, "txpk.datr", dr);
	json_object_dotset_string(root_obj_x, "txpk.codr", "4/5");
	json_object_dotset_boolean(root_obj_x, "txpk.ipol", 1);
	json_object_dotset_number(root_obj_x, "txpk.size", size);
	json_object_dotset_string(root_obj_x, "txpk.data", data);
	json_str = json_serialize_to_string_pretty(root_val_x);
	strncpy(json_data, json_str, strlen(json_str) + 1);
	json_free_serialized_string(json_str);
	json_value_free(root_val_x);
        sqlite3_close(db);
	return true;
}

/* ---------------------------------------------------------------------- */
/* --- handle the message receive from ns --------------------------------*/
struct res_handle as_msg_handle(char* msg, int index) {
	struct res_handle res;
	/*session keys and appkey*/
	uint8_t appSKey[16];
	uint8_t nwkSKey[16];
	uint8_t appKey[16];

	/*JSON parsing variables*/
	JSON_Value *root_val = NULL;
	JSON_Object *app_obj = NULL;
	JSON_Object *udata_obj = NULL;
	JSON_Object *devx_obj = NULL;
	JSON_Object *gwrx_obj = NULL;
	JSON_Object *join_obj = NULL;
	JSON_Object *request_obj = NULL;
	JSON_Object *accept_obj = NULL;
	JSON_Value *val = NULL;

        JSON_Value *root_val_x = NULL;
        JSON_Object * root_obj_x = NULL;

	char content[1024];/*1024 is big enough*/
	char msgname[64];
	char tempstr[32];
	const char* str;
	char* json_str;
	uint32_t devAddr;
	uint32_t seqno;
	uint8_t userdata_payload[LORAMAC_FRAME_MAXPAYLOAD];/*the frame payload defined in LORAWAN specification */
	uint8_t dec_userdata[LORAMAC_FRAME_MAXPAYLOAD];/*decrypted frame payload*/
	int psize;/*length of frame payload size*/
	int j;

	uint64_t deveui;
	uint64_t appeui;
	char  deveui_hex[17];
	char  appeui_hex[17];
	uint16_t devNonce;
	uint8_t frame_payload[JOIN_ACC_SIZE];
	uint8_t frame_payload_b64[MAX_NB_B64];
	char nwkskey_hex[33];/*store the nwkskey*/
	char appskey_hex[33];/*store the appskey*/
	char gwaddr[16];

	/*initialize some variable in case it uses uncertain value*/
	bzero(&res, sizeof(res));
	bzero(content, sizeof(content));
	bzero(dec_userdata, sizeof(dec_userdata));
	bzero(deveui_hex, sizeof(deveui_hex));
	bzero(appeui_hex, sizeof(appeui_hex));
	bzero(nwkskey_hex, sizeof(nwkskey_hex));
	bzero(appskey_hex, sizeof(appskey_hex));

        if (sqlite3_open("/tmp/loraserv", &db)) {
                MSG("ERROR: Can't open database: %s\n", sqlite3_errmsg(db));
                sqlite3_close(db);
		res.signal = 0;
                return res;
        }

	root_val = json_parse_string_with_comments(msg);

	if (root_val == NULL) {
		MSG("WARNING: [up] message received contains invalid JSON\n");
		json_value_free(root_val);
	}
	app_obj = json_object_get_object(json_value_get_object(root_val), "app");
	if (app_obj == NULL) {
		join_obj = json_object_get_object(json_value_get_object(root_val), "join");
		if (join_obj == NULL) {
			MSG("WARNING: [up] message received contains neither \"app\" nor \"join\"\n");
			res.signal = 0;
			json_value_free(root_val);
		} else {
			res.signal = 1;
			/*handling request json data*/
			snprintf(msgname, sizeof(msgname), "###############join_request_%d:###############\n", index);
			strcat(content, msgname);
			val = json_object_get_value(join_obj, "gwaddr");
			if (val != NULL) {
				strcpy(gwaddr, json_value_get_string(val));
				snprintf(tempstr, sizeof(tempstr), "gwaddr:%s", json_value_get_string(val));
				strcat(content, tempstr);
			}
			val = json_object_get_value(join_obj, "deveui");
			if (val != NULL) {
				strcpy(deveui_hex, json_value_get_string(val));
				snprintf(tempstr, sizeof(tempstr), " deveui:%s", json_value_get_string(val));
				strcat(content, tempstr);
			}
			val = json_object_get_value(join_obj, "appeui");
			if (val != NULL) {
				strcpy(appeui_hex, json_value_get_string(val));
				snprintf(tempstr, sizeof(tempstr), " appeui:%s", json_value_get_string(val));
				strcat(content, tempstr);
			}
			request_obj = json_object_get_object(join_obj, "request");
			if (request_obj == NULL) {
				MSG("WARNING:[up] join-request message received contains no \"request\" in \"join\"\n");
			} else {
				val = json_object_get_value(request_obj, "devnonce");
				devNonce = (uint16_t)json_value_get_number(val);
				if (val != NULL) {
					snprintf(tempstr, sizeof(tempstr), " devnonce:%.0f", json_value_get_number(val));
					strcat(content, tempstr);
				}
				val = json_object_get_value(request_obj, "devaddr");
				devAddr = (uint32_t)json_value_get_number(val);
				if (val != NULL) {
					snprintf(tempstr, sizeof(tempstr), " devaddr:%.0f", json_value_get_number(val));
					strcat(content, tempstr);
				}
			}

			/*query the appkey from the database*/
			if (query_db_by_deveui(db, "appkey", "appsdevinfo", deveui_hex, appKey, 16) == FAILED){
					/*the device isn't registered in the application server*/
					MSG("WARNING: [up] query the database failed\n");
					res.signal = 0;
			} else {
				as_prepare_frame(frame_payload, devNonce, appKey, devAddr, nwkSKey, appSKey);
				memcpy(res.appSKey, appSKey, sizeof(appSKey));
				/*binary to b64*/
				bin_to_b64(frame_payload, JOIN_ACC_SIZE, frame_payload_b64, MAX_NB_B64);
				/*get the nwkSKey and appSKey change it to char* */
				for (j=0; j<16; j++) {
					snprintf(tempstr, sizeof(tempstr), "%02x", nwkSKey[j]);
					strcat(nwkskey_hex, tempstr);
				}
				for (j=0; j<16; j++) {
					snprintf(tempstr, sizeof(tempstr), "%02x", appSKey[j]);
					strcat(appskey_hex, tempstr);
				}
				/*store the appSKey into the database*/
				if(update_db_by_deveui(db, "appskey", "appsdevinfo", deveui_hex, appskey_hex, 0) == FAILED){
					/*the device isn't registered in the application server*/
					MSG("WARNING: [up] update the database failed\n");
					res.signal = 0;
				} else {
					/*parsing data to json string*/
					root_val_x = json_value_init_object();
					root_obj_x = json_value_get_object(root_val_x);
					json_object_dotset_string(root_obj_x, "join.gwaddr", gwaddr);
					json_object_dotset_string(root_obj_x, "join.appeui", appeui_hex);
					json_object_dotset_string(root_obj_x, "join.deveui", deveui_hex);
					json_object_dotset_string(root_obj_x, "join.accept.frame", frame_payload_b64);
					json_object_dotset_string(root_obj_x,"join.accept.nwkskey",nwkskey_hex);
					json_str = json_serialize_to_string_pretty(root_val_x);
					strcpy(res.json_string, json_str);
					strcpy(res.appSKey, appskey_hex);
					json_free_serialized_string(json_str);
					json_value_free(root_val_x);
				}
			}
			json_value_free(root_val);
		}
	} else { /*handling app json data*/
		res.signal = 2;
		snprintf(msgname, sizeof(msgname),"###############common_message_%d:###############\n", index);
		strcat(content, msgname);
		val = json_object_get_value(app_obj, "gwaddr");
		if (val != NULL) {
			strcpy(gwaddr, json_value_get_string(val));
			snprintf(tempstr, sizeof(tempstr), "gwaddr:%s", json_value_get_string(val));
			strcat(content, tempstr);
		}
		val = json_object_get_value(app_obj, "deveui");
		if (val != NULL) {
			strcpy(deveui_hex, json_value_get_string(val));
			snprintf(tempstr, sizeof(tempstr), " deveui:%s", json_value_get_string(val));
			strcat(content, tempstr);
		}
		val = json_object_get_value(app_obj, "devaddr");
		if (val != NULL) {
			devAddr = (uint32_t)json_value_get_number(val);
			snprintf(tempstr, sizeof(tempstr), " devaddress:%.0f", json_value_get_number(val));
			strcat(content, tempstr);
		}
		val = json_object_get_value(app_obj, "dir");
		if (val != NULL) {
			snprintf(tempstr, sizeof(tempstr), " direction:%s", json_value_get_string(val));
			strcat(content, tempstr);
		}
		udata_obj = json_object_get_object(app_obj, "userdata");
		if (udata_obj == NULL) {
			MSG("WARNING: [up] message received contains no \"userdata\" in \"app\"\n");
		} else {
			val = json_object_get_value(udata_obj, "seqno");
			if (val != NULL) {
				seqno = (uint32_t)json_value_get_number(val);
				snprintf(tempstr, sizeof(tempstr), " sequenceNo:%.0f", json_value_get_number(val));
				strcat(content, tempstr);
			}
			val = json_object_get_value(udata_obj, "port");
			if (val != NULL) {
				snprintf(tempstr, sizeof(tempstr), " port:%.0f", json_value_get_number(val));
				strcat(content, tempstr);
			}
			val =json_object_get_value(udata_obj, "payload");
			if (val != NULL) {
				str = json_value_get_string(val);
				/*convert to binary*/
				psize = b64_to_bin(str, strlen(str), userdata_payload, LORAMAC_FRAME_MAXPAYLOAD);
				/*query the appkey from the database*/
				if (query_db_by_deveui(db, "appskey", "appsdevinfo", deveui_hex, appSKey, 16) == FAILED) {
					/*the device isn't registered in the application server*/
					MSG("WARNING: [up] query the database failed\n");
				} else {
					/*compute the LoRaMAC frame payload decryption*/
					LoRaMacPayloadDecrypt(userdata_payload, psize, appSKey, devAddr, UP, seqno, dec_userdata);
					strcat(content, " payload:");
					for (j = 0; j < psize; j++) {
						snprintf(tempstr, sizeof(tempstr), "0x%02x ", dec_userdata[j]);
						strcat(content, tempstr);
					}
				}
			}
			devx_obj = json_object_get_object(udata_obj, "devx");
			if (devx_obj == NULL) {
				MSG("WARNING: [up] message received contains no \"devx\" in \"userdata\"\n");
			} else {
				val = json_object_get_value(devx_obj, "freq");
				if (val != NULL) {
					snprintf(tempstr, sizeof(tempstr), " frequence:%.6f", json_value_get_number(val));
					strcat(content, tempstr);
				}
				val = json_object_get_value(devx_obj, "modu");
				if (val != NULL) {
					snprintf(tempstr, sizeof(tempstr), " modulation:%s", json_value_get_string(val));
					strcat(content, tempstr);
					if (strcmp("LORA", json_value_get_string(val)) == 0) {
						val = json_object_get_value(devx_obj, "datr");
						if (val != NULL) {
							snprintf(tempstr, sizeof(tempstr), " datr:%s", json_value_get_string(val));
							strcat(content, tempstr);
						}
					} else {
						val = json_object_get_value(devx_obj, "datr");
						if (val != NULL) {
							snprintf(tempstr, sizeof(tempstr), " datarate:%.2f", json_value_get_number(val));
							strcat(content, tempstr);
						}
					}
				}
				val = json_object_get_value(devx_obj, "codr");
				if (val != NULL) {
					snprintf(tempstr, sizeof(tempstr), " coderate:%s", json_value_get_string(val));
					strcat(content, tempstr);
				}
				val = json_object_get_value(devx_obj, "adr");
				if (val != NULL) {
					snprintf(tempstr, sizeof(tempstr), " adr:%d", json_value_get_boolean(val));
					strcat(content, tempstr);
				}
			}
			gwrx_obj = json_object_get_object(udata_obj, "gwrx");
			if (gwrx_obj == NULL) {
				MSG("WARNING: [up] message received contains no \"gwrx\" in \"userdata\"\n");
			} else {
				val = json_object_get_value(gwrx_obj, "time");
				if (val != NULL) {
					snprintf(tempstr, sizeof(tempstr), " time:%s", json_value_get_string(val));
					strcat(content, tempstr);
				}
				val = json_object_get_value(gwrx_obj, "chan");
				if (val != NULL) {
					snprintf(tempstr, sizeof(tempstr), " channel:%.0f", json_value_get_number(val));
					strcat(content, tempstr);
				}
				val = json_object_get_value(gwrx_obj, "rfch");
				if (val != NULL) {
					snprintf(tempstr, sizeof(tempstr), " rfchannel:%.0f", json_value_get_number(val));
					strcat(content, tempstr);
				}
				val = json_object_get_value(gwrx_obj, "rssi");
				if (val != NULL) {
					snprintf(tempstr, sizeof(tempstr), " rssi:%.0f", json_value_get_number(val));
					strcat(content, tempstr);
				}
				val = json_object_get_value(gwrx_obj, "lsnr");
				if (val != NULL) {
					snprintf(tempstr, sizeof(tempstr), " lsnr:%.2f", json_value_get_number(val));
					strcat(content, tempstr);
				}
			}
		}
		json_value_free(root_val);
	}

	MSG("%s\n", content);

	if (res.signal == 1) {
		MSG("###########Message that will be transfered to the network server#############\n");
		MSG("%s\n", res.json_string);
	}

        sqlite3_close(db);

	return res;
}

void as_prepare_frame(uint8_t *frame_payload, uint16_t devNonce, uint8_t* appKey, uint32_t devAddr, uint8_t *nwkSKey, uint8_t *appSKey) {
	uint8_t  payload[JOIN_ACC_SIZE];
	uint8_t index = 0;
	LoRaMacHeader_t hdr;
	int rand_num;
	uint8_t appNonce[3];
	uint8_t NetID[3] = {0x00, 0x00, 0x00};
	LoRaMacDLSettings_t dls;
	LoRaMacRxDelay_t rxd;
	uint32_t mic;

	/*the header of mac LoRaMac packet*/
	hdr.Value=0;
	hdr.Bits.MType = FRAME_TYPE_JOIN_ACCEPT;
	payload[index] = hdr.Value;
	/*generate the appNonce,here just for experienment using random number*/
	srand(time(NULL));
	rand_num = (uint32_t)rand();
	/*AppNonce*/
	payload[++index] = appNonce[2] = rand_num & 0xFF;
	payload[++index] = appNonce[1] = (rand_num>>8) & 0xFF;
	payload[++index] = appNonce[0] = (rand_num>>16) & 0xFF;
	/*NetID*/
	payload[++index] = NetID[2];
	payload[++index] = NetID[1];
	payload[++index] = NetID[0];
	/*DevAddr*/
	payload[++index] = devAddr & 0xFF;
	payload[++index] = (devAddr>>8) & 0xFF;
	payload[++index] = (devAddr>>16) & 0xFF;
	payload[++index] = (devAddr>>24) & 0xFF;
	/*DLSettings*/
	dls.Value = 0;
	dls.Bits.RX1DRoffset = 0;
	dls.Bits.RX2DataRate = 0;
	payload[++index] = dls.Value;
	/*RxDelay*/
	rxd.Value = 0;
	rxd.Bits.Del = 1;
	payload[++index] = rxd.Value;

	LoRaMacJoinComputeMic(payload, (uint16_t)17 - 4, appKey, &mic);
	payload[++index] = mic & 0xFF;
	payload[++index] = (mic>>8) & 0xFF;
	payload[++index] = (mic>>16) & 0xFF;
	payload[++index] = (mic>>24) & 0xFF;
	/*compute the two session key
	 *the second argument is corresponding to the LoRaMac.c(v4.0.0)
	 *it seems that it makes a mistake,because the byte-order is adverse
	*/
	LoRaMacJoinComputeSKeys(appKey, payload + 1, devNonce, nwkSKey, appSKey);
	/*encrypt join accept message*/
	LoRaMacJoinEncrypt(payload + 1, (uint16_t)JOIN_ACC_SIZE - 1, appKey, frame_payload + 1);
	frame_payload[0] = payload[0];
}

/* -------------------------------------------------------------------*/
/* ---- LoraMAC commands handle ---------------------------------------*/
int command_handle(int cid,uint32_t devAddr,char* json_data,...){

	va_list arg_ptr;

	uint8_t *payload;/*mac commmand*/
	uint8_t frame_raw[CMD_FRAME_DOWN_MAX];/*enough to storing downstream data*/
	char* frame;
	uint8_t nwkSKey[16];
	int frame_size;
	uint32_t downlink_counter;

	/*variables for generating mac command*/
	uint8_t margin;
	uint8_t gw_cnt;
	uint8_t datarate;
	uint8_t tx_power;
	uint16_t ch_mask;
	uint8_t  ch_mask_cntl;
	uint8_t  nb_rep;
	uint8_t  max_dcycle;
	uint8_t  rx1_droffset;
	uint8_t  rx2_datarate;
	uint32_t frequency;
	uint8_t ch_index;
	uint32_t freq;
	uint8_t  max_dr;
	uint8_t  min_dr;
	uint8_t  del;

        sqlite3* db;

	/*JSON parsing variables*/
	JSON_Value* root_val=NULL;
	JSON_Object* root_obj=NULL;
	char* json_str;

	int residue;
	int max_len;

	bzero(frame_raw,sizeof(frame_raw));
	bzero(nwkSKey,sizeof(nwkSKey));


        if (sqlite3_open("/tmp/loraserv", &db)) {
                MSG("ERROR: Can't open database: %s\n", sqlite3_errmsg(db));
                sqlite3_close(db);
                return -1;
        }

	if(query_db_by_addr(db, "nwkskey","nsdevinfo",devAddr,nwkSKey,16)==FAILED){
		MSG("WARNING: [down] query the database failed\n");
                sqlite3_close(db);
		return -1;
	}

	if(query_db_by_addr_uint(db, "downcnt", "nsdevinfo", devAddr, &downlink_counter) == FAILED){
		MSG("WARNING: [down] query the database failed\n");
                sqlite3_close(db);
		return -1;
	}
	va_start(arg_ptr,json_data);
	switch(cid){
		case SRV_MAC_LINK_CHECK_ANS:{
			margin=(uint8_t)va_arg(arg_ptr,int);
			gw_cnt=(uint8_t)va_arg(arg_ptr,int);
			payload=malloc(sizeof(uint8_t)*3);
			payload[0]=SRV_MAC_LINK_CHECK_ANS;
			payload[1]=margin;
			payload[2]=gw_cnt;
			nc_prepare_frame(FRAME_TYPE_DATA_UNCONFIRMED_DOWN,devAddr,downlink_counter,payload,nwkSKey,3,frame_raw,&frame_size);
			free(payload);
			break;
		}
		case SRV_MAC_LINK_ADR_REQ:{
			/*
			 * TODO:
			 * query the adr field in the database
			 */
			datarate=(uint8_t)va_arg(arg_ptr,int);
			tx_power=(uint8_t)va_arg(arg_ptr,int);
			ch_mask=(uint16_t)va_arg(arg_ptr,int);
			ch_mask_cntl=(uint8_t)va_arg(arg_ptr,int);
			nb_rep=(uint8_t)va_arg(arg_ptr,int);
			MacCmdDataRateTxPower_t datarate_txpower;
			MacCmdRedundancy_t redundancy;
			datarate_txpower.Bits.DataRate=datarate;
			datarate_txpower.Bits.TxPower=tx_power;
			redundancy.Value=0;
			redundancy.Bits.chMaskCntl=ch_mask_cntl;
			redundancy.Bits.NbRep=nb_rep;
			payload=malloc(sizeof(uint8_t)*5);
			payload[0]=SRV_MAC_LINK_ADR_REQ;
			payload[1]=datarate_txpower.Value;
			payload[2]=ch_mask&0xFF;
			payload[3]=(ch_mask>>8)&0xFF;
			payload[4]=redundancy.Value;
			nc_prepare_frame(FRAME_TYPE_DATA_CONFIRMED_DOWN,devAddr,downlink_counter,payload,nwkSKey,5,frame_raw,&frame_size);
			free(payload);
			break;
		}
		case SRV_MAC_DUTY_CYCLE_REQ:{
			max_dcycle=(uint8_t)va_arg(arg_ptr,int);
			payload=malloc(sizeof(uint8_t)*2);
			payload[0]=SRV_MAC_DUTY_CYCLE_REQ;
			payload[1]=max_dcycle;
			nc_prepare_frame(FRAME_TYPE_DATA_CONFIRMED_DOWN,devAddr,downlink_counter,payload,nwkSKey,2,frame_raw,&frame_size);
			free(payload);
			break;
		}
		case SRV_MAC_RX_PARAM_SETUP_REQ:{
			rx1_droffset=(uint8_t)va_arg(arg_ptr,int);
			rx2_datarate=(uint8_t)va_arg(arg_ptr,int);
			frequency=(uint32_t)va_arg(arg_ptr,int);
			MacCmdDLSettings_t dlsettings;
			dlsettings.Value=0;
			dlsettings.Bits.RX1DRoffset=rx1_droffset;
			dlsettings.Bits.RX2DataRate=rx2_datarate;
			payload=malloc(sizeof(uint8_t)*5);
			payload[0]=SRV_MAC_RX_PARAM_SETUP_REQ;
			payload[1]=dlsettings.Value;
			payload[2]=frequency&0xFF;
			payload[3]=(frequency>>8)&0xFF;
			payload[4]=(frequency>>16)&0xFF;
			nc_prepare_frame(FRAME_TYPE_DATA_CONFIRMED_DOWN,devAddr,downlink_counter,payload,nwkSKey,5,frame_raw,&frame_size);
			free(payload);
			break;
		}
		case SRV_MAC_DEV_STATUS_REQ:{
			payload=malloc(sizeof(uint8_t)*1);
			payload[0]=SRV_MAC_DEV_STATUS_REQ;
			nc_prepare_frame(FRAME_TYPE_DATA_CONFIRMED_DOWN,devAddr,downlink_counter,payload,nwkSKey,1,frame_raw,&frame_size);
			free(payload);
			break;
		}
		case SRV_MAC_NEW_CHANNEL_REQ:{
			ch_index=(uint8_t)va_arg(arg_ptr,int);
			freq=(uint32_t)va_arg(arg_ptr,int);
			max_dr=(uint8_t)va_arg(arg_ptr,int);
			min_dr=(uint8_t)va_arg(arg_ptr,int);
			MacCmdDrRange_t dr_range;
			dr_range.Bits.MaxDR=max_dr;
			dr_range.Bits.MinDR=min_dr;
			payload=malloc(sizeof(uint8_t)*6);
			payload[0]=SRV_MAC_NEW_CHANNEL_REQ;
			payload[1]=ch_index;
			payload[2]=freq&0xFF;
			payload[3]=(freq>>8)&0xFF;
			payload[4]=(freq>>16)&0xFF;
			payload[5]=dr_range.Value;
			nc_prepare_frame(FRAME_TYPE_DATA_CONFIRMED_DOWN,devAddr,downlink_counter,payload,nwkSKey,6,frame_raw,&frame_size);
			free(payload);
			break;
		}
		case SRV_MAC_RX_TIMING_SETUP_REQ:{
			del=(uint8_t)va_arg(arg_ptr,int);
			MacCmdSettings_t settings;
			settings.Value=0;
			settings.Bits.Del=del;
			payload=malloc(sizeof(uint8_t)*2);
			payload[0]=SRV_MAC_RX_TIMING_SETUP_REQ;
			payload[1]=settings.Value;
			nc_prepare_frame(FRAME_TYPE_DATA_CONFIRMED_DOWN,devAddr,downlink_counter,payload,nwkSKey,2,frame_raw,&frame_size);
			free(payload);
			break;
		}
		default:{
			va_end(arg_ptr);
			return -1;
		}
	}
	va_end(arg_ptr);
	downlink_counter++;
	if(update_db_by_addr_uint(db, "downcnt","nsdevinfo",devAddr,downlink_counter)==FAILED){
		MSG("WARNING: [down] update the database failed\n");
                sqlite3_close(db);
		return -1;
	}
	residue=frame_size%3;
	if(residue==1){
		max_len=(frame_size+2)/3*4+1;
		frame=malloc(sizeof(char)*max_len);
	}
	else if(residue==2){
		max_len=(frame_size+1)/3*4+1;
		frame=malloc(sizeof(char)*max_len);
	}
	else{
		max_len=frame_size/3*4+1;
		frame=malloc(sizeof(char)*max_len);
	}
	bin_to_b64(frame_raw,frame_size,frame,max_len);
	/*parsing the frame to json string*/
	root_val=json_value_init_object();
	root_obj=json_value_get_object(root_val);
	json_object_dotset_number(root_obj,"app.devaddr",devAddr);
	json_object_dotset_string(root_obj,"app.control.frame",frame);
	json_object_dotset_number(root_obj,"app.control.size",frame_size);
	json_str=json_serialize_to_string_pretty(root_val);
	strcpy(json_data,json_str);
	json_free_serialized_string(json_str);
	json_value_free(root_val);
	free(frame);

        sqlite3_close(db);

	return 1;
}

void nc_msg_handle(const char* msg, int index, struct command_info* cmd_info) {
	/*JSON parsing variables*/
	JSON_Value *root_val=NULL;
	JSON_Object *app_obj=NULL;
	JSON_Object *devx_obj=NULL;
	JSON_Object *gwrx_obj=NULL;
	JSON_Object *maccmd_obj=NULL;
	JSON_Value *val=NULL;

	char msgname[64];
	char tempstr[64];
	char content[512];
	char gwaddr[16];
	char deveui_hex[17];
	uint32_t devaddr;
	uint32_t seqno;
	int isencrypt;
	const char* str;
	uint8_t cmd_payload[CMD_UP_MAX];
	uint8_t command[CMD_UP_MAX];
	int psize;
	uint8_t nwkSKey[16];

	int i=0;
	int j=0;
	uint8_t cid;

        sqlite3* db;

	bzero(msgname,sizeof(msgname));
	bzero(content,sizeof(content));
	bzero(gwaddr,sizeof(gwaddr));
	bzero(deveui_hex,sizeof(deveui_hex));
	root_val=json_parse_string_with_comments(msg);
	if(root_val==NULL){
		MSG("WARNING: [up] message_%d contains invalid JSON\n",index);
		json_value_free(root_val);
	}
	app_obj=json_object_get_object(json_value_get_object(root_val),"app");
	if(app_obj==NULL){
		MSG("WARNING: [up] message received contains no \"app\"\n");
		json_value_free(root_val);
	}
	else{
		snprintf(msgname,sizeof(msgname),"###############command_message_%d:###############\n",index);
		strcat(content,msgname);
		val=json_object_get_value(app_obj,"gwaddr");
		if(val!=NULL){
			strcpy(gwaddr,json_value_get_string(val));
			snprintf(tempstr,sizeof(tempstr),"gwaddr:%s",json_value_get_string(val));
			strcat(content,tempstr);
		}
		val=json_object_get_value(app_obj,"deveui");
		if(val!=NULL){
			strcpy(deveui_hex,json_value_get_string(val));
			snprintf(tempstr,sizeof(tempstr)," deveui:%s",json_value_get_string(val));
			strcat(content,tempstr);
		}
		val=json_object_get_value(app_obj,"devaddr");
		if(val!=NULL){
			devaddr=(uint32_t)json_value_get_number(val);
			snprintf(tempstr,sizeof(tempstr)," devaddress:%.0f",json_value_get_number(val));
			strcat(content,tempstr);
		}
		val=json_object_get_value(app_obj,"dir");
		if(val!=NULL){
			snprintf(tempstr,sizeof(tempstr)," direction:%s",json_value_get_string(val));
			strcat(content,tempstr);
		}
		val=json_object_get_value(app_obj,"seqno");
		if(val!=NULL){
			seqno=json_value_get_number(val);
			snprintf(tempstr,sizeof(tempstr)," sequenceNo:%.0f",json_value_get_number(val));
			strcat(content,tempstr);
		}
		devx_obj=json_object_get_object(app_obj,"devx");
		if(devx_obj==NULL){
			MSG("WARNING: [up] message received contains no \"devx\" in \"app\"\n");
		}
		else{
			val=json_object_get_value(devx_obj,"freq");
			if(val!=NULL){
				snprintf(tempstr,sizeof(tempstr)," frequence:%.6f",json_value_get_number(val));
				strcat(content,tempstr);
			}
			val=json_object_get_value(devx_obj,"modu");
			if(val!=NULL){
				snprintf(tempstr,sizeof(tempstr)," modulation:%s",json_value_get_string(val));
				strcat(content,tempstr);
				if(strcmp("LORA",json_value_get_string(val))==0){
					val=json_object_get_value(devx_obj,"datr");
					if(val!=NULL){
						snprintf(tempstr,sizeof(tempstr)," datr:%s",json_value_get_string(val));
						strcat(content,tempstr);
					}
				}
				else{
					val=json_object_get_value(devx_obj,"datr");
					if(val!=NULL){
						snprintf(tempstr,sizeof(tempstr)," datarate:%.2f",json_value_get_number(val));
						strcat(content,tempstr);
					}
				}
			}
			val=json_object_get_value(devx_obj,"codr");
			if(val!=NULL){
				snprintf(tempstr,sizeof(tempstr)," coderate:%s",json_value_get_string(val));
				strcat(content,tempstr);
			}
			val=json_object_get_value(devx_obj,"adr");
			if(val!=NULL){
				snprintf(tempstr,sizeof(tempstr)," adr:%d",json_value_get_boolean(val));
				strcat(content,tempstr);
			}
		}
		gwrx_obj=json_object_get_object(app_obj,"gwrx");
		if(gwrx_obj==NULL){
			MSG("WARNING: [up] message received contains no \"gwrx\" in \"app\"\n");
		}
		else{
			val=json_object_get_value(gwrx_obj,"time");
			if(val!=NULL){
				snprintf(tempstr,sizeof(tempstr)," time:%s",json_value_get_string(val));
				strcat(content,tempstr);
			}
			val=json_object_get_value(gwrx_obj,"chan");
			if(val!=NULL){
				snprintf(tempstr,sizeof(tempstr)," channel:%.0f",json_value_get_number(val));
				strcat(content,tempstr);
			}
			val=json_object_get_value(gwrx_obj,"rfch");
			if(val!=NULL){
				snprintf(tempstr,sizeof(tempstr)," rfchannel:%.0f",json_value_get_number(val));
				strcat(content,tempstr);
			}
			val=json_object_get_value(gwrx_obj,"rssi");
			if(val!=NULL){
				snprintf(tempstr,sizeof(tempstr)," rssi:%.0f",json_value_get_number(val));
				strcat(content,tempstr);
			}
			val=json_object_get_value(gwrx_obj,"lsnr");
			if(val!=NULL){
				snprintf(tempstr,sizeof(tempstr)," lsnr:%.2f",json_value_get_number(val));
				strcat(content,tempstr);
			}
		}
		maccmd_obj=json_object_get_object(app_obj,"maccmd");
		if(maccmd_obj==NULL){
			MSG("WARNING: [up] message received contains no \"maccmd\" in \"app\"\n");
		}
		else{
			val=json_object_get_value(maccmd_obj,"isencrypt");
			if(val!=NULL){
				isencrypt=json_value_get_boolean(val);
				snprintf(tempstr,sizeof(tempstr)," isencrypt:%d",json_value_get_boolean(val));
				strcat(content,tempstr);
			}
			val=json_object_get_value(maccmd_obj,"command");
			if(val!=NULL){
				str=json_value_get_string(val);
				psize=b64_to_bin(str,strlen(str),cmd_payload,CMD_UP_MAX);
				if(isencrypt==1){
                                        if (sqlite3_open("/tmp/loraserv", &db)) {
                                                fprintf(stderr, "ERROR: Can't open database: %s\n", sqlite3_errmsg(db));
                                                sqlite3_close(db);
                                                return;
                                        }
					if(query_db_by_addr(db, "nwkskey","nsdevinfo",devaddr,nwkSKey,16)==FAILED){
							MSG("WARNING: [down] query the database failed\n");
                                                        sqlite3_close(db);
							return;
					}
                                        sqlite3_close(db);
					/*compute the LoRaMAC frame payload decryption*/
					LoRaMacPayloadDecrypt(cmd_payload,psize,nwkSKey,devaddr,UP,seqno,command);
				}
				else{
					memcpy(command,cmd_payload,psize);
				}
				cmd_info->devaddr=devaddr;
				while(i<psize){
					cid=command[i];
					switch(cid){
						case MOTE_MAC_LINK_CHECK_REQ:{
							/*
							 * TODO:
						 	 * call command_handle()
						 	 */
							cmd_info->type[j]=MOTE_MAC_LINK_CHECK_REQ;
							cmd_info->isworked[j]=true;
							strcpy(tempstr,"\ncommand:LINK_CHECK_REQUEST");
							strcat(content,tempstr);
							break;
						}
						case MOTE_MAC_LINK_ADR_ANS:{
							MacCmdADRStatus_t status;
							cmd_info->type[j]=MOTE_MAC_LINK_ADR_ANS;
							status.Value=command[++i];
							strcpy(tempstr,"\ncommand:LINK_ADR_ANSWER");
							strcat(content,tempstr);
							status.Bits.ChannelMaskAck==1 ? strcpy(tempstr,"   channel mask OK"):strcpy(tempstr,"   channel mask INVALID");
							strcat(content,tempstr);
							status.Bits.DatarateAck==1 ? strcpy(tempstr,"   datarate OK"):strcpy(tempstr,"   datarate INVALID");
							strcat(content,tempstr);
							status.Bits.PowerAck==1 ? strcpy(tempstr,"   power OK"):strcpy(tempstr,"   power INVALID");
							if(status.Bits.ChannelMaskAck==1&&status.Bits.DatarateAck==1&&status.Bits.PowerAck==1)
								cmd_info->isworked[j]=true;
							else
								cmd_info->isworked[j]=false;
							strcat(content,tempstr);
							break;
						}
						case MOTE_MAC_DUTY_CYCLE_ANS:{
							cmd_info->type[j]=MOTE_MAC_DUTY_CYCLE_ANS;
							cmd_info->isworked[j]=true;
							strcpy(tempstr,"\ncommand:DUTY_CYCLE_ANSWER");
							strcat(content,tempstr);
							break;
						}
						case MOTE_MAC_RX_PARAM_SETUP_ANS:{
							MacCmdRXStatus_t status;
							cmd_info->type[j]=MOTE_MAC_RX_PARAM_SETUP_ANS;
							status.Value=command[++i];
							strcpy(tempstr,"\ncommand:RX_PARAM_SETUP_ANSWER");
							strcat(content,tempstr);
							status.Bits.ChannelAck==1 ? strcpy(tempstr,"   channel OK"):strcpy(tempstr,"   channel INVALID");
							strcat(content,tempstr);
							status.Bits.RX2DatarateAck==1 ? strcpy(tempstr,"   RX2 datarate OK"):strcpy(tempstr,"   RX2 datarate INVALID");
							strcat(content,tempstr);
							status.Bits.RX1DRoffsetAck==1 ? strcpy(tempstr,"   RX1 datarate offset OK"):strcpy(tempstr,"   RX1 datarate offset INVALID");
							strcat(content,tempstr);
							if(status.Bits.ChannelAck==1&&status.Bits.RX2DatarateAck==1&&status.Bits.RX2DatarateAck==1)
								cmd_info->isworked[j]=true;
							else
								cmd_info->isworked[j]=false;
							break;
						}
						case MOTE_MAC_DEV_STATUS_ANS:{
							uint8_t battery;
							MacCmdMargin_t margin;
							cmd_info->type[j]=MOTE_MAC_DEV_STATUS_ANS;
							cmd_info->isworked[j]=true;
							battery=command[++i];
							margin.Value=command[++i];
							strcpy(tempstr,"\ncommand:DEVICE_STATUS_ANSWER");
							strcat(content,tempstr);
							snprintf(tempstr,sizeof(tempstr),"   battery:%d",battery);
							strcat(content,tempstr);
							snprintf(tempstr,sizeof(tempstr),"   margin:%d",margin.Bits.Margin);
							strcat(content,tempstr);
							break;
						}
						case MOTE_MAC_NEW_CHANNEL_ANS:{
							MacCmdChannelStatus_t status;
							cmd_info->type[j]=MOTE_MAC_NEW_CHANNEL_ANS;
							status.Value=command[++i];
							strcpy(tempstr,"\ncommand:NEW_CHANNEL_ANSWER");
							strcat(content,tempstr);
							status.Bits.ChannelFreqAck==1 ? strcpy(tempstr,"   channel frequency OK"):strcpy(tempstr,"   channel frequency INVALID");
							strcat(content,tempstr);
							status.Bits.DatarateRangeAck==1 ? strcpy(tempstr,"   datarate range OK"):strcpy(tempstr,"   datarate range INVALID");
							strcat(content,tempstr);
							if(status.Bits.ChannelFreqAck==1&&status.Bits.DatarateRangeAck)
								cmd_info->isworked[j]=true;
							else
								cmd_info->isworked[j]=false;
							break;
						}
						case MOTE_MAC_RX_TIMING_SETUP_ANS:{
							cmd_info->type[j]=MOTE_MAC_RX_TIMING_SETUP_ANS;
							cmd_info->isworked[j]=true;
							strcpy(tempstr,"\ncommand:RX_TIMING_SETUP_ANSWER");
							strcat(content,tempstr);
							break;
						}
						default:
							break;
					}
					++i;
					++j;
				}
				cmd_info->cmd_num=j;
			}
		}
	json_value_free(root_val);
	}
	MSG("%s\n",content);
}

void nc_prepare_frame(uint8_t type, uint32_t devAddr, uint32_t downcnt, const uint8_t* payload, const uint8_t* nwkSKey, int payload_size,uint8_t* frame, int* frame_size){
	LoRaMacHeader_t hdr;
	LoRaMacFrameCtrl_t fctrl;
	uint8_t index=0;
	uint8_t* encpayload;
	uint32_t mic;

	/*MHDR*/
	hdr.Value=0;
	hdr.Bits.MType=type;
	frame[index]=hdr.Value;

	/*DevAddr*/
	frame[++index]=devAddr&0xFF;
	frame[++index]=(devAddr>>8)&0xFF;
	frame[++index]=(devAddr>>16)&0xFF;
	frame[++index]=(devAddr>>24)&0xFF;

	/*FCtrl*/
	fctrl.Value=0;
	if(type==FRAME_TYPE_DATA_UNCONFIRMED_DOWN){
		fctrl.Bits.Ack=1;
	}
	fctrl.Bits.Adr=1;
	frame[++index]=fctrl.Value;

	/*FCnt*/
	frame[++index]=(downcnt)&0xFF;
	frame[++index]=(downcnt>>8)&0xFF;

	/*FOpts*/
	/*Fport*/
	frame[++index]=0;

	/*encrypt the payload*/
	encpayload=malloc(sizeof(uint8_t)*payload_size);
	LoRaMacPayloadEncrypt(payload,payload_size,nwkSKey,devAddr,DOWN,downcnt,encpayload);
	++index;
	memcpy(frame+index,encpayload,payload_size);
	free(encpayload);
	index+=payload_size;

	/*calculate the mic*/
	LoRaMacComputeMic(frame,index,nwkSKey,devAddr,DOWN,downcnt,&mic);
	frame[index]=mic&0xFF;
	frame[++index]=(mic>>8)&0xFF;
	frame[++index]=(mic>>16)&0xFF;
	frame[++index]=(mic>>24)&0xFF;
	*frame_size=index+1;
}

/* ---------------------------------------------------------------------------- */
/* ---------- process string message ------------------------------------------ */

void assign_msg_trans(void* data,const void* msg){
	struct msg_trans* data_x=(struct msg_trans*)data;
	struct msg_trans* msg_x=(struct msg_trans*)msg;
	data_x->devaddr=msg_x->devaddr;
	data_x->rx1_dr=msg_x->rx1_dr;
	data_x->rx2_dr=msg_x->rx2_dr;
	data_x->rx2_freq=msg_x->rx2_freq;
}

void assign_msg_rxdelay(void* data,const void* msg){
	struct msg_rxdelay* data_x=(struct msg_rxdelay*)data;
	struct msg_rxdelay* msg_x=(struct msg_rxdelay*)msg;
	data_x->devaddr=msg_x->devaddr;
	data_x->delay=msg_x->delay;
}

void copy_msg_trans(void* data,const void* msg){
	assign_msg_trans(data,msg);
}

void copy_msg_rxdelay(void* data,const void* msg){
	assign_msg_rxdelay(data,msg);
}

int compare_msg_trans(const void* data,const void* key){
	if(((struct msg_trans*)data)->devaddr==*(uint32_t*)key)
		return 0;
	else
		return 1;
}

int compare_msg_rxdelay(const void* data,const void* key){
	if(((struct msg_rxdelay*)data)->devaddr==*(uint32_t*)key)
		return 0;
	else
		return 1;
}
int compare_msg_down(const void* data, const void* key) {
	return strcmp(((struct msg_down*)data)->gwaddr, (const char*)key);
}

int compare_msg_delay(const void* data, const void* key) {
	if(((struct msg_delay*)data)->devaddr == *(uint32_t*)key)
		return  0;
	else
		return  1;
}

int compare_msg_join(const void* data, const void*key) {
	return strcmp(((struct msg_join*)data)->deveui_hex, (const char*)key);
}

void assign_msg(void* data, const void* msg) {
		struct msg* data_x = (struct msg*)data;
		struct msg* msg_x = (struct msg*)msg;
		data_x->json_string = msg_x->json_string;
}

void assign_msg_down(void* data, const void* msg) {
	struct msg_down* data_x = (struct msg_down*)data;
	struct msg_down* msg_x = (struct msg_down*)msg;
	data_x->gwaddr = msg_x->gwaddr;
	data_x->json_string = msg_x->json_string;
}

void assign_msg_join(void* data, const void* msg) {
	struct msg_join* data_x = (struct msg_join*)data;
	struct msg_join* msg_x = (struct msg_join*)msg;
	strcpy(data_x->deveui_hex, msg_x->deveui_hex);
	data_x->tmst = msg_x->tmst;
}

void assign_msg_delay(void* data, const void* msg) {
	struct msg_delay* data_x = (struct msg_delay*)data;
	struct msg_delay* msg_x = (struct msg_delay*)msg;
	data_x->devaddr = msg_x->devaddr;
	strcpy(data_x->deveui_hex, msg_x->deveui_hex);
	data_x->frame = msg_x->frame;
	data_x->size = msg_x->size;
}

void copy_msg_down(void* data, const void* msg) {
	struct msg_down* data_x = (struct msg_down*)data;
	struct msg_down* msg_x = (struct msg_down*)msg;
	strcpy(data_x->json_string, msg_x->json_string);
	strcpy(data_x->gwaddr, msg_x->gwaddr);
}

void copy_msg_delay(void* data, const void* msg){
	struct msg_delay* data_x = (struct msg_delay*)data;
	struct msg_delay* msg_x = (struct msg_delay*)msg;
	data_x->devaddr = msg_x->devaddr;
	strcpy(data_x->deveui_hex, msg_x->deveui_hex);
	strcpy(data_x->frame, msg_x->frame);
	data_x->size = msg_x->size;
}

void copy_msg_join(void* data, const void* msg) {
	assign_msg_join(data, msg);
}

void destroy_msg(void* msg){
	struct msg* data = (struct msg*)msg;
	free(data->json_string);
}

void destroy_msg_down(void* msg){
	struct msg_down* message = (struct msg_down*)msg;
	free(message->json_string);
	free(message->gwaddr);
}

void destroy_msg_delay(void* msg) {
	struct msg_delay* message = (struct msg_delay*)msg;
	free(message->frame);
}

void i8_to_hexstr(uint8_t* uint, char* str, int size) {
	/*in case that the str has a value,strcat() seems not safe*/
	bzero(str, size * 2 + 1);
	char tempstr[3];
	int i;
	for (i = 0; i < size; i++) {
		snprintf(tempstr, sizeof(tempstr), "%02x", uint[i]);
		strcat(str, tempstr);
	}
}

/* -------------------------------------------------------------*/
/* ----- common function ---------------------------------------*/

/*reverse memory copy*/
void revercpy( uint8_t *dst, const uint8_t *src, int size) {
    dst = dst + ( size - 1 );
    while( size-- ) {
        *dst-- = *src++;
    }
}

/*timer function*/
void set_timer(int sec,int msec) {
	/*it seems the select() will restart when interrupted by SIGINT*/
	struct timeval timeout;
	timeout.tv_sec = sec;
	timeout.tv_usec = msec;
	select(0, NULL, NULL, NULL, &timeout);
}

/*get TCP socket and bind it*/
void tcp_bind(const char* servaddr, const char* port, int* listenfd) {
	struct addrinfo hints;
	struct addrinfo *results,*r;
	int i;
	char host_name[64];
	char service_name[64];
	int listen;

	/*try to open and bind TCP listening socket*/
	bzero(&hints, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_INET;
	if (getaddrinfo(servaddr, port, &hints, &results) != 0) {
		MSG("ERROR: [down] getaddrinfo on address %s (PORT %s) returned %s\n", servaddr, port, gai_strerror(i));
		exit(EXIT_FAILURE);
	}
	r = results;
	do{
		listen = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
		if (listen < 0)
			continue;
		if (bind(listen, r->ai_addr, r->ai_addrlen) == 0)
			break;				/*success*/
		close(listen);		/*bind error,close and try next one*/
	} while ((r = r->ai_next) != NULL);

	if (r == NULL) {
		MSG("ERROR: [down] failed to open listening socket to any of server %s addresses (port %s)\n", servaddr, port);
		i = 1;
		for (r = results; r != NULL; r = r->ai_next) {
			getnameinfo(r->ai_addr, r->ai_addrlen, host_name, sizeof(host_name), service_name, sizeof(service_name), NI_NUMERICHOST);
			MSG("INFO: [down] result %i host:%s service:%s\n", i, host_name, service_name);
			i++;
		}
		exit(EXIT_FAILURE);
	}
	*listenfd = listen;
	freeaddrinfo(results);
}

/*get the socket fd and connect to the server*/
void tcp_connect(const char* servaddr, const char* port, int* sockfd, bool* exit_sig) {
	/*some  variables for building TCP sockets*/
	struct addrinfo hints;
	struct addrinfo *results;
	struct addrinfo *r;
	int i;
	char host_name[64];
	char service_name[64];
	int sock;
	const int on = 1;

	/*try to open and connect socket */
	while (! *exit_sig) {
		bzero(&hints, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		if (getaddrinfo(servaddr, port, &hints, &results) != 0) {
			MSG("ERROR: [up] getaddrinfo on address %s (PORT %s) returned %s\n", servaddr, port, gai_strerror(i));
			exit(EXIT_FAILURE);
		}
		for (r = results; r != NULL; r = r->ai_next) {
			sock = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
			if (sock < 0)
				continue;
			setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
			if (connect(sock, r->ai_addr, r->ai_addrlen) == 0)
				break;
			close(sock);/*connect error,close and try again*/
		}
		if (r == NULL) {
			MSG("ERROR: [up] failed to open socket to any of server %s addresses (port %s)\n", servaddr, port);
			i = 1;
			for (r = results; r != NULL; r = r->ai_next){
				getnameinfo(r->ai_addr, r->ai_addrlen, host_name, sizeof(host_name), service_name, sizeof(service_name), NI_NUMERICHOST);
				MSG("INFO: [up] result %i host:%s service:%s\n", i, host_name, service_name);
				i++;
			}
		} else {
			/*the connection to the server is established, break*/
			*sockfd = sock;
			MSG("INFO: connect to the server %s addresses (port %s) successfully\n", servaddr, port);
			freeaddrinfo(results);
			break;
		}
		freeaddrinfo(results);
		set_timer(SOCKET_INTERVAL_S, SOCKET_INTERVAL_MS);
	}
}

/*get the udp socket fd and bind it*/
void udp_bind(const char* servaddr, const char* port, int* sockfd) {
	/*some  variables for building UDP sockets*/
	struct addrinfo hints;
	struct addrinfo *results;
	struct addrinfo *r;
	int i;
	char host_name[64];
	char service_name[64];
	int sock;

	/*try to open and bind UDP socket */
	bzero(&hints,sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	if (getaddrinfo(servaddr, port, &hints, &results) != 0) {
		MSG("ERROR: [up] getaddrinfo on address %s (PORT %s) returned %s\n", servaddr, port, gai_strerror(i));
		exit(EXIT_FAILURE);
	}
	for (r = results; r != NULL; r = r->ai_next) {
		sock = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
		if (sock < 0)
			continue;
		if (bind(sock, r->ai_addr, r->ai_addrlen) == 0)
			break;
		close(sock);/*bind error,close and try again*/
	}
	if ( r== NULL) {
		MSG("ERROR: [up] failed to open socket to any of server %s addresses (port %s)\n", servaddr, port);
		i = 1;
		for (r = results; r != NULL; r = r->ai_next) {
			getnameinfo(r->ai_addr, r->ai_addrlen, host_name, sizeof(host_name), service_name, sizeof(service_name), NI_NUMERICHOST);
			MSG("INFO: [up] result %i host:%s service:%s\n", i, host_name, service_name);
			i++;
		}
		exit(EXIT_FAILURE);
	}
	*sockfd = sock;
	freeaddrinfo(results);
}


