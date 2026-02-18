#include "gamma.h"
#include "sys_platform.h"


/* External HAL function */
extern uint8_t lsc_32_write(uint32_t reg_addr, uint32_t value);

/* Hardware-specific base address */
#define GAMMA_BASE_ADDR   MISC_IMAGE_CORRECTION_APB_INST_BASE_ADDR
#define GAMMA_TABLE_BASE_OFFSET 0x300
#define GAMMA_STRIDE      4U    /* 32-bit registers */

/*
 * Gamma tables stored in flash.
 * Replace example values with real gamma data.
 */
static const uint32_t gamma_table[GAMMA_SETS][GAMMA_POINTS] =
{
    /* ------------------------------------------------- */
    /* Gamma Set 0 : all_zero_gamma.mem (64 words)      */
    /* ------------------------------------------------- */
    {
        /* 64 entries */
        0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U,
        0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U,
        0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U,
        0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U,
        0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U,
        0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U,
        0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U,
        0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U,

        0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U,
        0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U,
        0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U,
        0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U,
        0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U,
        0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U,
        0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U,
        0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U
    },

    /* ------------------------------------------------- */
    /* Gamma Set 1 : filmic_gamma.mem (first 64 words)        */
    /* ------------------------------------------------- */
    {
        0x03020100U, 0x07060504U, 0x0B0A0908U, 0x100E0D0CU,
        0x14131211U, 0x19181716U, 0x1E1D1C1BU, 0x24222120U,
        0x29272625U, 0x2E2D2B2AU, 0x3332312FU, 0x38373635U,
        0x3E3C3B3AU, 0x4342403FU, 0x48474644U, 0x4D4C4B49U,
        0x5251504EU, 0x57565554U, 0x5C5B5A59U, 0x61605F5EU,
        0x66656462U, 0x6B6A6867U, 0x706E6D6CU, 0x74737271U,
        0x79787675U, 0x7D7C7B7AU, 0x82817F7EU, 0x86858483U,
        0x8A898887U, 0x8F8E8D8BU, 0x93929190U, 0x97969594U,
        0x9B9A9998U, 0x9F9E9D9CU, 0xA3A2A1A0U, 0xA7A6A5A4U,
        0xABAAA9A8U, 0xAFAEADACU, 0xB2B1B0AFU, 0xB6B5B4B3U,
        0xB9B9B8B7U, 0xBDBCBBBAU, 0xC0C0BFBEU, 0xC4C3C2C1U,
        0xC7C6C6C5U, 0xCBCAC9C8U, 0xCECDCCCBU, 0xD1D0D0CFU,
        0xD4D4D3D2U, 0xD7D7D6D5U, 0xDBDAD9D8U, 0xDEDDDCDBU,
        0xE1E0DFDEU, 0xE4E3E2E1U, 0xE7E6E5E4U, 0xE9E9E8E7U,
        0xECECEBEAU, 0xEFEEEEEDU, 0xF2F1F0F0U, 0xF5F4F3F2U,
        0xF7F7F6F5U, 0xFAF9F9F8U, 0xFCFCFBFAU, 0xFFFEFEFDU
    },
	/* Gamma Set 2 : sRGB Gamma */
    {
    		0x1C160D00,0x2E2A2622,0x3B383532,0x4542403D,0x4D4B4947,0x5553514F,0x5C5A5856,0x62605F5D,
    		0x68666563,0x6D6C6A69,0x7271706E,0x77767573,0x7C7A7978,0x807F7E7D,0x84838281,0x88878685,
    		0x8C8B8A89,0x908F8E8D,0x94939291,0x97969594,0x9B9A9998,0x9E9D9C9B,0xA1A09F9F,0xA4A3A3A2,
    		0xA7A7A6A5,0xAAAAA9A8,0xADADACAB,0xB0AFAFAE,0xB3B2B2B1,0xB6B5B4B4,0xB9B8B7B6,0xBBBBBAB9,
    		0xBEBDBDBC,0xC0C0BFBE,0xC3C2C2C1,0xC5C5C4C4,0xC8C7C7C6,0xCACAC9C8,0xCDCCCBCB,0xCFCECECD,
    		0xD1D1D0D0,0xD4D3D2D2,0xD6D5D5D4,0xD8D7D7D6,0xDADAD9D8,0xDCDCDBDB,0xDEDEDDDD,0xE0E0DFDF,
    		0xE3E2E2E1,0xE5E4E4E3,0xE7E6E6E5,0xE9E8E8E7,0xEBEAEAE9,0xEDECECEB,0xEEEEEEED,0xF0F0EFEF,
    		0xF2F2F1F1,0xF4F4F3F3,0xF6F6F5F5,0xF8F7F7F6,0xFAF9F9F8,0xFBFBFBFA,0xFDFDFCFC,0xFFFFFEFE
    }
};


/*
 * Load selected gamma table into ISP
 */
uint8_t gamma_load(uint8_t set)
{
    uint32_t addr;
    uint32_t i;
    uint8_t status;

    if (set >= GAMMA_SETS)
    {
        return 1U;  /* Invalid selection */
    }

    addr = GAMMA_BASE_ADDR + GAMMA_TABLE_BASE_OFFSET;

    for (i = 0U; i < GAMMA_POINTS; i++)
    {
        status = lsc_32_write(addr, gamma_table[set][i]);

        if (status != 0U)
        {
            return status;  /* Propagate HAL error */
        }

        addr += GAMMA_STRIDE;
        /* Alternatively:
           addr = GAMMA_BASE_ADDR + (i << 2);
        */
    }

    return 0U;  /* Success */
}
