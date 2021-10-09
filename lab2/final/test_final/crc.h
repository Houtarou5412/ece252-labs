/**
 * @file: crc.h
 * @brief: crc calculation functions for PNG file
 */

 //LAB2 Brian and Jason

#pragma once

void make_crc_table(void);
unsigned long update_crc(unsigned long crc, unsigned char *buf, int len);
unsigned long crc(unsigned char *buf, int len);
