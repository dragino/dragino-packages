/*
LoRa concentrator : Packet Forwarder trace helpers

License: Revised BSD License, see LICENSE.TXT file include in the project
*/


#ifndef _LORA_PKTFWD_TRACE_H
#define _LORA_PKTFWD_TRACE_H

#include <stdint.h>

extern uint8_t DEBUG_PKT_FWD;   
extern uint8_t DEBUG_MAC_HEAD;
extern uint8_t DEBUG_JIT;       
extern uint8_t DEBUG_JIT_ERROR; 
extern uint8_t DEBUG_TIMERSYNC; 
extern uint8_t DEBUG_BEACON;    
extern uint8_t DEBUG_INFO;      
extern uint8_t DEBUG_WARNING;   
extern uint8_t DEBUG_ERROR;     

/*
typedef union t_loginfo_level {
    unit16_t level_value;
    struct level_bits {
        uint8_t DEBUG_PKT_FWD       : 1 ;   
        uint8_t DEBUG_MAC_HEAD      : 1 ; 
        uint8_t DEBUG_JIT           : 1 ;
        uint8_t DEBUG_JIT_ERROR     : 1 ; 
        uint8_t DEBUG_TIMERSYNC     : 1 ; 
        uint8_t DEBUG_BEACON        : 1 ; 
        uint8_t DEBUG_INFO          : 1 ; 
        uint8_t DEBUG_WARNING       : 1 ; 
        uint8_t DEBUG_ERROR         : 1 ; 
        uint8_t RFU                 : 7 ;
    }bits;
}loginfo_level_t; 
*/

#define MSG(args...) printf(args) /* message that is destined to the user */
#define MSG_DEBUG(FLAG, fmt, ...)                                                                         \
            do  {                                                                                         \
                if (FLAG)                                                                                 \
                    fprintf(stdout, fmt, ##__VA_ARGS__); \
            } while (0)

#define MSG_INFO(FLAG, fmt, ...)                                                                         \
            do  {                                                                                         \
                if (FLAG)                                                                                 \
                    fprintf(stdout, fmt, ##__VA_ARGS__); \
            } while (0)



#endif
/* --- EOF ------------------------------------------------------------------ */
