/*
 * cli.h
 *
 *  Created on: 17 Jan 2026
 *      Author: Kevin Beach
 */

#ifndef CLI_H_
#define CLI_H_

#include <stdint.h>

#define CLI_MAX_LINE 64

// CLI interface
void cli_rx_char(char c);
void process_command(const char *cmd);

// Receive a single character from UART and handle the CLI
void cli_rx_char(char c);

// Sensor read/write helpers
uint8_t sensor_read(uint16_t reg);
void sensor_write(uint16_t reg, uint8_t val);

#endif /* CLI_H_ */
