/*
 * cli.c
 *
 *  Created on: 17 Jan 2026
 *      Author: Kevin Beach
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "cli.h"
#include "uart.h"
#include "camera_initialization.h"
#include "i2c_master.h"
#include "i2c_master_regs.h"
#include "lsc_ccm_reg.h"     // GET_CCM_COEFF_INT()
#include "lsc_reg_access.h"   // lsc_32_write(), lsc_32_read()
#include "sys_platform.h"     // provides AHBL_TO_AXI_LITE_BRIDGE_BASE_ADDR
#include "version_info.h"

static char line_buf[CLI_MAX_LINE];
static uint8_t line_len = 0;

/* ---------------------- NEW ---------------------- */
static uint8_t cli_verbose = 1;  // 1=ON, 0=OFF
/* ------------------------------------------------- */

extern struct i2cm_instance i2c_master_core;
extern struct uart_instance uart_core_uart;

/* --------------------------------------------------------------------------
 * Sensor (I2C) helpers
 * -------------------------------------------------------------------------- */

uint8_t sensor_read(uint16_t reg)
{
    uint8_t data;
    uint8_t tx[2];

    tx[0] = (reg >> 8) & 0xFF;
    tx[1] = reg & 0xFF;

    i2c_master_repeated_start(&i2c_master_core,
                              TRGT_SLV_ADDR,
                              2, tx,
                              1, &data);
    return data;
}

void sensor_write(uint16_t reg, uint8_t val)
{
    uint8_t tx[3];

    tx[0] = (reg >> 8) & 0xFF;
    tx[1] = reg & 0xFF;
    tx[2] = val;

    i2c_master_write(&i2c_master_core,
                     TRGT_SLV_ADDR,
                     3, tx);
}

/* --------------------------------------------------------------------------
 * Command processor
 * -------------------------------------------------------------------------- */

