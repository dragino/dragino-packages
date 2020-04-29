
#ifndef _CMAC_H_
#define _CMAC_H_

#include <stdint.h>
#include "aes.h"
  
#define AES_CMAC_KEY_LENGTH     16
#define AES_CMAC_DIGEST_LENGTH  16
 
typedef struct _AES_CMAC_CTX {
            aes_context    rijndael;
            uint8_t        X[16];
            uint8_t        M_last[16];
            uint32_t       M_n;
    } AES_CMAC_CTX;
   
//#include <sys/cdefs.h>
    
//__BEGIN_DECLS
void     AES_CMAC_Init(AES_CMAC_CTX * ctx);
void     AES_CMAC_SetKey(AES_CMAC_CTX * ctx, const uint8_t key[AES_CMAC_KEY_LENGTH]);
void     AES_CMAC_Update(AES_CMAC_CTX * ctx, const uint8_t * data, uint32_t len);
          //          __attribute__((__bounded__(__string__,2,3)));
void     AES_CMAC_Final(uint8_t digest[AES_CMAC_DIGEST_LENGTH], AES_CMAC_CTX  * ctx);
            //     __attribute__((__bounded__(__minbytes__,1,AES_CMAC_DIGEST_LENGTH)));
//__END_DECLS

#endif /* _CMAC_H_ */

