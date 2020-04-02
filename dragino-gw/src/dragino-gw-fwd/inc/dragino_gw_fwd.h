/*
 * DR_PKT_FWD.h
 *
 *  Created on: Aug 28, 2015
 *      Author: ruud
 */

#ifndef _DR_PKT_FWD_H_
#define _DR_PKT_FWD_H_

/* -------------------------------------------------------------------------- */
/* --- MAC OSX Extensions  -------------------------------------------------- */

struct timespec;

double difftimespec(struct timespec end, struct timespec beginning);

#define MAX_SERVERS     4
#define NB_PKT_MAX      8		/* max number of packets per fetch/send cycle */

#define MIN_LORA_PREAMB 6		/* minimum Lora preamble length for this application */
#define STD_LORA_PREAMB 8
#define MIN_FSK_PREAMB  3		/* minimum FSK preamble length for this application */
#define STD_FSK_PREAMB  4

#define PATH_LEN        64      /* no use PATH_MAX */


#endif							/* _DR_PKT_FWD_H_ */
