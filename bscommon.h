#ifndef BS_COMMON_H
#define BS_COMMON_H

# include <stdint.h>

#define HEADER_TXT ("ENDSLEY/BSDIFF44")
typedef struct BSHeader{
	char header_txt[sizeof(HEADER_TXT)];
	uint8_t new_size[8];
    uint8_t old_size[8];
    uint8_t new_chksum[2];
    uint8_t old_chksum[2];
}BSHeader;

#endif