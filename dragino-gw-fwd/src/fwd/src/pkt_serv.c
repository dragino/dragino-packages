/*
 *  ____  ____      _    ____ ___ _   _  ___  
 *  |  _ \|  _ \    / \  / ___|_ _| \ | |/ _ \ 
 *  | | | | |_) |  / _ \| |  _ | ||  \| | | | |
 *  | |_| |  _ <  / ___ \ |_| || || |\  | |_| |
 *  |____/|_| \_\/_/   \_\____|___|_| \_|\___/ 
 *
 * Dragino_gw_fwd -- An opensource lora gateway forward 
 *
 * See http://www.dragino.com for more information about
 * the lora gateway project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 *
 * Maintainer: skerlan
 *
 */

/*!
 * \file
 * \brief 
 *  Description:
*/

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include <semaphore.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include "db.h"
#include "fwd.h"
#include "jitqueue.h"
#include "loragw_hal.h"
#include "loragw_aux.h"

#include "service.h"
#include "pkt_service.h"
#include "loramac-crypto.h"
#include "mac-header-decode.h"

DECLARE_GW;
DECLARE_HAL;

static uint8_t rx2bw;
static uint8_t rx2dr;
static uint32_t rx2freq;

static char db_family[32] = {'\0'};
static char db_key[32] = {'\0'};
static char tmpstr[16] = {'\0'};

LGW_LIST_HEAD_STATIC(dn_list, _dn_pkt); // downlink for customer

static void pkt_prepare_downlink(void* arg);
static void pkt_deal_up(void* arg);
static void prepare_frame(uint8_t, devinfo_s*, uint32_t, const uint8_t*, int, uint8_t*, int*);

static enum jit_error_e custom_rx2dn(dn_pkt_s* dnelem, devinfo_s *devinfo, uint32_t us, uint8_t txmode) {
    int i, fsize = 0;

    uint32_t dwfcnt = 0;

    uint8_t payload_en[DEFAULT_PAYLOAD_SIZE] = {'\0'};  /* data which have decrypted */
    struct lgw_pkt_tx_s txpkt;

    uint32_t current_concentrator_time;
    enum jit_error_e jit_result = JIT_ERROR_OK;
    enum jit_pkt_type_e downlink_type;

    memset(&txpkt, 0, sizeof(txpkt));
    txpkt.modulation = MOD_LORA;
    txpkt.count_us = us + 2000000UL; /* rx2 window plus 1s */
    txpkt.no_crc = true;
    txpkt.freq_hz = rx2freq; /* same as the up */
    txpkt.rf_chain = 0;
    txpkt.rf_power = 20;
    txpkt.datarate = rx2dr;
    txpkt.bandwidth = rx2bw;
    txpkt.coderate = CR_LORA_4_5;
    txpkt.invert_pol = true;
    txpkt.preamble = STD_LORA_PREAMB;
    txpkt.tx_mode = txmode;
    if (txmode)
        downlink_type = JIT_PKT_TYPE_DOWNLINK_CLASS_A;
    else
        downlink_type = JIT_PKT_TYPE_DOWNLINK_CLASS_C;

    /* 这个key重启将会删除, 下发的计数器 */
    sprintf(db_family, "/downlink/%08X", devinfo->devaddr);
    if (lgw_db_get(db_family, "fcnt", tmpstr, sizeof(tmpstr)) == -1) {
        lgw_db_put(db_family, "fcnt", "0");
    } else { 
        dwfcnt = atol(tmpstr);
        sprintf(tmpstr, "%u", dwfcnt + 1);
        lgw_db_put(db_family, "fcnt", tmpstr);
    }

    /* prepare MAC message */
    lgw_memset(payload_en, '\0', sizeof(payload_en));

    prepare_frame(FRAME_TYPE_DATA_UNCONFIRMED_DOWN, devinfo, dwfcnt++, (uint8_t *)dnelem->payload, dnelem->psize, payload_en, &fsize);

    lgw_memcpy(txpkt.payload, payload_en, fsize);

