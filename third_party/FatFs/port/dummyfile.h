/*
 * dummyfile.h
 *
 *  Created on: 2024年3月5日
 *      Author: ch.wang
 */

#ifndef DUMMYFILE_H_
#define DUMMYFILE_H_

#define DUMMY_TEXT_LEN (4*1024)
#define DUMMY_WRITE_LEN (2*1024*1024)
#define DUMMY_ITERATION (DUMMY_WRITE_LEN/DUMMY_TEXT_LEN)
#define DUMMY_TEXT_BASE (0x3B)
#define DUMMY_TEXT_BOUND (0x40) // 3B(;)(0011 1011) ~ 7A(z)(01111010)

uint8_t dummy_text[DUMMY_TEXT_LEN];

uint8_t* init_dummy_text(void);



#if 0
const unsigned int dummy_file_len = 16*10;
// array size
const uint8_t dummy_file[] /*__attribute__ ((section(".eh_fram")))*/ = {
    0x53, 0x69, 0x57, 0x47, 0x39, 0x31, 0x37, 0x20, 0x53, 0x6f, 0x43, 0x20, 0x53, 0x69, 0x6e, 0x67,
      0x6c, 0x65, 0x20, 0x43, 0x68, 0x69, 0x70, 0x20, 0x57, 0x69, 0x2d, 0x46, 0x69, 0xc2, 0xae, 0x20,
      0x61, 0x6e, 0x64, 0x0a, 0x42, 0x6c, 0x75, 0x65, 0x74, 0x6f, 0x6f, 0x74, 0x68, 0xc2, 0xae, 0x20,
      0x4c, 0x45, 0x20, 0x57, 0x69, 0x72, 0x65, 0x6c, 0x65, 0x73, 0x73, 0x20, 0x53, 0x65, 0x63, 0x75,
      0x72, 0x65, 0x20, 0x4d, 0x43, 0x55, 0x0a, 0x53, 0x6f, 0x6c, 0x75, 0x74, 0x69, 0x6f, 0x6e, 0x73,
      0x0a, 0x0a, 0x20, 0x53, 0x69, 0x6c, 0x69, 0x63, 0x6f, 0x6e, 0x20, 0x4c, 0x61, 0x62, 0x73, 0x20,
      0x53, 0x69, 0x57, 0x47, 0x39, 0x31, 0x37, 0x20, 0x53, 0x6f, 0x43, 0x20, 0x69, 0x73, 0x20, 0x6f,
      0x75, 0x72, 0x20, 0x6c, 0x6f, 0x77, 0x65, 0x73, 0x74, 0x20, 0x70, 0x6f, 0x77, 0x65, 0x72, 0x20,
      0x57, 0x69, 0x2d, 0x46, 0x69, 0x20, 0x36, 0x20, 0x70, 0x6c, 0x75, 0x73, 0x20, 0x42, 0x6c, 0x75,
      0x65, 0x74, 0x6f, 0x6f, 0x74, 0x68, 0x20, 0x4c, 0x45, 0x20, 0x35, 0x2e, 0x34, 0x20, 0x53, 0x6f,
};
#endif
#endif /* DUMMYFILE_H_ */
