/*
 * lsc_reg_access.h
 *
 *  Created on: 24 Jan 2026
 *      Author: Kevin Beach
 */

#ifndef LSC_REG_ACCESS_H_
#define LSC_REG_ACCESS_H_

#include <stdint.h>

void     lsc_32_write(uint32_t addr, uint32_t data);
uint32_t lsc_32_read (uint32_t addr);

#endif