    txpkt.size = fsize;

    lgw_log(LOG_DEBUG, "DEBUG~ [pkt-dwn] TX(%d):", fsize);
    for (i = 0; i < fsize; ++i) {
        lgw_log(LOG_DEBUG, "%02X", payload_en[i]);
    }
    lgw_log(LOG_DEBUG, "\n");

    pthread_mutex_lock(&GW.hal.mx_concent);
    HAL.lgw_get_instcnt(&current_concentrator_time);
    pthread_mutex_unlock(&GW.hal.mx_concent);
    jit_result = jit_enqueue(&GW.tx.jit_queue[txpkt.rf_chain], current_concentrator_time, &txpkt, downlink_type);
    lgw_log(LOG_DEBUG, "DEBUG~ [pkt-dwn] DNRX2-> tmst:%u, freq:%u, psize:%u.\n", txpkt.count_us, txpkt.freq_hz, txpkt.size);
    return jit_result;
}

static void prepare_frame(uint8_t type, devinfo_s *devinfo, uint32_t downcnt, const uint8_t* payload, int payload_size, uint8_t* frame, int* frame_size) {
	uint32_t mic;
	uint8_t index = 0;
	uint8_t* encrypt_payload;

	LoRaMacHeader_t hdr;
	LoRaMacFrameCtrl_t fctrl;

	/*MHDR*/
	hdr.Value = 0;
	hdr.Bits.MType = type;
	frame[index] = hdr.Value;

	/*DevAddr*/
	frame[++index] = devinfo->devaddr&0xFF;
	frame[++index] = (devinfo->devaddr>>8)&0xFF;
	frame[++index] = (devinfo->devaddr>>16)&0xFF;
	frame[++index] = (devinfo->devaddr>>24)&0xFF;

	/*FCtrl*/
	fctrl.Value = 0;
	if(type == FRAME_TYPE_DATA_UNCONFIRMED_DOWN){
		fctrl.Bits.Ack = 1;
	}
	fctrl.Bits.Adr = 1;
	frame[++index] = fctrl.Value;

	/*FCnt*/
	frame[++index] = (downcnt)&0xFF;
	frame[++index] = (downcnt>>8)&0xFF;

	/*FOpts*/
	/*Fport*/
	frame[++index] = (DEFAULT_DOWN_FPORT)&0xFF;

	/*encrypt the payload*/
	encrypt_payload = lgw_malloc(sizeof(uint8_t) * payload_size);
	LoRaMacPayloadEncrypt(payload, payload_size, (DEFAULT_DOWN_FPORT == 0) ? devinfo->nwkskey : devinfo->appskey, devinfo->devaddr, DOWN, downcnt, encrypt_payload);
	++index;
	memcpy(frame + index, encrypt_payload, payload_size);
	lgw_free(encrypt_payload);
	index += payload_size;

	/*calculate the mic*/
	LoRaMacComputeMic(frame, index, devinfo->nwkskey, devinfo->devaddr, DOWN, downcnt, &mic);
    //printf("INFO~ [MIC] %08X\n", mic);
	frame[index] = mic&0xFF;
	frame[++index] = (mic>>8)&0xFF;
	frame[++index] = (mic>>16)&0xFF;
	frame[++index] = (mic>>24)&0xFF;
	*frame_size = index + 1;
}

static dn_pkt_s* search_dn_list(const char* addr) {
    dn_pkt_s* entry = NULL;

    LGW_LIST_LOCK(&dn_list);
    LGW_LIST_TRAVERSE(&dn_list, entry, list) {
        if (!strcasecmp(entry->devaddr, addr))
            break;
    }
    LGW_LIST_UNLOCK(&dn_list);

    return entry;
}


