/*
 *  ____  ____      _    ____ ___ _   _  ___  
 *  |  _ \|  _ \    / \  / ___|_ _| \ | |/ _ \ 
 *  | | | | |_) |  / _ \| |  _ | ||  \| | | | |
 *  | |_| |  _ <  / ___ \ |_| || || |\  | |_| |
 *  |____/|_| \_\/_/   \_\____|___|_| \_|\___/ 
 *
 * Dragino Forward -- An opensource lora gateway forward 
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
 * \brief FWD main include file . File version handling , generic functions.
 */

#ifndef _LGW_LOGGER_H
#define _LGW_LOGGER_H

#include <stdint.h>
#include <stdio.h>

uint8_t LOG_PKT = 0;
uint8_t LOG_REPORT = 0;
uint8_t LOG_JIT = 0;
uint8_t LOG_JIT_ERROR = 0;
uint8_t LOG_BEACON = 0;
uint8_t LOG_INFO = 0;
uint8_t LOG_WARNING = 0;
uint8_t LOG_ERROR = 0;
uint8_t LOG_DEBUG = 0;

#define MSG(args...) printf(args) /* message that is destined to the user */

#define lgw_msg(args...) printf(args) /* message that is destined to the user */

#define lgw_log(FLAG, fmt, ...)                               \
            do  {                                             \
                if (FLAG)                                     \
                    fprintf(stdout, fmt, ##__VA_ARGS__);      \
                } while (0)

#define MSG_DEBUG(FLAG, fmt, ...)                               \
                do  {                                           \
                    if (FLAG)                                   \
                        fprintf(stdout, fmt, ##__VA_ARGS__);    \
                    } while (0)

#endif /* _LGW_LOGGER_H */
