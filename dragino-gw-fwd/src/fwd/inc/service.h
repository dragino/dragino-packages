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
 * \brief Persistant data storage 
 */

#ifndef _SERVICE_H
#define _SERVICE_H

#include "gwcfg.h"

/*!
 * \brief rxpkt分发处理
 * 
 */
//void service_handle_rxpkt(rxpkts_s* rxpkt);

/*!
 * \brief rxpkt分发处理
 * 
 */
void service_start();

/*!
 * \brief rxpkt分发处理
 * 
 */
void service_stop();

/*!
 * \brief 准备一个网络文件描述符，用于后续的连接
 * \param timeout 是一个连接的超时时间，用来中断连续，避免长时间阻塞
 */
int init_sock(const char* addr, const char* port, const void* timeout, int size);

/*!
 * \brief pkt包的简单过滤
 * \param addr 设备的网络地址 
 * \param fport MACfport过滤
 * \ret false过滤包， true不过滤
 */
bool pkt_basic_filter(serv_s* serv, const uint32_t addr, const uint8_t fport);

/*!
 */
int parse_cfg(const char* cfgfile);

/*!
 */
uint16_t crc16(const uint8_t * data, unsigned size);

#endif							