int pkt_start(serv_s* serv) {
    if (lgw_pthread_create_background(&serv->thread.t_up, NULL, (void *(*)(void *))pkt_deal_up, serv)) {
        lgw_log(LOG_WARNING, "WARNING~ [%s] Can't create packages deal pthread.\n", serv->info.name);
        return -1;
    }

    if (GW.cfg.custom_downlink) {
        switch (GW.cfg.region) {
            case EU: 
                rx2dr = DR_LORA_SF12;
                rx2bw = BW_125KHZ;
                rx2freq = 869525000UL;
                break;
            case US:
                rx2dr = DR_LORA_SF12;
                rx2bw = BW_500KHZ;
                rx2freq = 923300000UL;
                break;
            case CN:
                rx2dr = DR_LORA_SF12;
                rx2bw = BW_125KHZ;
                rx2freq = 505300000UL;
                break;
            case AU:
                rx2dr = DR_LORA_SF12;
                rx2bw = BW_500KHZ;
                rx2freq = 923300000UL;
                break;
            default:
                rx2dr = DR_LORA_SF12;
                rx2bw = BW_125KHZ;
                rx2freq = 921900000UL;
                break;
        }

        if (lgw_pthread_create_background(&serv->thread.t_down, NULL, (void *(*)(void *))pkt_prepare_downlink, serv)) {
            lgw_log(LOG_WARNING, "WARNING~ [%s] Can't create pthread for custom downlonk.\n", serv->info.name);
            return -1;
        }
    }

    lgw_db_put("service/pkt", serv->info.name, "runing");
    lgw_db_put("thread", serv->info.name, "runing");

    return 0;
}

void pkt_stop(serv_s* serv) {
    serv->thread.stop_sig = true;
	sem_post(&serv->thread.sema);
	pthread_join(serv->thread.t_up, NULL);
	pthread_join(serv->thread.t_down, NULL);
    lgw_db_del("service/pkt", serv->info.name);
    lgw_db_del("thread", serv->info.name);
}