void process_command(const char *cmd)
{
    char type, rw;

    /* Temporary parse buffers */
    char tmp[5];
    char tmp_base[9];
    char tmp_off[3];
    char tmp_data[9];

    tmp[4]      = '\0';
    tmp_base[8] = '\0';
    tmp_off[2]  = '\0';
    tmp_data[8] = '\0';

    if (strlen(cmd) == 0)
        return;

    /* ----------------- Verbose command ----------------- */
    if (cmd[0] == 'V') {
        char tmp_arg[4];
        if (sscanf(cmd, "V %3s", tmp_arg) == 1) {
            if (strcasecmp(tmp_arg,"ON") == 0)
                cli_verbose = 1;
            else if (strcasecmp(tmp_arg,"OFF") == 0)
                cli_verbose = 0;
            else {
                if (cli_verbose)
                    printf("ERR: Unknown V argument '%s'\r\n", tmp_arg);
                return;
            }

            if (cli_verbose)
                printf("Verbose mode %s\r\n", cli_verbose ? "ON":"OFF");
        } else {
            if (cli_verbose)
                printf("ERR: V command requires ON or OFF\r\n");
        }
        return;
    }

    /* ------------------- HELP ------------------- */
    if (strcmp(cmd, "HELP") == 0) {
        if (cli_verbose) {
            printf (" \r\n Version number : %s \r\n",VERSION_NO);
            printf (" Version Resolution : %s \r\n",VERSION_RESOLUTION);
            printf (" %s \r\n", VERSION_INFO);
            printf (" Github : %s \r\n",VERSION_GITHUB);
            printf("Available commands:\r\n");
            printf("  S R <addr> <data>        - Read sensor register\r\n");
            printf("  S W <addr> <data>        - Write sensor register\r\n");
            printf("  F R <base> <offset>      - Read FPGA AXI register\r\n");
            printf("  F W <base> <offset> <d>  - Write FPGA AXI register\r\n");
            printf("  C R <offset>      - Read CCM coefficient\r\n");
            printf("  C W <offset> <i>  - Write CCM coefficient (-99..99)\r\n");
            printf("  C U                      - Update and enable new CCM configuration");
            printf("    addr = base + (offset << 2)\r\n");
        }
        return;
    }

    /* Parse command type/rw */
    if (sscanf(cmd, " %c %c", &type, &rw) != 2) {
        if (cli_verbose)
            printf("ERR: Cannot parse command\r\n");
        return;
    }

    /* ---------------- Sensor commands (S) ---------------- */
    if (type == 'S') {
        uint16_t reg;
        uint8_t  val;

        if (sscanf(cmd, "%*c %*c %4s", tmp) != 1) {
            if (cli_verbose)
                printf("ERR: Cannot parse sensor address\r\n");
            return;
        }

        reg = strtoul(tmp, NULL, 16);

        if (rw == 'R') {
            val = sensor_read(reg);
            if (cli_verbose)
                printf("Read: 0x%04X = 0x%02X\r\n", reg, val);
            else
                printf("0x%02X\r\n", val);
        }
        else if (rw == 'W') {
            if (sscanf(cmd, "%*c %*c %*4s %2s", tmp) != 1) {
                if (cli_verbose)
                    printf("ERR: Cannot parse sensor data\r\n");
                return;
            }

            val = strtoul(tmp, NULL, 16);
            sensor_write(reg, val);

            if (cli_verbose) {
                uint8_t verify = sensor_read(reg);
                printf("Wrote: 0x%02X to 0x%04X, verified = 0x%02X\r\n",
                       val, reg, verify);
            }
        }
        else {
            if (cli_verbose)
                printf("ERR: Unknown S operation '%c'\r\n", rw);
        }
    }

    /* ---------------- FPGA AXI commands (F) ---------------- */
    else if (type == 'F') {
        uint32_t base, addr, data;
        uint8_t  offset;

        if (rw == 'W') {
            if (sscanf(cmd, "%*c %*c %8s %2s %8s",
                       tmp_base, tmp_off, tmp_data) != 3) {
                if (cli_verbose)
                    printf("ERR: Cannot parse F W command\r\n");
                return;
            }

            base   = strtoul(tmp_base, NULL, 16);
            offset = strtoul(tmp_off,  NULL, 16);
            data   = strtoul(tmp_data, NULL, 16);
            addr   = base + ((uint32_t)offset << 2);

            lsc_32_write(addr, data);

            if (cli_verbose) {
                uint32_t verify = lsc_32_read(addr);
                printf("F W: [0x%08lX] <= 0x%08lX (rb 0x%08lX)\r\n",
                       addr, data, verify);
            }
        }
        else if (rw == 'R') {
            if (sscanf(cmd, "%*c %*c %8s %2s",
                       tmp_base, tmp_off) != 2) {
                if (cli_verbose)
                    printf("ERR: Cannot parse F R command\r\n");
                return;
            }

            base   = strtoul(tmp_base, NULL, 16);
            offset = strtoul(tmp_off,  NULL, 16);
            addr   = base + ((uint32_t)offset << 2);

            data = lsc_32_read(addr);

            if (cli_verbose)
                printf("F R: [0x%08lX] = 0x%08lX\r\n", addr, data);
            else
                printf("0x%08lX\r\n", data);
        }
        else {
            if (cli_verbose)
                printf("ERR: Unknown F operation '%c'\r\n", rw);
        }
    }

    /* ---------------- CCM commands (C) ---------------- */
    else if (type == 'C') {
        uint32_t addr;
        uint8_t  offset;
        int32_t  coeff_int;
        int32_t  coeff_hw;

        if (rw == 'W') {
            if (sscanf(cmd, "%*c %*c %2s %8s", tmp_off, tmp_data) != 2) {
                if (cli_verbose)
                    printf("ERR: Cannot parse C W command\r\n");
                return;
            }

            offset    = strtoul(tmp_off,  NULL, 16);
            coeff_int = strtol(tmp_data,  NULL, 10);

            if (coeff_int < -99 || coeff_int > 99) {
                if (cli_verbose)
                    printf("ERR: CCM coefficient out of range (-99..99)\r\n");
                return;
            }

            coeff_hw = GET_CCM_COEFF_INT(coeff_int);
            addr     = AHBL_TO_AXI_LITE_BRIDGE_BASE_ADDR + ((uint32_t)offset << 2);

            lsc_32_write(addr, (uint32_t)coeff_hw);

            if (cli_verbose) {
                uint32_t verify = lsc_32_read(addr);
                printf("C W: [0x%08lX] <= %ld (hw 0x%08lX, rb 0x%08lX)\r\n",
                       addr, coeff_int, (uint32_t)coeff_hw, verify);
            }
        }
        else if (rw == 'R') {
            if (sscanf(cmd, "%*c %*c %2s", tmp_off) != 1) {
                if (cli_verbose)
                    printf("ERR: Cannot parse C R command\r\n");
                return;
            }

            offset = strtoul(tmp_off,  NULL, 16);
            addr   = AHBL_TO_AXI_LITE_BRIDGE_BASE_ADDR + ((uint32_t)offset << 2);

            coeff_hw  = (int32_t)lsc_32_read(addr);
            coeff_int = (coeff_hw * 100) / 32768;

            if (cli_verbose)
                printf("C R: [0x%08lX] = %ld (hw 0x%08lX)\r\n",
                       addr, coeff_int, (int16_t)coeff_hw);
            else
                printf("%ld\r\n", coeff_int);
        }
        else if (rw == 'U'){
            addr = AHBL_TO_AXI_LITE_BRIDGE_BASE_ADDR + CONFIGURATION_UPDATE;
            lsc_32_write(addr, 0x01); // enact changes
            if (cli_verbose)
                printf("CCM Update triggered\r\n");
        }
        else {
            if (cli_verbose)
                printf("ERR: Unknown C operation '%c'\r\n", rw);
        }
    }

    /* ---------------- T commands (translate matrix) ---------------- */
    else if (type == 'T') {
        uint32_t addr;
        uint16_t offset;
        int32_t  data_int;
        int16_t  data_hw;

        if (rw == 'W') {
            if (sscanf(cmd, "%*c %*c %4s %8s",
                       tmp_off, tmp_data) != 2) {
                if (cli_verbose)
                    printf("ERR: Cannot parse T W command\r\n");
                return;
            }

            offset   = (uint16_t)strtoul(tmp_off, NULL, 16);
            data_int = strtol(tmp_data, NULL, 10);

            if (data_int < -255 || data_int > 255) {
                if (cli_verbose)
                    printf("ERR: T data out of range (-255..255)\r\n");
                return;
            }

            data_hw = (int16_t)data_int;
            addr = AHBL_TO_AXI_LITE_BRIDGE_BASE_ADDR + ((uint32_t)offset << 2);
            lsc_32_write(addr, (uint32_t)(uint16_t)data_hw);

            if (cli_verbose) {
                uint32_t verify = lsc_32_read(addr);
                printf("T W: [0x%08lX] <= %ld (hw 0x%04X, rb 0x%08lX)\r\n",
                       addr, data_int, (uint16_t)data_hw, verify);
            }
        }
        else if (rw == 'R') {
            if (sscanf(cmd, "%*c %*c %4s", tmp_off) != 1) {
                if (cli_verbose)
                    printf("ERR: Cannot parse T R command\r\n");
                return;
            }

            offset = (uint16_t)strtoul(tmp_off, NULL, 16);
            addr = AHBL_TO_AXI_LITE_BRIDGE_BASE_ADDR + ((uint32_t)offset << 2);

            data_hw  = (int16_t)lsc_32_read(addr);
            data_int = (int32_t)data_hw;

            if (cli_verbose)
                printf("T R: [0x%08lX] = %ld (hw 0x%04X)\r\n",
                       addr, data_int, (uint16_t)data_hw);
            else
                printf("%ld\r\n", data_int);
        }
        else {
            if (cli_verbose)
                printf("ERR: Unknown T operation '%c'\r\n", rw);
        }
    }

    /* ---------------- Unknown command ---------------- */
    else {
        if (cli_verbose)
            printf("ERR: Unknown command type '%c'\r\n", type);
    }
}

/* --------------------------------------------------------------------------
 * UART RX handler
 * -------------------------------------------------------------------------- */

void cli_rx_char(char c)
{
    if (c == '\r' || c == '\n') {
        if (line_len > 0) {
            line_buf[line_len] = '\0';
            process_command(line_buf);
            line_len = 0;
        }
        printf("\r\n> ");
        return;
    }

    if (c == '\b' || c == 0x7F) {
        if (line_len > 0) {
            line_len--;
            printf("\b \b");
        }
        return;
    }

    if (line_len < CLI_MAX_LINE - 1) {
        line_buf[line_len++] = c;
        if (cli_verbose)
            uart_putc(&uart_core_uart, c);
    }
}
