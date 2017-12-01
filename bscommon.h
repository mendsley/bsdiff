#ifndef BS_COMMON_H
#define BS_COMMON_H

# include <stdint.h>

#define HEADER_TXT ("ENDSLEY/BSDIFF43")
typedef struct BSHeader{
	char header_txt[sizeof(HEADER_TXT)];
	uint8_t new_size[8];
}BSHeader;

#endif