static void pkt_deal_up(void* arg) {
    serv_s* serv = (serv_s*) arg;
    lgw_log(LOG_DEBUG, "DEBUG~ [%s] Staring pkt_deal_up thread\n", serv->info.name);

	int i;					/* loop variables */
    int fsize = 0;
    int index = 0;
    int nb_pkt = 0;

	struct lgw_pkt_rx_s *p;	/* pointer on a RX packet */

    rxpkts_s* rxpkt_entry = NULL;

	int buff_index;

    enum jit_error_e jit_result = JIT_ERROR_OK;
    
    uint8_t payload_encrypt[DEFAULT_PAYLOAD_SIZE] = {'\0'};
    uint8_t payload_txt[DEFAULT_PAYLOAD_SIZE] = {'\0'};

    LoRaMacMessageData_t macmsg;

	while (!serv->thread.stop_sig) {
		// wait for data to arrive
		sem_wait(&serv->thread.sema);

        nb_pkt = get_rxpkt(serv);     //only get the first rxpkt of list

        if (nb_pkt == 0)
            continue;

        lgw_log(LOG_DEBUG, "DEBUG~ [%s] pkt_push_up fetch %d pachages.\n", serv->info.name, nb_pkt);

        /* serialize one Lora packet metadata and payload */
        for (i = 0; i < nb_pkt; i++) {
            p = &serv->rxpkt[i];

            if (p->size < 13) /* offset of frmpayload */
                continue;

            macmsg.Buffer = p->payload;
            macmsg.BufSize = p->size;

            if (LoRaMacParserData(&macmsg) != LORAMAC_PARSER_SUCCESS)
                continue;

            decode_mac_pkt_up(&macmsg, p);

            if (GW.cfg.mac_decoded || GW.cfg.custom_downlink) {
                devinfo_s devinfo = { .devaddr = macmsg.FHDR.DevAddr, 
                                      .devaddr_str = {0},
                                      .appskey_str = {0},
                                      .nwkskey_str = {0}
                                    };
                sprintf(db_family, "devinfo/%08X", devinfo.devaddr);
                if ((lgw_db_get(db_family, "appskey", devinfo.appskey_str, sizeof(devinfo.appskey_str)) == -1) || 
                    (lgw_db_get(db_family, "nwkskey", devinfo.nwkskey_str, sizeof(devinfo.nwkskey_str)) == -1)) {
                    continue;
                }

                str2hex(devinfo.appskey, devinfo.appskey_str, sizeof(devinfo.appskey));
                str2hex(devinfo.nwkskey, devinfo.nwkskey_str, sizeof(devinfo.nwkskey));

                /* Debug message of appskey */
                lgw_log(LOG_DEBUG, "\nDEBUG~ [MAC-Decode]appskey:");
                for (i = 0; i < (int)sizeof(devinfo.appskey); ++i) {
                    lgw_log(LOG_DEBUG, "%02X", devinfo.appskey[i]);
                }
                lgw_log(LOG_DEBUG, "\n");

                if (GW.cfg.mac_decoded) {
                    fsize = p->size - 13 - macmsg.FHDR.FCtrl.Bits.FOptsLen; 
                    memcpy(payload_encrypt, p->payload + 9 + macmsg.FHDR.FCtrl.Bits.FOptsLen, fsize);
                    LoRaMacPayloadDecrypt(payload_encrypt, fsize, devinfo.appskey, devinfo.devaddr, UP, (uint32_t)macmsg.FHDR.FCnt, payload_txt);

                    /* Debug message of decoded payload */
                    lgw_log(LOG_DEBUG, "\nDEBUG~ [MAC-Decode] RX(%d):", fsize);
                    for (i = 0; i < fsize; ++i) {
                        lgw_log(LOG_DEBUG, "%02X", payload_txt[i]);
                    }
                    lgw_log(LOG_DEBUG, "\n");

                    if (GW.cfg.mac2file) {
                        FILE *fp;
                        char pushpath[128];
                        snprintf(pushpath, sizeof(pushpath), "/var/iot/channels/%08X", devinfo.devaddr);
                        fp = fopen(pushpath, "w+");
                        if (NULL == fp)
                            lgw_log(LOG_INFO, "INFO~ [Decrypto] Fail to open path: %s\n", pushpath);
                        else { 
                            fwrite(payload_txt, sizeof(uint8_t), fsize, fp);
                            fflush(fp); 
                            fclose(fp);
                        }
                    
                    }

                    if (GW.cfg.mac2db) { /* 每个devaddr最多保存10个payload */
                        sprintf(db_family, "/payload/%08X", devinfo.devaddr);
                        if (lgw_db_get(db_family, "index", tmpstr, sizeof(tmpstr)) == -1) {
                            lgw_db_put(db_family, "index", "0");
                        } else 
                            index = atoi(tmpstr) % 9;
                        sprintf(db_family, "/payload/%08X/%d", devinfo.devaddr, index);
                        sprintf(db_key, "%u", p->count_us);
                        lgw_db_put(db_family, db_key, (char*)payload_txt);
                    }
                }

                if (GW.cfg.custom_downlink) {
                    /* Customer downlink process */
                    dn_pkt_s* dnelem = NULL;
                    sprintf(tmpstr, "%08X", devinfo.devaddr);
                    dnelem = search_dn_list(tmpstr);
                    if (dnelem != NULL) {
                        lgw_log(LOG_INFO, "INFO~ [cus-dwn]Found a match devaddr: %s, prepare a downlink!\n", tmpstr);
                        jit_result = custom_rx2dn(dnelem, &devinfo, p->count_us, TIMESTAMPED);
                        if (jit_result == JIT_ERROR_OK) { /* Next upmsg willbe indicate if received by note */
                            LGW_LIST_LOCK(&dn_list);
                            LGW_LIST_REMOVE(&dn_list, dnelem, list);
                            lgw_free(dnelem);
                            LGW_LIST_UNLOCK(&dn_list);
                        } else {
                            lgw_log(LOG_ERROR, "ERROR~ [cus-dwn]Packet REJECTED (jit error=%d)\n", jit_result);
                        }

                    } else
                        lgw_log(LOG_INFO, "INFO~ [cus-dwn] Can't find SessionKeys for Dev %08X\n", devinfo.devaddr);
                }
            }
        }
	}
}

static void pkt_prepare_downlink(void* arg) {
    serv_s* serv = (serv_s*) arg;
    lgw_log(LOG_INFO, "INFO~ [%s] Staring pkt_prepare_downlink thread\n", serv->info.name);
    
    int i, j; /* loop variables */

    DIR *dir;
    FILE *fp;
    struct dirent *ptr;
    struct stat statbuf;
    char dn_file[128]; 

    char db_family[32] = {'\0'};

    /* data buffers */
    uint8_t buff_down[512]; /* buffer to receive downstream packets */
    uint8_t dnpld[256];
    uint8_t hexpld[256];
    
    uint32_t uaddr;
    char addr[16];
    char txmode[8];
    char pdformat[8];
    uint8_t psize = 0;

    dn_pkt_s* entry = NULL;

    enum jit_error_e jit_result = JIT_ERROR_OK;

    while (!serv->thread.stop_sig) {
        
        /* lookup file */
        if ((dir = opendir(DNPATH)) == NULL) {
            //lgw_log(LOG_ERROR, "ERROR~ [push]open sending path error\n");
            wait_ms(100); 
            continue;
        }

	    while ((ptr = readdir(dir)) != NULL) {
            if (strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0) /* current dir OR parrent dir */
                continue;

            lgw_log(LOG_INFO, "INFO~ [DNLK]Looking file : %s\n", ptr->d_name);

            snprintf(dn_file, sizeof(dn_file), "%s/%s", DNPATH, ptr->d_name);

            if (stat(dn_file, &statbuf) < 0) {
                lgw_log(LOG_ERROR, "ERROR~ [DNLK]Canot stat %s!\n", ptr->d_name);
                continue;
            }

            if ((statbuf.st_mode & S_IFMT) == S_IFREG) {
                if ((fp = fopen(dn_file, "r")) == NULL) {
                    lgw_log(LOG_ERROR, "ERROR~ [DNLK]Cannot open %s\n", ptr->d_name);
                    continue;
                }

                lgw_memset(buff_down, '\0', sizeof(buff_down));

                psize = fread(buff_down, sizeof(char), sizeof(buff_down), fp); /* the size less than buff_down return EOF */
                fclose(fp);

                unlink(dn_file); /* delete the file */

                memset(addr, '\0', sizeof(addr));
                memset(pdformat, '\0', sizeof(pdformat));
                memset(txmode, '\0', sizeof(txmode));
                lgw_memset(hexpld, '\0', sizeof(hexpld));
                lgw_memset(dnpld, '\0', sizeof(dnpld));

                for (i = 0, j = 0; i < psize; i++) {
                    if (buff_down[i] == ',')
                        j++;
                }

                if (j < 3) { /* Error Format, ',' must be greater than or equal to 3*/
                    lgw_log(LOG_INFO, "INFO~ [DNLK]Format error: %s\n", buff_down);
                    continue;
                }

                for (i = 0, j = 0; i < (int)sizeof(addr); i++) {
                    if (buff_down[i] == ',') { 
                        i++;
                        break;
                    }
                    if (buff_down[i] != ' ')
                        addr[j++] = buff_down[i];
                }

                while (buff_down[i] == ' ' && i < psize) {
                    i++;
                }

                for (j = 0; j < (int)sizeof(txmode); i++) {
                    if (buff_down[i] == ',') {
                        i++;
                        break;
                    }
                    if (buff_down[i] != ' ')
                        txmode[j++] = buff_down[i];
                }

                while (buff_down[i] == ' ' && i < psize) {
                    i++;
                }

                for (j = 0; j < (int)sizeof(pdformat); i++) {
                    if (buff_down[i] == ',') {
                        i++;
                        break;
                    }
                    if (buff_down[i] != ' ')
                        pdformat[j++] = buff_down[i];
                }

                while (buff_down[i] == ' ' && i < psize) {
                    i++;
                }

                for (j = 0; i < psize + 1; i++) {
					if(buff_down[i] != 0 && buff_down[i] != 10 )
                        dnpld[j++] = buff_down[i];
                }

                psize = j;

                if ((strlen(addr) < 1) || (psize < 1))
                    continue;
                
                if (strlen(txmode) < 1)
                    strcpy(txmode, "time");

                if (strlen(pdformat) < 1)
                    strcpy(pdformat, "txt");

                entry = (dn_pkt_s*) lgw_malloc(sizeof(dn_pkt_s));
                strcpy(entry->devaddr, addr);

                if (strstr(pdformat, "hex") != NULL) { 
                    if (psize % 2) {
                        lgw_log(LOG_INFO, "INFO~ [DNLK] Size of hex payload invalid.\n");
                        lgw_free(entry);
                        continue;
                    }
                    hex2str(dnpld, hexpld, psize);
                    psize = psize/2;
                    lgw_memcpy(entry->payload, hexpld, psize + 1);
                } else
                    lgw_memcpy(entry->payload, dnpld, psize + 1);

                strcpy(entry->txmode, txmode);
                strcpy(entry->pdformat, pdformat);
                entry->psize = psize;
				
                lgw_log(LOG_DEBUG, "DEBUG~ [DNLK]devaddr:%s, txmode:%s, pdfm:%s, size:%d\n",
                                  entry->devaddr, entry->txmode, entry->pdformat, entry->psize);

                if (strstr(entry->txmode, "imme") != NULL) {
                    lgw_log(LOG_INFO, "INFO~ [DNLK] Pending IMMEDIATE downlink for %s\n", addr);

                    uaddr = strtoul(addr, NULL, 16);

                    devinfo_s devinfo = { .devaddr = uaddr };

                    sprintf(db_family, "devinfo/%08X", devinfo.devaddr);

                    if ((lgw_db_get(db_family, "appskey", devinfo.appskey_str, sizeof(devinfo.appskey_str)) == -1) || 
                        (lgw_db_get(db_family, "nwkskey", devinfo.devaddr_str, sizeof(devinfo.nwkskey_str)) == -1)) {
                        lgw_free(entry);
                        continue;
                    }

                    str2hex(devinfo.appskey, devinfo.appskey_str, sizeof(devinfo.appskey));
                    str2hex(devinfo.nwkskey, devinfo.nwkskey_str, sizeof(devinfo.nwkskey));

                    jit_result = custom_rx2dn(entry, &devinfo, 0, IMMEDIATE);

                    if (jit_result != JIT_ERROR_OK)  
                        lgw_log(LOG_ERROR, "ERROR~ [DNLK]Packet REJECTED (jit error=%d)\n", jit_result);
                    else
                        lgw_log(LOG_INFO, "INFO~ [DNLK]No devaddr match, Drop the link of %s\n", addr);

                    lgw_free(entry);
                    continue;
                }

                LGW_LIST_LOCK(&dn_list);
                LGW_LIST_INSERT_TAIL(&dn_list, entry, list);
                LGW_LIST_UNLOCK(&dn_list);

                if (dn_list.size > 16) {   // 当下发链里的包数目大于16时，删除一部分下发包，节省内存
                    LGW_LIST_LOCK(&dn_list);
                    LGW_LIST_TRAVERSE_SAFE_BEGIN(&dn_list, entry, list) { // entry重新赋值
                        if (dn_list.size < 8)
                            break;
                        LGW_LIST_REMOVE_CURRENT(list);
                        dn_list.size--;
                        lgw_free(entry);
                    }
                    LGW_LIST_TRAVERSE_SAFE_END;
                    LGW_LIST_UNLOCK(&dn_list);
                }
            }
            wait_ms(20); /* wait for HAT send or other process */
        }
        if (closedir(dir) < 0)
            lgw_log(LOG_INFO, "INFO~ [DNLK] Cannot close DIR: %s\n", DNPATH);
        wait_ms(100);
    }
}

