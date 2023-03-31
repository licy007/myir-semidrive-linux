/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#ifndef _CE_REG_H
#define _CE_REG_H

#include "__regs_ap_cryptoengine.h"

//--------------------------------------------------------------------------
// IP Ref Info     : REG_AP_APB_CRYPTOENGINE
// RTL version     :
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_STAT
// Register Offset : 0x0
// Description     :
//--------------------------------------------------------------------------
#define APB_CE1_BASE 0
#if (CE_SOC_BASE_ADDR == APB_CE1_BASE)
//use ce1
#define CE_JUMP                     0x0
#else
//#define CE_JUMP                     0x1000
#define CE_JUMP                     0x0
#endif

#define REG_STAT_CE_(i)             (REG_AP_APB_CRYPTOENGINE_CE0_STAT + CE_JUMP * i)
#define PKE_BUSY_SHIFT              CE0_STAT_PKE_BUSY_FIELD_OFFSET
#define PKE_BUSY_MASK               1UL << PKE_BUSY_SHIFT
#define GENKEY_BUSY_SHIFT           CE0_STAT_GENKEY_BUSY_FIELD_OFFSET
#define GENKEY_BUSY_MASK            1UL << GENKEY_BUSY_SHIFT
#define HASH_BUSY_SHIFT             CE0_STAT_HASH_BUSY_FIELD_OFFSET
#define HASH_BUSY_MASK              1UL << HASH_BUSY_SHIFT
#define CIPHER_BUSY_SHIFT           CE0_STAT_CIPHER_BUSY_FIELD_OFFSET
#define CIPHER_BUSY_MASK            1UL << CIPHER_BUSY_SHIFT
#define GETRNDNUM_BUSY_SHIFT        CE0_STAT_GETRNDNUM_BUSY_FIELD_OFFSET
#define GETRNDNUM_BUSY_MASK         1UL << GETRNDNUM_BUSY_SHIFT
#define DMA_BUSY_SHIFT              CE0_STAT_DMA_BUSY_FIELD_OFFSET
#define DMA_BUSY_MASK               1UL << DMA_BUSY_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_ERRSTAT
// Register Offset : 0x4
// Description     :
//--------------------------------------------------------------------------
#define REG_ERRSTAT_CE_(i)      (REG_AP_APB_CRYPTOENGINE_CE0_ERRSTAT + CE_JUMP * i)
#define SKC_ERR_STAT_SHIFT      CE0_ERRSTAT_SKC_ERR_STAT_FIELD_OFFSET
#define SKC_ERR_STAT_MASK       0xF << SKC_ERR_STAT_SHIFT
#define SSRAM_ERR_STAT_SHIFT    CE0_ERRSTAT_SSRAM_ERR_STAT_FIELD_OFFSET
#define SSRAM_ERR_STAT_MASK     1UL << SSRAM_ERR_STAT_SHIFT
#define DMA_ERR_STAT_SHIFT      CE0_ERRSTAT_DMA_ERR_STAT_FIELD_OFFSET
#define DMA_ERR_STAT_MASK       7UL << DMA_ERR_STAT_SHIFT
#define CIPHER_ERR_STAT_SHIFT   CE0_ERRSTAT_CIPHER_ERR_STAT_FIELD_OFFSET
#define CIPHER_ERR_STAT_MASK    0x3F << CIPHER_ERR_STAT_SHIFT
#define HASH_ERR_STAT_SHIFT     CE0_ERRSTAT_HASH_ERR_STAT_FIELD_OFFSET
#define HASH_ERR_STAT_MASK      0xF << HASH_ERR_STAT_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_CIPHER_CTRL
// Register Offset : 0xc
// Description     :
//--------------------------------------------------------------------------
#define REG_CIPHER_CTRL_CE_(i)              (REG_AP_APB_CRYPTOENGINE_CE0_CIPHER_CTRL + CE_JUMP * i)
#define CE_CIPHER_CTRL_HEADER_SAVE_SHIFT    CE0_CIPHER_CTRL_CIPHER_HEADER_SAVE_FIELD_OFFSET
#define CE_CIPHER_CTRL_HEADER_SAVE_MASK     1UL << CE_CIPHER_CTRL_HEADER_SAVE_SHIFT
#define CE_CIPHER_CTRL_KEY2TYPE_SHIFT       CE0_CIPHER_CTRL_CIPHER_KEY2TYPE_FIELD_OFFSET
#define CE_CIPHER_CTRL_KEY2TYPE_MASK        3UL << CE_CIPHER_CTRL_KEY2TYPE_SHIFT
#define CE_CIPHER_CTRL_KEYTYPE_SHIFT        CE0_CIPHER_CTRL_CIPHER_KEYTYPE_FIELD_OFFSET
#define CE_CIPHER_CTRL_KEYTYPE_MASK         3UL << CE_CIPHER_CTRL_KEYTYPE_SHIFT
#define CE_CIPHER_CTRL_AESMODE_SHIFT        CE0_CIPHER_CTRL_AESMODE_FIELD_OFFSET
#define CE_CIPHER_CTRL_AESMODE_MASK         0x1FF << CE_CIPHER_CTRL_AESMODE_SHIFT
#define CE_CIPHER_CTRL_SAVE_SHIFT           CE0_CIPHER_CTRL_CIPHER_SAVE_FIELD_OFFSET
#define CE_CIPHER_CTRL_SAVE_MASK            1UL << CE_CIPHER_CTRL_SAVE_SHIFT
#define CE_CIPHER_CTRL_LOAD_SHIFT           CE0_CIPHER_CTRL_CIPHER_LOAD_FIELD_OFFSET
#define CE_CIPHER_CTRL_LOAD_MASK            1UL << CE_CIPHER_CTRL_LOAD_SHIFT
#define CE_CIPHER_CTRL_DECKEYCAL_SHIFT      CE0_CIPHER_CTRL_CIPHER_DECKEYCAL_FIELD_OFFSET
#define CE_CIPHER_CTRL_DECKEYCAL_MASK       1UL << CE_CIPHER_CTRL_DECKEYCAL_SHIFT
#define CE_CIPHER_CTRL_KEYSIZE_SHIFT        CE0_CIPHER_CTRL_CIPHER_KEYSIZE_FIELD_OFFSET
#define CE_CIPHER_CTRL_KEYSIZE_MASK         3UL << CE_CIPHER_CTRL_KEYSIZE_SHIFT
#define CE_CIPHER_CTRL_CIPHERMODE_SHIFT     CE0_CIPHER_CTRL_CIPHERMODE_FIELD_OFFSET
#define CE_CIPHER_CTRL_CIPHERMODE_MASK      1UL << CE_CIPHER_CTRL_CIPHERMODE_SHIFT
#define CE_CIPHER_CTRL_GO_SHIFT             CE0_CIPHER_CTRL_CIPHER_GO_FIELD_OFFSET
#define CE_CIPHER_CTRL_GO_MASK              1UL << CE_CIPHER_CTRL_GO_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_CIPHER_HEADER_LEN
// Register Offset : 0x10
// Description     :
//--------------------------------------------------------------------------
#define REG_CIPHER_HEADER_LEN_CE_(i)        (REG_AP_APB_CRYPTOENGINE_CE0_CIPHER_HEADER_LEN + CE_JUMP * i)
#define CE_CIPHER_HEADER_LEN_SHIFT          CE0_CIPHER_HEADER_LEN_CIPHER_HEADER_LEN_FIELD_OFFSET
#define CE_CIPHER_HEADER_LEN_MASK           0xFFFFFFFF << CE_CIPHER_HEADER_LEN_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_CIPHER_KEY_ADDR
// Register Offset : 0x14
// Description     :
//--------------------------------------------------------------------------
#define REG_CIPHER_KEY_ADDR_CE_(i)          (REG_AP_APB_CRYPTOENGINE_CE0_CIPHER_KEY_ADDR + CE_JUMP * i)
#define CE_CIPHER_KEY2_ADDR_SHIFT           CE0_CIPHER_KEY_ADDR_CIPHER_KEY2_ADDR_FIELD_OFFSET
#define CE_CIPHER_KEY2_ADDR_MASK            0xFFFF << CE_CIPHER_KEY2_ADDR_SHIFT
#define CE_CIPHER_KEY_ADDR_SHIFT            CE0_CIPHER_KEY_ADDR_CIPHER_KEY_ADDR_FIELD_OFFSET
#define CE_CIPHER_KEY_ADDR_MASK             0xFFFF << CE_CIPHER_KEY_ADDR_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_CIPHER_SRC_ADDR
// Register Offset : 0x18
// Description     :
//--------------------------------------------------------------------------
#define REG_CIPHER_SRC_ADDR_CE_(i)          (REG_AP_APB_CRYPTOENGINE_CE0_CIPHER_SRC_ADDR + CE_JUMP * i)
#define CE_CIPHER_SRC_ADDR_SHIFT            CE0_CIPHER_SRC_ADDR_CIPHER_SRC_ADDR_FIELD_OFFSET
#define CE_CIPHER_SRC_ADDR_MASK0            0xFFFFFFFF << CE_CIPHER_SRC_ADDR_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_CIPHER_SRC_ADDR_H
// Register Offset : 0x1c
// Description     :
//--------------------------------------------------------------------------
#define REG_CIPHER_SRC_ADDR_H_CE_(i)        (REG_AP_APB_CRYPTOENGINE_CE0_CIPHER_SRC_ADDR_H + CE_JUMP * i)
#define CE_CIPHER_SRC_TYPE_SHIFT            CE0_CIPHER_SRC_ADDR_H_CIPHER_SRC_TYPE_FIELD_OFFSET
#define CE_CIPHER_SRC_TYPE_MASK             7UL << CE_CIPHER_SRC_TYPE_SHIFT
#define CE_CIPHER_SRC_ADDR_H_SHIFT          CE0_CIPHER_SRC_ADDR_H_CIPHER_SRC_ADDR_H_FIELD_OFFSET
#define CE_CIPHER_SRC_ADDR_H_MASK           0xFF << CE_CIPHER_SRC_ADDR_H_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_CIPHER_DST_ADDR
// Register Offset : 0x20
// Description     :
//--------------------------------------------------------------------------
#define REG_CIPHER_DST_ADDR_CE_(i)          (REG_AP_APB_CRYPTOENGINE_CE0_CIPHER_DST_ADDR + CE_JUMP * i)
#define CE_CIPHER_DST_ADDR_SHIFT             CE0_CIPHER_DST_ADDR_CIPHER_DST_ADDR_FIELD_OFFSET
#define CE_CIPHER_DST_ADDR_MASK              0xFFFFFFFF << CE_CIPHER_DST_ADDR_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_CIPHER_DST_ADDR_H
// Register Offset : 0x24
// Description     :
//--------------------------------------------------------------------------
#define REG_CIPHER_DST_ADDR_H_CE_(i)        (REG_AP_APB_CRYPTOENGINE_CE0_CIPHER_DST_ADDR_H + CE_JUMP * i)
#define CE_CIPHER_DST_TYPE_SHIFT            CE0_CIPHER_DST_ADDR_H_CIPHER_DST_TYPE_FIELD_OFFSET
#define CE_CIPHER_DST_TYPE_MASK             7UL << CE_CIPHER_DST_TYPE_SHIFT
#define CE_CIPHER_DST_ADDR_H_SHIFT          CE0_CIPHER_DST_ADDR_H_CIPHER_DST_ADDR_H_FIELD_OFFSET
#define CE_CIPHER_DST_ADDR_H_MASK           0xFF << CE_CIPHER_DST_ADDR_H_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_CIPHER_IV_CONTEXT_ADDR
// Register Offset : 0x28
// Description     :
//--------------------------------------------------------------------------
#define REG_CIPHER_IV_CONTEXT_ADDR_CE_(i)   (REG_AP_APB_CRYPTOENGINE_CE0_CIPHER_IV_CONTEXT_ADDR + CE_JUMP * i)
#define CE_CIPHER_CONTEXT_ADDR_SHIFT        CE0_CIPHER_IV_CONTEXT_ADDR_CIPHER_CONTEXT_ADDR_FIELD_OFFSET
#define CE_CIPHER_CONTEXT_ADDR_MASK         0xFFFF << CE_CIPHER_CONTEXT_ADDR_SHIFT
#define CE_CIPHER_IV_ADDR_SHIFT             CE0_CIPHER_IV_CONTEXT_ADDR_CIPHER_IV_ADDR_FIELD_OFFSET
#define CE_CIPHER_IV_ADDR_MASK              0xFFFF << CE_CIPHER_IV_ADDR_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_CIPHER_PAYLOAD_LEN
// Register Offset : 0x2c
// Description     :
//--------------------------------------------------------------------------
#define REG_CIPHER_PAYLOAD_LEN_CE_(i)       (REG_AP_APB_CRYPTOENGINE_CE0_CIPHER_PAYLOAD_LEN  + CE_JUMP * i)
#define CE_CIPHER_PAYLOAD_LEN_SHIFT         CE0_CIPHER_PAYLOAD_LEN_CIPHER_PAYLOAD_LEN_FIELD_OFFSET
#define CE_CIPHER_PAYLOAD_LEN_MASK          0xFFFFFFFF << CE_CIPHER_PAYLOAD_LEN_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_HASH_CTRL
// Register Offset : 0x30
// Description     :
//--------------------------------------------------------------------------
#define REG_HASH_CTRL_CE_(i)                (REG_AP_APB_CRYPTOENGINE_CE0_HASH_CTRL + CE_JUMP * i)
#define CE_HASH_HMAC_EN_SHIFT               CE0_HASH_CTRL_HASH_HMAC_EN_FIELD_OFFSET
#define CE_HASH_HMAC_EN_MASK                1UL << CE_HASH_HMAC_EN_SHIFT
#define CE_HASH_KEY_SHIFT                   CE0_HASH_CTRL_HASH_KEY_FIELD_OFFSET
#define CE_HASH_KEY_MASK                    1UL << CE_HASH_KEY_SHIFT
#define CE_HASH_INIT_SECURE_SHIFT           CE0_HASH_CTRL_HASH_INIT_SECURE_FIELD_OFFSET
#define CE_HASH_INIT_SECURE_MASK            1UL << CE_HASH_INIT_SECURE_SHIFT
#define CE_HASH_INIT_SHIFT                  CE0_HASH_CTRL_HASH_INIT_FIELD_OFFSET
#define CE_HASH_INIT_MASK                   1UL << CE_HASH_INIT_SHIFT
#define CE_HASH_SHAUPDATE_SHIFT             CE0_HASH_CTRL_HASH_SHAUPDATE_FIELD_OFFSET
#define CE_HASH_SHAUPDATE_MASK              1UL << CE_HASH_SHAUPDATE_SHIFT
#define CE_HASH_PADDINGEN_SHIFT             CE0_HASH_CTRL_HASH_PADDINGEN_FIELD_OFFSET
#define CE_HASH_PADDINGEN_MASK              1UL << CE_HASH_PADDINGEN_SHIFT
#define CE_HASH_MODE_SHIFT                   CE0_HASH_CTRL_HASH_MODE_FIELD_OFFSET
#define CE_HASH_MODE_MASK                   0x7F << CE_HASH_MODE_SHIFT
#define CE_HASH_GO_SHIFT                    CE0_HASH_CTRL_HASH_GO_FIELD_OFFSET
#define CE_HASH_GO_MASK                     1UL << CE_HASH_GO_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_HASH_KEYIV_ADDR
// Register Offset : 0x34
// Description     :
//--------------------------------------------------------------------------
#define REG_HASH_KEYIV_ADDR_CE_(i)          (REG_AP_APB_CRYPTOENGINE_CE0_HASH_KEYIV_ADDR + CE_JUMP * i)
#define CE_HASH_KEY_ADDR_SHIFT              CE0_HASH_KEYIV_ADDR_HASH_KEY_ADDR_FIELD_OFFSET
#define CE_HASH_KEY_ADDR_MASK               0xFFFF << CE_HASH_KEY_ADDR_SHIFT
#define CE_HASH_IV_ADDR_SHIFT               CE0_HASH_KEYIV_ADDR_HASH_IV_ADDR_FIELD_OFFSET
#define CE_HASH_IV_ADDR_MASK                0xFFFF << CE_HASH_IV_ADDR_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_HMAC_KEY_CTRL
// Register Offset : 0x38
// Description     :
//--------------------------------------------------------------------------
#define REG_HMAC_KEY_CTRL_CE_(i)            (REG_AP_APB_CRYPTOENGINE_CE0_HMAC_KEY_CTRL + CE_JUMP * i)
#define CE_HMAC_KEY_TYPE_SHIFT              CE0_HMAC_KEY_CTRL_HMAC_KEY_TYPE_FIELD_OFFSET
#define CE_HMAC_KEY_TYPE_MASK               3UL << CE_HMAC_KEY_TYPE_SHIFT
#define CE_HMAC_KEY_LEN_SHIFT               CE0_HMAC_KEY_CTRL_HMAC_KEY_LEN_FIELD_OFFSET
#define CE_HMAC_KEY_LEN_MASK                0xFFFF << CE_HMAC_KEY_LEN_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_HASH_CALC_LEN
// Register Offset : 0x3c
// Description     :
//--------------------------------------------------------------------------
#define REG_HASH_CALC_LEN_CE_(i)            (REG_AP_APB_CRYPTOENGINE_CE0_HASH_CALC_LEN + CE_JUMP * i)
#define CE_HASH_CALC_LEN_SHIFT              CE0_HASH_CALC_LEN_HASH_CALC_LEN_FIELD_OFFSET
#define CE_HASH_CALC_LEN_MASK               0xFFFFFFFF << CE_HASH_CALC_LEN_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_HASH_SRC_ADDR
// Register Offset : 0x40
// Description     :
//--------------------------------------------------------------------------
#define REG_HASH_SRC_ADDR_CE_(i)            (REG_AP_APB_CRYPTOENGINE_CE0_HASH_SRC_ADDR + CE_JUMP * i)
#define CE_HASH_SRC_ADDR_SHIFT              CE0_HASH_SRC_ADDR_HASH_SRC_ADDR_FIELD_OFFSET
#define CE_HASH_SRC_ADDR_MASK               0xFFFFFFFF << CE_HASH_SRC_ADDR_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_HASH_SRC_ADDR_H
// Register Offset : 0x44
// Description     :
//--------------------------------------------------------------------------
#define REG_HASH_SRC_ADDR_H_CE_(i)          (REG_AP_APB_CRYPTOENGINE_CE0_HASH_SRC_ADDR_H + CE_JUMP * i)
#define CE_HASH_SRC_TYPE_SHIFT              CE0_HASH_SRC_ADDR_H_HASH_SRC_TYPE_FIELD_OFFSET
#define CE_HASH_SRC_TYPE_MASK               7UL << CE_HASH_SRC_TYPE_SHIFT
#define CE_HASH_SRC_ADDR_H_SHIFT            CE0_HASH_SRC_ADDR_H_HASH_SRC_ADDR_H_FIELD_OFFSET
#define CE_HASH_SRC_ADDR_H_MASK             0xFF << CE_HASH_SRC_ADDR_H_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_HASH_DST_ADDR
// Register Offset : 0x48
// Description     :
//--------------------------------------------------------------------------
#define REG_HASH_DST_ADDR_CE_(i)            (REG_AP_APB_CRYPTOENGINE_CE0_HASH_DST_ADDR + CE_JUMP * i)
#define CE_HASH_DST_TYPE_SHIFT              CE0_HASH_DST_ADDR_HASH_DST_TYPE_FIELD_OFFSET
#define CE_HASH_DST_TYPE_MASK               7UL << CE_HASH_DST_TYPE_SHIFT
#define CE_HASH_DST_ADDR_SHIFT              CE0_HASH_DST_ADDR_HASH_DST_ADDR_FIELD_OFFSET
#define CE_HASH_DST_ADDR_MASK               0xFFFF << CE_HASH_DST_ADDR_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_DMA_CTRL
// Register Offset : 0x50
// Description     :
//--------------------------------------------------------------------------
#define REG_DMA_CTRL_CE_(i)                 (REG_AP_APB_CRYPTOENGINE_CE0_DMA_CTRL + CE_JUMP * i)
#define CE_DMA_DONEINTEN_SHIFT              CE0_DMA_CTRL_DMA_DONEINTEN_FIELD_OFFSET
#define CE_DMA_DONEINTEN_MASK               1UL << CE_DMA_DONEINTEN_SHIFT
#define CE_DMA_CMDFIFO_FULL_SHIFT           CE0_DMA_CTRL_DMA_CMDFIFO_FULL_FIELD_OFFSET
#define CE_DMA_CMDFIFO_FULL_MASK            1UL << CE_DMA_CMDFIFO_FULL_SHIFT
#define CE_DMA_CMDFIFO_EMPTY_SHIFT          CE0_DMA_CTRL_DMA_CMDFIFO_EMPTY_FIELD_OFFSET
#define CE_DMA_CMDFIFO_EMPTY_MASK           1UL << CE_DMA_CMDFIFO_EMPTY_SHIFT
#define CE_DMA_GO_SHIFT                     CE0_DMA_CTRL_DMA_GO_FIELD_OFFSET
#define CE_DMA_GO_MASK                      1UL << CE_DMA_GO_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_DMA_SRC_ADDR
// Register Offset : 0x54
// Description     :
//--------------------------------------------------------------------------
#define REG_DMA_SRC_ADDR_CE_(i)             (REG_AP_APB_CRYPTOENGINE_CE0_DMA_SRC_ADDR + CE_JUMP * i)
#define CE_DMA_SRC_ADDR_SHIFT               CE0_DMA_SRC_ADDR_DMA_SRC_ADDR_FIELD_OFFSET
#define CE_DMA_SRC_ADDR_MASK                0xFFFFFFFF << CE_DMA_SRC_ADDR_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_DMA_SRC_ADDR_H
// Register Offset : 0x58
// Description     :
//--------------------------------------------------------------------------
#define REG_DMA_SRC_ADDR_H_CE_(i)           (REG_AP_APB_CRYPTOENGINE_CE0_DMA_SRC_ADDR_H + CE_JUMP * i)
#define CE_DMA_SRC_TYPE_SHIFT               CE0_DMA_SRC_ADDR_H_DMA_SRC_TYPE_FIELD_OFFSET
#define CE_DMA_SRC_TYPE_MASK                7UL << CE_DMA_SRC_TYPE_SHIFT
#define CE_DMA_SRC_ADDR_H_SHIFT             CE0_DMA_SRC_ADDR_H_DMA_SRC_ADDR_H_FIELD_OFFSET
#define CE_DMA_SRC_ADDR_H_MASK              0xFF << CE_DMA_SRC_ADDR_H_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_DMA_DST_ADDR
// Register Offset : 0x5c
// Description     :
//--------------------------------------------------------------------------
#define REG_DMA_DST_ADDR_CE_(i)             (REG_AP_APB_CRYPTOENGINE_CE0_DMA_DST_ADDR + CE_JUMP * i)
#define CE_DMA_DST_ADDR_SHIFT               CE0_DMA_DST_ADDR_DMA_DST_ADDR_FIELD_OFFSET
#define CE_DMA_DST_ADDR_MASK                0xFFFFFFFF << CE_DMA_DST_ADDR_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_DMA_DST_ADDR_H
// Register Offset : 0x60
// Description     :
//--------------------------------------------------------------------------
#define REG_DMA_DST_ADDR_H_CE_(i)           (REG_AP_APB_CRYPTOENGINE_CE0_DMA_DST_ADDR_H + CE_JUMP * i)
#define CE_DMA_DST_TYPE_SHIFT               CE0_DMA_DST_ADDR_H_DMA_DST_TYPE_FIELD_OFFSET
#define CE_DMA_DST_TYPE_MASK                7UL << CE_DMA_DST_TYPE_SHIFT
#define CE_DMA_DST_ADDR_H_SHIFT             CE0_DMA_DST_ADDR_H_DMA_DST_ADDR_H_FIELD_OFFSET
#define CE_DMA_DST_ADDR_H_MASK              0xFF << CE_DMA_DST_ADDR_H_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_DMA_LEN
// Register Offset : 0x64
// Description     :
//--------------------------------------------------------------------------
#define REG_DMA_LEN_CE_(i)                  (REG_AP_APB_CRYPTOENGINE_CE0_DMA_LEN + CE_JUMP * i)
#define CE_DMA_LEN_SHIFT                    CE0_DMA_LEN_DMA_LEN_FIELD_OFFSET
#define CE_DMA_LEN_MASK                     0xFFFF << CE_DMA_LEN_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_PK_POINTERREG
// Register Offset : 0x80
// Description     :
//--------------------------------------------------------------------------
#define REG_PK_POINTERREG_CE_(i)            (REG_AP_APB_CRYPTOENGINE_CE0_PK_POINTERREG + CE_JUMP * i)
#define CE_PK_OPPTRN_SHIFT                  CE0_PK_POINTERREG_PK_OPPTRN_FIELD_OFFSET
#define CE_PK_OPPTRN_MASK                   0xF << CE_PK_OPPTRN_SHIFT
#define CE_PK_OPPTRC_SHIFT                  CE0_PK_POINTERREG_PK_OPPTRC_FIELD_OFFSET
#define CE_PK_OPPTRC_MASK                   0xF << CE_PK_OPPTRC_SHIFT
#define CE_PK_OPPTRB_SHIFT                  CE0_PK_POINTERREG_PK_OPPTRB_FIELD_OFFSET
#define CE_PK_OPPTRB_MASK                   0xF << CE_PK_OPPTRB_SHIFT
#define CE_PK_OPPTRA_SHIFT                  CE0_PK_POINTERREG_PK_OPPTRA_FIELD_OFFSET
#define CE_PK_OPPTRA_MASK                   0xF << CE_PK_OPPTRA_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_PK_COMMANDREG
// Register Offset : 0x84
// Description     :
//--------------------------------------------------------------------------
#define REG_PK_COMMANDREG_CE_(i)            (REG_AP_APB_CRYPTOENGINE_CE0_PK_COMMANDREG + CE_JUMP * i)
#define CE_PK_CALCR2_SHIFT                  CE0_PK_COMMANDREG_PK_CALCR2_FIELD_OFFSET
#define CE_PK_CALCR2_MASK                   1UL << CE_PK_CALCR2_SHIFT
#define CE_PK_FLAGB_SHIFT                   CE0_PK_COMMANDREG_PK_FLAGB_FIELD_OFFSET
#define CE_PK_FLAGB_MASK                    1UL << CE_PK_FLAGB_SHIFT
#define CE_PK_FLAGA_SHIFT                   CE0_PK_COMMANDREG_PK_FLAGA_FIELD_OFFSET
#define CE_PK_FLAGA_MASK                    1UL << CE_PK_FLAGA_SHIFT
#define CE_PK_SWAP_SHIFT                    CE0_PK_COMMANDREG_PK_SWAP_FIELD_OFFSET
#define CE_PK_SWAP_MASK                     1UL << CE_PK_SWAP_SHIFT
#define CE_PK_BUFFER_SHIFT                  CE0_PK_COMMANDREG_PK_BUFFER_FIELD_OFFSET
#define CE_PK_BUFFER_MASK                   1UL << CE_PK_BUFFER_SHIFT
#define CE_PK_EDWARDS_SHIFT                 CE0_PK_COMMANDREG_PK_EDWARDS_FIELD_OFFSET
#define CE_PK_EDWARDS_MASK                  1UL << CE_PK_EDWARDS_SHIFT
#define CE_PK_RANDPROJ_SHIFT                CE0_PK_COMMANDREG_PK_RANDPROJ_FIELD_OFFSET
#define CE_PK_RANDPROJ_MASK                 1UL << CE_PK_RANDPROJ_SHIFT
#define CE_PK_RANDKE_SHIFT                  CE0_PK_COMMANDREG_PK_RANDKE_FIELD_OFFSET
#define CE_PK_RANDKE_MASK                   1UL << CE_PK_RANDKE_SHIFT
#define CE_PK_SELCURVE_SHIFT                CE0_PK_COMMANDREG_PK_SELCURVE_FIELD_OFFSET
#define CE_PK_SELCURVE_MASK                 0xF << CE_PK_SELCURVE_SHIFT
#define CE_PK_SIZE_SHIFT                    CE0_PK_COMMANDREG_PK_SIZE_FIELD_OFFSET
#define CE_PK_SIZE_MASK                     0x7FF << CE_PK_SIZE_SHIFT
#define CE_PK_FIELD_SHIFT                   CE0_PK_COMMANDREG_PK_FIELD_FIELD_OFFSET
#define CE_PK_FIELD_MASK                    1UL << CE_PK_FIELD_SHIFT
#define CE_PK_TYPE_SHIFT                    CE0_PK_COMMANDREG_PK_TYPE_FIELD_OFFSET
#define CE_PK_TYPE_MASK                     0x7F << CE_PK_TYPE_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_PKE_CTRL
// Register Offset : 0x88
// Description     :
//--------------------------------------------------------------------------
#define REG_PKE_CTRL_CE_(i)                 (REG_AP_APB_CRYPTOENGINE_CE0_PKE_CTRL + CE_JUMP * i)
#define CE_PKE_SOFT_RST_SHIFT               CE0_PKE_CTRL_PKE_SOFT_RST_FIELD_OFFSET
#define CE_PKE_SOFT_RST_MASK                1UL << CE_PKE_SOFT_RST_SHIFT
#define CE_PKE_CLRMEMAFTEROP_SHIFT          CE0_PKE_CTRL_PK_CLRMEMAFTEROP_FIELD_OFFSET
#define CE_PKE_CLRMEMAFTEROP_MASK           1UL << CE_PKE_CLRMEMAFTEROP_SHIFT
#define CE_PKE_GO_SHIFT                     CE0_PKE_CTRL_PKE_GO_FIELD_OFFSET
#define CE_PKE_GO_MASK                      1UL << CE_PKE_GO_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_PK_STATUSREG
// Register Offset : 0x8c
// Description     :
//--------------------------------------------------------------------------
#define REG_PK_STATUSREG_CE_(i)             (REG_AP_APB_CRYPTOENGINE_CE0_PK_STATUSREG + CE_JUMP * i)
#define CE_PK_INTERRUPT_SHIFT               CE0_PK_STATUSREG_PK_INTERRUPT_FIELD_OFFSET
#define CE_PK_INTERRUPT_MASK                1UL << CE_PK_INTERRUPT_SHIFT
#define CE_PK_BUSY_SHIFT                    CE0_PK_STATUSREG_PK_BUSY_FIELD_OFFSET
#define CE_PK_BUSY_MASK                     1UL << CE_PK_BUSY_SHIFT
#define CE_PK_NOTQUADRATICRESIDUE_SHIFT     CE0_PK_STATUSREG_PK_NOTQUADRATICRESIDUE_FIELD_OFFSET
#define CE_PK_NOTQUADRATICRESIDUE_MASK      1UL << CE_PK_NOTQUADRATICRESIDUE_SHIFT
#define CE_PK_COMPOSITE_SHIFT               CE0_PK_STATUSREG_PK_COMPOSITE_FIELD_OFFSET
#define CE_PK_COMPOSITE_MASK                1UL << CE_PK_COMPOSITE_SHIFT
#define CE_PK_NOTINVERTIBLE_SHIFT           CE0_PK_STATUSREG_PK_NOTINVERTIBLE_FIELD_OFFSET
#define CE_PK_NOTINVERTIBLE_MASK            1UL << CE_PK_NOTINVERTIBLE_SHIFT
#define CE_PK_PARAM_AB_NOTVALID_SHIFT       CE0_PK_STATUSREG_PK_PARAM_AB_NOTVALID_FIELD_OFFSET
#define CE_PK_PARAM_AB_NOTVALID_MASK        1UL << CE_PK_PARAM_AB_NOTVALID_SHIFT
#define CE_PK_SIGNATURE_NOTVALID_SHIFT      CE0_PK_STATUSREG_PK_SIGNATURE_NOTVALID_FIELD_OFFSET
#define CE_PK_SIGNATURE_NOTVALID_MASK       1UL << CE_PK_SIGNATURE_NOTVALID_SHIFT
#define CE_PK_NOTIMPLEMENTED_SHIFT          CE0_PK_STATUSREG_PK_NOTIMPLEMENTED_FIELD_OFFSET
#define CE_PK_NOTIMPLEMENTED_MASK           1UL << CE_PK_NOTIMPLEMENTED_SHIFT
#define CE_PK_PARAM_N_NOTVALID_SHIFT        CE0_PK_STATUSREG_PK_PARAM_N_NOTVALID_FIELD_OFFSET
#define CE_PK_PARAM_N_NOTVALID_MASK         1UL << CE_PK_PARAM_N_NOTVALID_SHIFT
#define CE_PK_COUPLE_NOTVALID_SHIFT         CE0_PK_STATUSREG_PK_COUPLE_NOTVALID_FIELD_OFFSET
#define CE_PK_COUPLE_NOTVALID_MASK          1UL << CE_PK_COUPLE_NOTVALID_SHIFT
#define CE_PK_POINT_PX_ATINFINITY_SHIFT     CE0_PK_STATUSREG_PK_POINT_PX_ATINFINITY_FIELD_OFFSET
#define CE_PK_POINT_PX_ATINFINITY_MASK      1UL << CE_PK_POINT_PX_ATINFINITY_SHIFT
#define CE_PK_POINT_PX_NOTONCURVE_SHIFT     CE0_PK_STATUSREG_PK_POINT_PX_NOTONCURVE_FIELD_OFFSET
#define CE_PK_POINT_PX_NOTONCURVE_MASK      1UL << CE_PK_POINT_PX_NOTONCURVE_SHIFT
#define CE_PK_FAIL_ADDRESS_SHIFT            CE0_PK_STATUSREG_PK_FAIL_ADDRESS_FIELD_OFFSET
#define CE_PK_FAIL_ADDRESS_MASK             0xF << CE_PK_FAIL_ADDRESS_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_PK_TIMER
// Register Offset : 0x94
// Description     :
//--------------------------------------------------------------------------
#define REG_PK_TIMER_CE_(i)                 (REG_AP_APB_CRYPTOENGINE_CE0_PK_TIMER + CE_JUMP * i)
#define CE_PK_TIMER_SHIFT                   CE0_PK_TIMER_PK_TIMER_FIELD_OFFSET
#define CE_PK_TIMER_MASK                    0xFFFFFFFF << CE_PK_TIMER_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_PK_HWCONFIGREG
// Register Offset : 0x98
// Description     :
//--------------------------------------------------------------------------
#define REG_PK_HWCONFIGREG_CE_(i)           (REG_AP_APB_CRYPTOENGINE_CE0_PK_HWCONFIGREG + CE_JUMP * i)
#define CE_PK_DISABLECM_SHIFT               CE0_PK_HWCONFIGREG_PK_DISABLECM_FIELD_OFFSET
#define CE_PK_DISABLECM_MASK                1UL << CE_PK_DISABLECM_SHIFT
#define CE_PK_25519_SHIFT                   CE0_PK_HWCONFIGREG_PK_25519_FIELD_OFFSET
#define CE_PK_25519_MASK                    1UL << CE_PK_25519_SHIFT
#define CE_PK_P192_SHIFT                    CE0_PK_HWCONFIGREG_PK_P192_FIELD_OFFSET
#define CE_PK_P192_MASK                     1UL << CE_PK_P192_SHIFT
#define CE_PK_P521_SHIFT                    CE0_PK_HWCONFIGREG_PK_P521_FIELD_OFFSET
#define CE_PK_P521_MASK                     1UL << CE_PK_P521_SHIFT
#define CE_PK_P384_SHIFT                    CE0_PK_HWCONFIGREG_PK_P384_FIELD_OFFSET
#define CE_PK_P384_MASK                     1UL << CE_PK_P384_SHIFT
#define CE_PK_P256_SHIFT                    CE0_PK_HWCONFIGREG_PK_P256_FIELD_OFFSET
#define CE_PK_P256_MASK                     1UL << CE_PK_P256_SHIFT
#define CE_PK_BINARYFIELD_SHIFT             CE0_PK_HWCONFIGREG_PK_BINARYFIELD_FIELD_OFFSET
#define CE_PK_BINARYFIELD_MASK              1UL << CE_PK_BINARYFIELD_SHIFT
#define CE_PK_PRIMEFIELD_SHIFT              CE0_PK_HWCONFIGREG_PK_PRIMEFIELD_FIELD_OFFSET
#define CE_PK_PRIMEFIELD_MASK               1UL << CE_PK_PRIMEFIELD_SHIFT
#define CE_PK_NBMULT_SHIFT                  CE0_PK_HWCONFIGREG_PK_NBMULT_FIELD_OFFSET
#define CE_PK_NBMULT_MASK                   0xF << CE_PK_NBMULT_SHIFT
#define CE_PK_MAXOPSIZE_SHIFT               CE0_PK_HWCONFIGREG_PK_MAXOPSIZE_FIELD_OFFSET
#define CE_PK_MAXOPSIZE_MASK                0xFFF << CE_PK_MAXOPSIZE_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_PK_RESULTS_CTRL
// Register Offset : 0x9c
// Description     :
//--------------------------------------------------------------------------
#define REG_PK_RESULTS_CTRL_CE_(i)          (REG_AP_APB_CRYPTOENGINE_CE0_PK_RESULTS_CTRL + CE_JUMP * i)
#define CE_PKE_RESULTS_OP_LEN_SHIFT         CE0_PK_RESULTS_CTRL_PKE_RESULTS_OP_LEN_FIELD_OFFSET
#define CE_PKE_RESULTS_OP_LEN_MASK          0xFFFF << CE_PKE_RESULTS_OP_LEN_SHIFT
#define CE_PKE_RESULTS_OP_SELECT_SHIFT      CE0_PK_RESULTS_CTRL_PKE_RESULTS_OP_SELECT_FIELD_OFFSET
#define CE_PKE_RESULTS_OP_SELECT_MASK       0xFFFF << CE_PKE_RESULTS_OP_SELECT_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_PK_RESULTS_DST_ADDR
// Register Offset : 0xa0
// Description     :
//--------------------------------------------------------------------------
#define REG_PK_RESULTS_DST_ADDR_CE_(i)      (REG_AP_APB_CRYPTOENGINE_CE0_PK_RESULTS_DST_ADDR + CE_JUMP * i)
#define CE_ADDR_PKE_RESULTS_DST_ADDR_SHIFT  CE0_PK_RESULTS_DST_ADDR_PKE_RESULTS_DST_ADDR_FIELD_OFFSET
#define CE_ADDR_PKE_RESULTS_DST_ADDR_MASK   0xFFFFFFFF << CE_ADDR_PKE_RESULTS_DST_ADDR_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_PK_RESULTS_DST_ADDR_H
// Register Offset : 0xa4
// Description     :
//--------------------------------------------------------------------------
#define REG_PK_RESULTS_DST_ADDR_H_CE_(i)    (REG_AP_APB_CRYPTOENGINE_CE0_PK_RESULTS_DST_ADDR_H + CE_JUMP * i)
#define CE_PKE_RESULTS_DST_TYPE_SHIFT       CE0_PK_RESULTS_DST_ADDR_H_PKE_RESULTS_DST_TYPE_FIELD_OFFSET
#define CE_PKE_RESULTS_DST_TYPE_MASK        7UL << CE_PKE_RESULTS_DST_TYPE_SHIFT
#define CE_PKE_RESULTS_DST_ADDR_H_SHIFT     CE0_PK_RESULTS_DST_ADDR_H_PKE_RESULTS_DST_ADDR_H_FIELD_OFFSET
#define CE_PKE_RESULTS_DST_ADDR_H_MASK      0xFF << CE_PKE_RESULTS_DST_ADDR_H_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_TRNG_STATUS
// Register Offset : 0x100
// Description     :
//--------------------------------------------------------------------------
#define REG_TRNG_STATUS_CE_(i)              (REG_AP_APB_CRYPTOENGINE_CE0_TRNG_STATUS + CE_JUMP * i)
#define CE_RNG_SEQ_RUNNING_SHIFT            CE0_TRNG_STATUS_RNG_SEQ_RUNNING_FIELD_OFFSET
#define CE_RNG_SEQ_RUNNING_MASK             1UL << CE_RNG_SEQ_RUNNING_SHIFT
#define CE_RNG_READY_SHIFT                  CE0_TRNG_STATUS_RNG_READY_FIELD_OFFSET
#define CE_RNG_READY_MASK                   1UL << CE_RNG_READY_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_TRNG_NUM0
// Register Offset : 0x104
// Description     :
//--------------------------------------------------------------------------
#define REG_TRNG_NUM0_CE_(i)                (REG_AP_APB_CRYPTOENGINE_CE0_TRNG_NUM0 + CE_JUMP * i)
#define CE_TRNG_NUM0_SHIFT                  CE0_TRNG_NUM0_TRNG_NUM0_FIELD_OFFSET
#define CE_TRNG_NUM0_MASK                   0xFFFFFFFF << CE_TRNG_NUM0_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_TRNG_NUM1
// Register Offset : 0x108
// Description     :
//--------------------------------------------------------------------------
#define REG_TRNG_NUM1_CE_(i)                (REG_AP_APB_CRYPTOENGINE_CE0_TRNG_NUM1 + CE_JUMP * i)
#define CE_TRNG_NUM1_SHIFT                  CE0_TRNG_NUM1_TRNG_NUM1_FIELD_OFFSET
#define CE_TRNG_NUM1_MASK                   0xFFFFFFFF << CE_TRNG_NUM1_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_HRNG_NUM
// Register Offset : 0x114
// Description     :
//--------------------------------------------------------------------------
#define REG_HRNG_NUM_CE_(i)                 (REG_AP_APB_CRYPTOENGINE_CE0_HRNG_NUM + CE_JUMP * i)
#define CE_HRNG_NUM_SHIFT                   CE0_HRNG_NUM_HRNG_NUM_FIELD_OFFSET
#define CE_HRNG_NUM_MASK                    0xFFFFFFFF << CE_HRNG_NUM_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_SKC_CTRL
// Register Offset : 0x120
// Description     :
//--------------------------------------------------------------------------
#define REG_SKC_CTRL_CE_(i)                 (REG_AP_APB_CRYPTOENGINE_CE0_SKC_CTRL + CE_JUMP * i)
#define CE_SKC_CMD_SHIFT                    CE0_SKC_CTRL_SKC_CMD_FIELD_OFFSET
#define CE_SKC_CMD_MASK                     7UL << CE_SKC_CMD_SHIFT
#define CE_SKC_GO_SHIFT                     CE0_SKC_CTRL_SKC_GO_FIELD_OFFSET
#define CE_SKC_GO_MASK                      1UL << CE_SKC_GO_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_SKC_RNG_ADDR
// Register Offset : 0x124
// Description     :
//--------------------------------------------------------------------------
#define REG_SKC_RNG_ADDR_CE_(i)             (REG_AP_APB_CRYPTOENGINE_CE0_SKC_RNG_ADDR + CE_JUMP * i)
#define CE_SKC_RNG_SECURE_SHIFT             CE0_SKC_RNG_ADDR_SKC_RNG_SECURE_FIELD_OFFSET
#define CE_SKC_RNG_SECURE_MASK              1UL << CE_SKC_RNG_SECURE_SHIFT
#define CE_SKC_RNG_LEN_SHIFT                CE0_SKC_RNG_ADDR_SKC_RNG_LEN_FIELD_OFFSET
#define CE_SKC_RNG_LEN_MASK                 0x7F << CE_SKC_RNG_LEN_SHIFT
#define CE_SKC_RNG_ADDR_SHIFT               CE0_SKC_RNG_ADDR_SKC_RNG_ADDR_FIELD_OFFSET
#define CE_SKC_RNG_ADDR_MASK                0x1FFF << CE_SKC_RNG_ADDR_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_SKC_KEYBLOB_CTRL
// Register Offset : 0x128
// Description     :
//--------------------------------------------------------------------------
#define REG_SKC_KEYBLOB_CTRL_CE_(i)         (REG_AP_APB_CRYPTOENGINE_CE0_SKC_KEYBLOB_CTRL + CE_JUMP * i)
#define CE_SKC_KEYBLOB_CTRL_SHIFT           CE0_SKC_KEYBLOB_CTRL_SKC_KEYBLOB_CTRL_FIELD_OFFSET
#define CE_SKC_KEYBLOB_CTRL_MASK            1UL << CE_SKC_KEYBLOB_CTRL_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_INTEN
// Register Offset : 0x140
// Description     :
//--------------------------------------------------------------------------
#define REG_INTEN_CE_(i)                    (REG_AP_APB_CRYPTOENGINE_CE0_INTEN + CE_JUMP * i)
#define CE_DMA_FIFOERROR_INTEN_SHIFT        CE0_INTEN_DMA_FIFOERROR_INTEN_FIELD_OFFSET
#define CE_DMA_FIFOERROR_INTEN_MASK         1UL << CE_DMA_FIFOERROR_INTEN_SHIFT
#define CE_PKE_INTEGRITY_ERROR_INTEN_SHIFT  CE0_INTEN_PKE_INTEGRITY_ERROR_INTEN_FIELD_OFFSET
#define CE_PKE_INTEGRITY_ERROR_INTEN_MASK   1UL << CE_PKE_INTEGRITY_ERROR_INTEN_SHIFT
#define CE_HASH_INTEGRITY_ERROR_INTEN_SHIFT CE0_INTEN_HASH_INTEGRITY_ERROR_INTEN_FIELD_OFFSET
#define CE_HASH_INTEGRITY_ERROR_INTEN_MASK  1UL << CE_HASH_INTEGRITY_ERROR_INTEN_SHIFT
#define CE_CIPHER_INTEGRITY_ERROR_INTEN_SHIFT  CE0_INTEN_CIPHER_INTEGRITY_ERROR_INTEN_FIELD_OFFSET
#define CE_CIPHER_INTEGRITY_ERROR_INTEN_MASK   1UL << CE_CIPHER_INTEGRITY_ERROR_INTEN_SHIFT
#define CE_SKC_INTEGRITY_ERROR_INTEN_SHIFT  CE0_INTEN_SKC_INTEGRITY_ERROR_INTEN_FIELD_OFFSET
#define CE_SKC_INTEGRITY_ERROR_INTEN_MASK   1UL << CE_SKC_INTEGRITY_ERROR_INTEN_SHIFT
#define CE_DMA_INTEGRITY_ERROR_INTEN_SHIFT  CE0_INTEN_DMA_INTEGRITY_ERROR_INTEN_FIELD_OFFSET
#define CE_DMA_INTEGRITY_ERROR_INTEN_MASK   1UL << CE_DMA_INTEGRITY_ERROR_INTEN_SHIFT
#define CE_PKE_DONE_INTEN_SHIFT             CE0_INTEN_PKE_DONE_INTEN_FIELD_OFFSET
#define CE_PKE_DONE_INTEN_MASK              1UL << CE_PKE_DONE_INTEN_SHIFT
#define CE_HASH_DONE_INTEN_SHIFT            CE0_INTEN_HASH_DONE_INTEN_FIELD_OFFSET
#define CE_HASH_DONE_INTEN_MASK             1UL << CE_HASH_DONE_INTEN_SHIFT
#define CE_CIPHER_DONE_INTEN_SHIFT          CE0_INTEN_CIPHER_DONE_INTEN_FIELD_OFFSET
#define CE_CIPHER_DONE_INTEN_MASK           1UL << CE_CIPHER_DONE_INTEN_SHIFT
#define CE_SKC_DONE_INTEN_SHIFT             CE0_INTEN_SKC_DONE_INTEN_FIELD_OFFSET
#define CE_SKC_DONE_INTEN_MASK              1UL << CE_SKC_DONE_INTEN_SHIFT
#define CE_DMA_DONE_INTEN_SHIFT             CE0_INTEN_DMA_DONE_INTEN_FIELD_OFFSET
#define CE_DMA_DONE_INTEN_MASK              1UL << CE_DMA_DONE_INTEN_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_INTSTAT
// Register Offset : 0x144
// Description     :
//--------------------------------------------------------------------------
#define REG_INTSTAT_CE_(i)                  (REG_AP_APB_CRYPTOENGINE_CE0_INTSTAT + CE_JUMP * i)
#define CE_DMA_FIFOERROR_INTSTAT_SHIFT      CE0_INTSTAT_DMA_FIFOERROR_INTSTAT_FIELD_OFFSET
#define CE_DMA_FIFOERROR_INTSTAT_MASK       1UL << CE_DMA_FIFOERROR_INTSTAT_SHIFT
#define CE_PKE_INTEGRITY_ERROR_INTSTAT_SHIFT    CE0_INTSTAT_PKE_INTEGRITY_ERROR_INTSTAT_FIELD_OFFSET
#define CE_PKE_INTEGRITY_ERROR_INTSTAT_MASK     1UL << CE_PKE_INTEGRITY_ERROR_INTSTAT_SHIFT
#define CE_HASH_INTEGRITY_ERROR_INTSTAT_SHIFT   CE0_INTSTAT_HASH_INTEGRITY_ERROR_INTSTAT_FIELD_OFFSET
#define CE_HASH_INTEGRITY_ERROR_INTSTAT_MASK    1UL << CE_HASH_INTEGRITY_ERROR_INTSTAT_SHIFT
#define CE_CIPHER_INTEGRITY_ERROR_INTSTAT_SHIFT CE0_INTSTAT_CIPHER_INTEGRITY_ERROR_INTSTAT_FIELD_OFFSET
#define CE_CIPHER_INTEGRITY_ERROR_INTSTAT_MASK  1UL << CE_CIPHER_INTEGRITY_ERROR_INTSTAT_SHIFT
#define CE_SKC_INTEGRITY_ERROR_INTSTAT_SHIFT    CE0_INTSTAT_SKC_INTEGRITY_ERROR_INTSTAT_FIELD_OFFSET
#define CE_SKC_INTEGRITY_ERROR_INTSTAT_MASK     1UL << CE_SKC_INTEGRITY_ERROR_INTSTAT_SHIFT
#define CE_DMA_INTEGRITY_ERROR_INTSTAT_SHIFT    CE0_INTSTAT_DMA_INTEGRITY_ERROR_INTSTAT_FIELD_OFFSET
#define CE_DMA_INTEGRITY_ERROR_INTSTAT_MASK     1UL << CE_DMA_INTEGRITY_ERROR_INTSTAT_SHIFT
#define CE_PKE_DONE_INTSTAT_SHIFT           CE0_INTSTAT_PKE_DONE_INTSTAT_FIELD_OFFSET
#define CE_PKE_DONE_INTSTAT_MASK            1UL << CE_PKE_DONE_INTSTAT_SHIFT
#define CE_HASH_DONE_INTSTAT_SHIFT          CE0_INTSTAT_HASH_DONE_INTSTAT_FIELD_OFFSET
#define CE_HASH_DONE_INTSTAT_MASK           1UL << CE_HASH_DONE_INTSTAT_SHIFT
#define CE_CIPHER_DONE_INTSTAT_SHIFT        CE0_INTSTAT_CIPHER_DONE_INTSTAT_FIELD_OFFSET
#define CE_CIPHER_DONE_INTSTAT_MASK         1UL << CE_CIPHER_DONE_INTSTAT_SHIFT
#define CE_SKC_DONE_INTSTAT_SHIFT           CE0_INTSTAT_SKC_DONE_INTSTAT_FIELD_OFFSET
#define CE_SKC_DONE_INTSTAT_MASK            1UL << CE_SKC_DONE_INTSTAT_SHIFT
#define CE_DMA_DONE_INTSTAT_SHIFT           CE0_INTSTAT_DMA_DONE_INTSTAT_FIELD_OFFSET
#define CE_DMA_DONE_INTSTAT_MASK            1UL << CE_DMA_DONE_INTSTAT_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_CE0_INTCLR
// Register Offset : 0x148
// Description     :
//--------------------------------------------------------------------------
#define REG_INTCLR_CE_(i)                   (REG_AP_APB_CRYPTOENGINE_CE0_INTCLR + CE_JUMP * i)
#define CE_DMA_FIFOERROR_INTCLR_SHIFT       CE0_INTCLR_DMA_FIFOERROR_INTCLR_FIELD_OFFSET
#define CE_DMA_FIFOERROR_INTCLR_MASK        1UL << CE_DMA_FIFOERROR_INTCLR_SHIFT
#define CE_PKE_INTEGRITY_ERROR_INTCLR_SHIFT CE0_INTCLR_PKE_INTEGRITY_ERROR_INTCLR_FIELD_OFFSET
#define CE_PKE_INTEGRITY_ERROR_INTCLR_MASK  1UL << CE_PKE_INTEGRITY_ERROR_INTCLR_SHIFT
#define CE_HASH_INTEGRITY_ERROR_INTCLR_SHIFT    CE0_INTCLR_HASH_INTEGRITY_ERROR_INTCLR_FIELD_OFFSET
#define CE_HASH_INTEGRITY_ERROR_INTCLR_MASK     1UL << CE_HASH_INTEGRITY_ERROR_INTCLR_SHIFT
#define CE_CIPHER_INTEGRITY_ERROR_INTCLR_SHIFT  CE0_INTCLR_CIPHER_INTEGRITY_ERROR_INTCLR_FIELD_OFFSET
#define CE_CIPHER_INTEGRITY_ERROR_INTCLR_MASK   1UL << CE_CIPHER_INTEGRITY_ERROR_INTCLR_SHIFT
#define CE_SKC_INTEGRITY_ERROR_INTCLR_SHIFT     CE0_INTCLR_SKC_INTEGRITY_ERROR_INTCLR_FIELD_OFFSET
#define CE_SKC_INTEGRITY_ERROR_INTCLR_MASK      1UL << CE_SKC_INTEGRITY_ERROR_INTCLR_SHIFT
#define CE_DMA_INTEGRITY_ERROR_INTCLR_SHIFT     CE0_INTCLR_DMA_INTEGRITY_ERROR_INTCLR_FIELD_OFFSET
#define CE_DMA_INTEGRITY_ERROR_INTCLR_MASK      1UL << CE_DMA_INTEGRITY_ERROR_INTCLR_SHIFT
#define CE_PKE_DONE_INTCLR_SHIFT            CE0_INTCLR_PKE_DONE_INTCLR_FIELD_OFFSET
#define CE_PKE_DONE_INTCLR_MASK             1UL << CE_PKE_DONE_INTCLR_SHIFT
#define CE_HASH_DONE_INTCLR_SHIFT           CE0_INTCLR_HASH_DONE_INTCLR_FIELD_OFFSET
#define CE_HASH_DONE_INTCLR_MASK            1UL << CE_HASH_DONE_INTCLR_SHIFT
#define CE_CIPHER_DONE_INTCLR_SHIFT         CE0_INTCLR_CIPHER_DONE_INTCLR_FIELD_OFFSET
#define CE_CIPHER_DONE_INTCLR_MASK          1UL << CE_CIPHER_DONE_INTCLR_SHIFT
#define CE_SKC_DONE_INTCLR_SHIFT            CE0_INTCLR_SKC_DONE_INTCLR_FIELD_OFFSET
#define CE_SKC_DONE_INTCLR_MASK             1UL << CE_SKC_DONE_INTCLR_SHIFT
#define CE_DMA_DONE_INTCLR_SHIFT            CE0_INTCLR_DMA_DONE_INTCLR_FIELD_OFFSET
#define CE_DMA_DONE_INTCLR_MASK             1UL << CE_DMA_DONE_INTCLR_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_SECURE_CTRL
// Register Offset : 0x8000
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_SECURE_CTRL                  (REG_AP_APB_CRYPTOENGINE_SECURE_CTRL)
#define CE_SRAM_SIZE_VIO_SHIFT              SECURE_CTRL_CE_SRAM_SIZE_VIO_FIELD_OFFSET
#define CE_SRAM_SIZE_VIO_MASK               0xFF << CE_SRAM_SIZE_VIO_SHIFT
#define SRAM_SIZE_TOTAL_VIO_SHIFT           SECURE_CTRL_SRAM_SIZE_TOTAL_VIO_FIELD_OFFSET
#define SRAM_SIZE_TOTAL_VIO_MASK            1UL << SRAM_SIZE_TOTAL_VIO_SHIFT
#define PKE_CM_DISABLE_SHIFT                SECURE_CTRL_PKE_CM_DISABLE_FIELD_OFFSET
#define PKE_CM_DISABLE_MASK                 1UL << PKE_CM_DISABLE_SHIFT
#define SECURE_VIOLATION_STATUS_SHIFT       SECURE_CTRL_SECURE_VIOLATION_STATUS_FIELD_OFFSET
#define SECURE_VIOLATION_STATUS_MASK        0xFF << SECURE_VIOLATION_STATUS_SHIFT
#define SECURE_VIOLATION_ENABLE_SHIFT       SECURE_CTRL_SECURE_VIOLATION_ENABLE_FIELD_OFFSET
#define SECURE_VIOLATION_ENABLE_MASK        0xFF << SECURE_VIOLATION_ENABLE_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_SESRAM_SIZE
// Register Offset : 0x8004
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_SESRAM_SIZE                  (REG_AP_APB_CRYPTOENGINE_SESRAM_SIZE)
#define CE3_RAM_SIZE_SHIFT                  SESRAM_SIZE_CE3_RAM_SIZE_FIELD_OFFSET
#define CE3_RAM_SIZE_MASK                   0x7F << CE3_RAM_SIZE_SHIFT
#define CE2_RAM_SIZE_SHIFT                  SESRAM_SIZE_CE2_RAM_SIZE_FIELD_OFFSET
#define CE2_RAM_SIZE_MASK                   0x7F << CE2_RAM_SIZE_SHIFT
#define CE1_RAM_SIZE_SHIFT                  SESRAM_SIZE_CE1_RAM_SIZE_FIELD_OFFSET
#define CE1_RAM_SIZE_MASK                   0x7F << CE1_RAM_SIZE_SHIFT
#define CE0_RAM_SIZE_SHIFT                  SESRAM_SIZE_CE0_RAM_SIZE_FIELD_OFFSET
#define CE0_RAM_SIZE_MASK                   0x7F << CE0_RAM_SIZE_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_SESRAM_SIZE_CONT
// Register Offset : 0x8008
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_SESRAM_SIZE_CONT             (REG_AP_APB_CRYPTOENGINE_SESRAM_SIZE_CONT)
#define CE7_RAM_SIZE_SHIFT                  SESRAM_SIZE_CONT_CE7_RAM_SIZE_FIELD_OFFSET
#define CE7_RAM_SIZE_MASK                   0x7F << CE7_RAM_SIZE_SHIFT
#define CE6_RAM_SIZE_SHIFT                  SESRAM_SIZE_CONT_CE6_RAM_SIZE_FIELD_OFFSET
#define CE6_RAM_SIZE_MASK                   0x7F << CE6_RAM_SIZE_SHIFT
#define CE5_RAM_SIZE_SHIFT                  SESRAM_SIZE_CONT_CE5_RAM_SIZE_FIELD_OFFSET
#define CE5_RAM_SIZE_MASK                   0x7F << CE5_RAM_SIZE_SHIFT
#define CE4_RAM_SIZE_SHIFT                  SESRAM_SIZE_CONT_CE4_RAM_SIZE_FIELD_OFFSET
#define CE4_RAM_SIZE_MASK                   0x7F << CE4_RAM_SIZE_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_SESRAM_SASIZE
// Register Offset : 0x800c
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_SESRAM_SASIZE                (REG_AP_APB_CRYPTOENGINE_SESRAM_SASIZE)
#define CE3_RAM_SASIZE_SHIFT                SESRAM_SASIZE_CE3_RAM_SASIZE_FIELD_OFFSET
#define CE3_RAM_SASIZE_MASK                 0x7F << CE3_RAM_SASIZE_SHIFT
#define CE2_RAM_SASIZE_SHIFT                SESRAM_SASIZE_CE2_RAM_SASIZE_FIELD_OFFSET
#define CE2_RAM_SASIZE_MASK                 0x7F << CE2_RAM_SASIZE_SHIFT
#define CE1_RAM_SASIZE_SHIFT                SESRAM_SASIZE_CE1_RAM_SASIZE_FIELD_OFFSET
#define CE1_RAM_SASIZE_MASK                 0x7F << CE1_RAM_SASIZE_SHIFT
#define CE0_RAM_SASIZE_SHIFT                SESRAM_SASIZE_CE0_RAM_SASIZE_FIELD_OFFSET
#define CE0_RAM_SASIZE_MASK                 0x7F << CE0_RAM_SASIZE_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_SESRAM_SASIZE_CONT
// Register Offset : 0x8010
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_SESRAM_SASIZE_CONT           (REG_AP_APB_CRYPTOENGINE_SESRAM_SASIZE_CONT)
#define CE7_RAM_SASIZE_SHIFT                SESRAM_SASIZE_CONT_CE7_RAM_SASIZE_FIELD_OFFSET
#define CE7_RAM_SASIZE_MASK                 0x7F << CE7_RAM_SASIZE_SHIFT
#define CE6_RAM_SASIZE_SHIFT                SESRAM_SASIZE_CONT_CE6_RAM_SASIZE_FIELD_OFFSET
#define CE6_RAM_SASIZE_MASK                 0x7F << CE6_RAM_SASIZE_SHIFT
#define CE5_RAM_SASIZE_SHIFT                SESRAM_SASIZE_CONT_CE5_RAM_SASIZE_FIELD_OFFSET
#define CE5_RAM_SASIZE_MASK                 0x7F << CE5_RAM_SASIZE_SHIFT
#define CE4_RAM_SASIZE_SHIFT                SESRAM_SASIZE_CONT_CE4_RAM_SASIZE_FIELD_OFFSET
#define CE4_RAM_SASIZE_MASK                 0x7F << CE4_RAM_SASIZE_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_VCE_SINATURE_MASK
// Register Offset : 0x8014
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_VCE_SINATURE_MASK            (REG_AP_APB_CRYPTOENGINE_VCE_SINATURE_MASK)
#define VCE_SINATURE_MASK_SHIFT             VCE_SINATURE_MASK_VCE_SINATURE_MASK_FIELD_OFFSET
#define VCE_SINATURE_MASK_MASK              0xFFFFFFFF << VCE_SINATURE_MASK_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_HRNG_CTRL
// Register Offset : 0x8020
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_HRNG_CTRL                    (REG_AP_APB_CRYPTOENGINE_HRNG_CTRL)
#define HRNG_LOAD_CNT_SHIFT                 HRNG_CTRL_HRNG_LOAD_CNT_FIELD_OFFSET
#define HRNG_LOAD_CNT_MASK                  0xF << HRNG_LOAD_CNT_SHIFT
#define NOISE_ENABLE_SHIFT                  HRNG_CTRL_NOISE_ENABLE_FIELD_OFFSET
#define NOISE_ENABLE_MASK                   1UL << NOISE_ENABLE_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_HRNG_SEED_CTRL
// Register Offset : 0x8024
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_HRNG_SEED_CTRL               (REG_AP_APB_CRYPTOENGINE_HRNG_SEED_CTRL)
#define HRNG_LOAD_SEED_GO_SHIFT             HRNG_SEED_CTRL_LOAD_SEED_GO_FIELD_OFFSET
#define HRNG_LOAD_SEED_GO_MASK              1UL << HRNG_LOAD_SEED_GO_SHIFT
#define HRNG_SEED_NUM_SHIFT                 HRNG_SEED_CTRL_SEED_NUM_FIELD_OFFSET
#define HRNG_SEED_NUM_MASK                  0x7FFFFFFF << HRNG_SEED_NUM_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_TRNG_CTRL
// Register Offset : 0x8030
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_TRNG_CTRL                    (REG_AP_APB_CRYPTOENGINE_TRNG_CTRL)
#define TRNG_SEQ_GO_SHIFT                   TRNG_CTRL_CE_TRNG_SEQ_GO_FIELD_OFFSET
#define TRNG_SEQ_GO_MASK                    1UL << TRNG_SEQ_GO_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_TRNG_CONTROL
// Register Offset : 0x8100
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_TRNG_CONTROL                 (REG_AP_APB_CRYPTOENGINE_TRNG_CONTROL)
#define TRNG_FIFOWRITESTARTUP_SHIFT         TRNG_CONTROL_TRNG_FIFOWRITESTARTUP_FIELD_OFFSET
#define TRNG_FIFOWRITESTARTUP_MASK          1UL << TRNG_FIFOWRITESTARTUP_SHIFT
#define TRNG_NB128BITBLOCKS_SHIFT           TRNG_CONTROL_TRNG_NB128BITBLOCKS_FIELD_OFFSET
#define TRNG_NB128BITBLOCKS_MASK            0xF << TRNG_NB128BITBLOCKS_SHIFT
#define TRNG_AIS31TESTSEL_SHIFT             TRNG_CONTROL_TRNG_AIS31TESTSEL_FIELD_OFFSET
#define TRNG_AIS31TESTSEL_MASK              1UL << TRNG_AIS31TESTSEL_SHIFT
#define TRNG_HEALTHTESTSEL_SHIFT            TRNG_CONTROL_TRNG_HEALTHTESTSEL_FIELD_OFFSET
#define TRNG_HEALTHTESTSEL_MASK             1UL << TRNG_HEALTHTESTSEL_SHIFT
#define TRNG_AIS31BYPASS_SHIFT              TRNG_CONTROL_TRNG_AIS31BYPASS_FIELD_OFFSET
#define TRNG_AIS31BYPASS_MASK               1UL << TRNG_AIS31BYPASS_SHIFT
#define TRNG_HEALTHTESTBYPASS_SHIFT         TRNG_CONTROL_TRNG_HEALTHTESTBYPASS_FIELD_OFFSET
#define TRNG_HEALTHTESTBYPASS_MASK          1UL << TRNG_HEALTHTESTBYPASS_SHIFT
#define TRNG_FORCERUN_SHIFT                 TRNG_CONTROL_TRNG_FORCERUN_FIELD_OFFSET
#define TRNG_FORCERUN_MASK                  1UL << TRNG_FORCERUN_SHIFT
#define TRNG_INTENALM_SHIFT                 TRNG_CONTROL_TRNG_INTENALM_FIELD_OFFSET
#define TRNG_INTENALM_MASK                  1UL << TRNG_INTENALM_SHIFT
#define TRNG_INTENPRE_SHIFT                 TRNG_CONTROL_TRNG_INTENPRE_FIELD_OFFSET
#define TRNG_INTENPRE_MASK                  1UL << TRNG_INTENPRE_SHIFT
#define TRNG_SOFTRESET_SHIFT                TRNG_CONTROL_TRNG_SOFTRESET_FIELD_OFFSET
#define TRNG_SOFTRESET_MASK                 1UL << TRNG_SOFTRESET_SHIFT
#define TRNG_INTENFULL_SHIFT                TRNG_CONTROL_TRNG_INTENFULL_FIELD_OFFSET
#define TRNG_INTENFULL_MASK                 1UL << TRNG_INTENFULL_SHIFT
#define TRNG_INTENPROP_SHIFT                TRNG_CONTROL_TRNG_INTENPROP_FIELD_OFFSET
#define TRNG_INTENPROP_MASK                 1UL << TRNG_INTENPROP_SHIFT
#define TRNG_INTENREP_SHIFT                 TRNG_CONTROL_TRNG_INTENREP_FIELD_OFFSET
#define TRNG_INTENREP_MASK                  1UL << TRNG_INTENREP_SHIFT
#define TRNG_CONDBYPASS_SHIFT               TRNG_CONTROL_TRNG_CONDBYPASS_FIELD_OFFSET
#define TRNG_CONDBYPASS_MASK                1UL << TRNG_CONDBYPASS_SHIFT
#define TRNG_TESTEN_SHIFT                   TRNG_CONTROL_TRNG_TESTEN_FIELD_OFFSET
#define TRNG_TESTEN_MASK                    1UL << TRNG_TESTEN_SHIFT
#define TRNG_LFSREN_SHIFT                   TRNG_CONTROL_TRNG_LFSREN_FIELD_OFFSET
#define TRNG_LFSREN_MASK                    1UL << TRNG_LFSREN_SHIFT
#define TRNG_ENABLE_SHIFT                   TRNG_CONTROL_TRNG_ENABLE_FIELD_OFFSET
#define TRNG_ENABLE_MASK                    1UL << TRNG_ENABLE_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_TRNG_FIFOLEVEL
// Register Offset : 0x8104
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_TRNG_FIFOLEVEL               (REG_AP_APB_CRYPTOENGINE_TRNG_FIFOLEVEL)
#define TRNG_FIFOTHD_SHIFT                  TRNG_FIFOLEVEL_TRNG_FIFOTHD_FIELD_OFFSET
#define TRNG_FIFOTHD_MASK                   0xFFFFFFFF << TRNG_FIFOTHD_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_TRNG_FIFODEPTH
// Register Offset : 0x810c
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_TRNG_FIFODEPTH               (REG_AP_APB_CRYPTOENGINE_TRNG_FIFODEPTH)
#define TRNG_FIFODEPTH_SHIFT                TRNG_FIFODEPTH_TRNG_FIFODEPTH_FIELD_OFFSET
#define TRNG_FIFODEPTH_MASK                 0xFFFFFFFF << TRNG_FIFODEPTH_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_TRNG_KEY0REG
// Register Offset : 0x8110
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_TRNG_KEY0REG                 (REG_AP_APB_CRYPTOENGINE_TRNG_KEY0REG)
#define TRNG_KEY0REG_SHIFT                  TRNG_KEY0REG_TRNG_KEY0REG_FIELD_OFFSET
#define TRNG_KEY0REG_MASK                   0xFFFFFFFF << TRNG_KEY0REG_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_TRNG_KEY1REG
// Register Offset : 0x8114
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_TRNG_KEY1REG                 (REG_AP_APB_CRYPTOENGINE_TRNG_KEY1REG)
#define TRNG_KEY1REG_SHIFT                  TRNG_KEY1REG_TRNG_KEY1REG_FIELD_OFFSET
#define TRNG_KEY1REG_MASK                   0xFFFFFFFF << TRNG_KEY1REG_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_TRNG_KEY2REG
// Register Offset : 0x8118
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_TRNG_KEY2REG                 (REG_AP_APB_CRYPTOENGINE_TRNG_KEY2REG)
#define TRNG_KEY2REG_SHIFT                  TRNG_KEY2REG_TRNG_KEY2REG_FIELD_OFFSET
#define TRNG_KEY2REG_MASK 3                 0xFFFFFFFF << TRNG_KEY2REG_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_TRNG_KEY3REG
// Register Offset : 0x811c
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_TRNG_KEY3REG                 (REG_AP_APB_CRYPTOENGINE_TRNG_KEY3REG)
#define TRNG_KEY3REG_SHIFT                  TRNG_KEY3REG_TRNG_KEY3REG_FIELD_OFFSET
#define TRNG_KEY3REG_MASK                   0xFFFFFFFF << TRNG_KEY3REG_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_TRNG_TESTDATA
// Register Offset : 0x8120
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_TRNG_TESTDATA                (REG_AP_APB_CRYPTOENGINE_TRNG_TESTDATA)
#define TRNG_TESTDATA_SHIFT                 TRNG_TESTDATA_TRNG_TESTDATA_FIELD_OFFSET
#define TRNG_TESTDATA_MASK                  0xFFFFFFFF << TRNG_TESTDATA_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_TRNG_REPTHRESREG
// Register Offset : 0x8124
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_TRNG_REPTHRESREG             (REG_AP_APB_CRYPTOENGINE_TRNG_REPTHRESREG)
#define TRNG_REPTHRESREG_SHIFT              TRNG_REPTHRESREG_TRNG_REPTHRESREG_FIELD_OFFSET
#define TRNG_REPTHRESREG_MASK               0xFFFFFFFF << TRNG_REPTHRESREG_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_TRNG_PROPTHRESREG
// Register Offset : 0x8128
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_TRNG_PROPTHRESREG            (REG_AP_APB_CRYPTOENGINE_TRNG_PROPTHRESREG)
#define TRNG_PROPTHRESREG_SHIFT             TRNG_PROPTHRESREG_TRNG_PROPTHRESREG_FIELD_OFFSET
#define TRNG_PROPTHRESREG_MASK              0xFFFFFFFF << TRNG_PROPTHRESREG_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_TRNG_STATUS
// Register Offset : 0x8130
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_TRNG_STATUS                  (REG_AP_APB_CRYPTOENGINE_TRNG_STATUS)
#define TRNG_FIFOACCFAIL_SHIFT              TRNG_STATUS_TRNG_FIFOACCFAIL_FIELD_OFFSET
#define TRNG_FIFOACCFAIL_MASK               1UL << TRNG_FIFOACCFAIL_SHIFT
#define TRNG_STARTUPFAIL_SHIFT              TRNG_STATUS_TRNG_STARTUPFAIL_FIELD_OFFSET
#define TRNG_STARTUPFAIL_MASK               1UL << TRNG_STARTUPFAIL_SHIFT
#define TRNG_ALMINT_SHIFT                   TRNG_STATUS_TRNG_ALMINT_FIELD_OFFSET
#define TRNG_ALMINT_MASK                    1UL << TRNG_ALMINT_SHIFT
#define TRNG_PREINT_SHIFT                   TRNG_STATUS_TRNG_PREINT_FIELD_OFFSET
#define TRNG_PREINT_MASK                    1UL << TRNG_PREINT_SHIFT
#define TRNG_FULLINT_SHIFT                  TRNG_STATUS_TRNG_FULLINT_FIELD_OFFSET
#define TRNG_FULLINT_MASK                   1UL << TRNG_FULLINT_SHIFT
#define TRNG_PROPFAIL_SHIFT                 TRNG_STATUS_TRNG_PROPFAIL_FIELD_OFFSET
#define TRNG_PROPFAIL_MASK                  1UL << TRNG_PROPFAIL_SHIFT
#define TRNG_REPFAIL_SHIFT                  TRNG_STATUS_TRNG_REPFAIL_FIELD_OFFSET
#define TRNG_REPFAIL_MASK                   1UL << TRNG_REPFAIL_SHIFT
#define TRNG_STATE_SHIFT                    TRNG_STATUS_TRNG_STATE_FIELD_OFFSET
#define TRNG_STATE_MASK                     7UL << TRNG_STATE_SHIFT
#define TRNG_TESTDATABUSY_SHIFT             TRNG_STATUS_TRNG_TESTDATABUSY_FIELD_OFFSET
#define TRNG_TESTDATABUSY_MASK              1UL << TRNG_TESTDATABUSY_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_TRNG_INITWAITVAL
// Register Offset : 0x8134
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_TRNG_INITWAITVAL             (REG_AP_APB_CRYPTOENGINE_TRNG_INITWAITVAL)
#define TRNG_INITWAITVAL_SHIFT              TRNG_INITWAITVAL_TRNG_INITWAITVAL_FIELD_OFFSET
#define TRNG_INITWAITVAL_MASK               0xFFFF << TRNG_INITWAITVAL_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_TRNG_DISABLEOSC
// Register Offset : 0x8138
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_TRNG_DISABLEOSC              (REG_AP_APB_CRYPTOENGINE_TRNG_DISABLEOSC)
#define TRNG_DISABLEOS0_SHIFT               TRNG_DISABLEOSC_TRNG_DISABLEOS0_FIELD_OFFSET
#define TRNG_DISABLEOS0_MASK                0xFFFFFFFF << TRNG_DISABLEOS0_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_TRNG_DISABLEOSC1
// Register Offset : 0x813c
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_TRNG_DISABLEOSC1             (REG_AP_APB_CRYPTOENGINE_TRNG_DISABLEOSC1)
#define TRNG_DISABLEOS1_SHIFT               TRNG_DISABLEOSC1_TRNG_DISABLEOS1_FIELD_OFFSET
#define TRNG_DISABLEOS1_MASK                0xFFFFFFFF << TRNG_DISABLEOS1_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_TRNG_SWOFFTMRVAL
// Register Offset : 0x8140
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_TRNG_SWOFFTMRVAL             (REG_AP_APB_CRYPTOENGINE_TRNG_SWOFFTMRVAL)
#define TRNG_SWOFFTMRVAL_SHIFT              TRNG_SWOFFTMRVAL_TRNG_SWOFFTMRVAL_FIELD_OFFSET
#define TRNG_SWOFFTMRVAL_MASK               0xFFFF << TRNG_SWOFFTMRVAL_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_TRNG_CLKDIV
// Register Offset : 0x8144
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_TRNG_CLKDIV                  (REG_AP_APB_CRYPTOENGINE_TRNG_CLKDIV)
#define TRNG_CLKDIV_SHIFT                   TRNG_CLKDIV_TRNG_CLKDIV_FIELD_OFFSET
#define TRNG_CLKDIV_MASK                    0xFF << TRNG_CLKDIV_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_TRNG_AIS31CONF0
// Register Offset : 0x8148
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_TRNG_AIS31CONF0              (REG_AP_APB_CRYPTOENGINE_TRNG_AIS31CONF0)
#define TRNG_ONLINETHRESH_SHIFT             TRNG_AIS31CONF0_TRNG_ONLINETHRESH_FIELD_OFFSET
#define TRNG_ONLINETHRESH_MASK              0xFFFF << TRNG_ONLINETHRESH_SHIFT
#define TRNG_STARTUPTHRESH_SHIF             TRNG_AIS31CONF0_TRNG_STARTUPTHRESH_FIELD_OFFSET
#define TRNG_STARTUPTHRESH_MASK             0xFFFF << TRNG_STARTUPTHRESH_SHIF

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_TRNG_AIS31CONF1
// Register Offset : 0x814c
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_TRNG_AIS31CONF1              (REG_AP_APB_CRYPTOENGINE_TRNG_AIS31CONF1)
#define TRNG_HEXPECTEDVALUE_SHIFT           TRNG_AIS31CONF1_TRNG_HEXPECTEDVALUE_FIELD_OFFSET
#define TRNG_HEXPECTEDVALUE_MASK            0xFFFF << TRNG_HEXPECTEDVALUE_SHIFT
#define TRNG_ONLINEREPTHRESH_SHIFT          TRNG_AIS31CONF1_TRNG_ONLINEREPTHRESH_FIELD_OFFSET
#define TRNG_ONLINEREPTHRESH_MASK           0xFFFF << TRNG_ONLINEREPTHRESH_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_TRNG_AIS31CONF2
// Register Offset : 0x8150
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_TRNG_AIS31CONF2              (REG_AP_APB_CRYPTOENGINE_TRNG_AIS31CONF2)
#define TRNG_HMAX_SHIFT                     TRNG_AIS31CONF2_TRNG_HMAX_FIELD_OFFSET
#define TRNG_HMAX_MASK                      0xFFFF << TRNG_HMAX_SHIFT
#define TRNG_HMIN_SHIFT                     TRNG_AIS31CONF2_TRNG_HMIN_FIELD_OFFSET
#define TRNG_HMIN_MASK                      0xFFFF << TRNG_HMIN_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_TRNG_AIS31STATUS
// Register Offset : 0x8154
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_TRNG_AIS31STATUS             (REG_AP_APB_CRYPTOENGINE_TRNG_AIS31STATUS)
#define TRNG_PRELIMNOISEALARMREP_SHIFT      TRNG_AIS31STATUS_TRNG_PRELIMNOISEALARMREP_FIELD_OFFSET
#define TRNG_PRELIMNOISEALARMREP_MASK       1UL << TRNG_PRELIMNOISEALARMREP_SHIFT
#define TRNG_PRELIMNOISEALARMRNG_SHIFT      TRNG_AIS31STATUS_TRNG_PRELIMNOISEALARMRNG_FIELD_OFFSET
#define TRNG_PRELIMNOISEALARMRNG_MASK       1UL << TRNG_PRELIMNOISEALARMRNG_SHIFT
#define TRNG_NUMPRELIMALARMS_SHIFT          TRNG_AIS31STATUS_TRNG_NUMPRELIMALARMS_FIELD_OFFSET
#define TRNG_NUMPRELIMALARMS_MASK           0xFFFF << TRNG_NUMPRELIMALARMS_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_TRNG_HWCONF
// Register Offset : 0x8158
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_TRNG_HWCONF                  (REG_AP_APB_CRYPTOENGINE_TRNG_HWCONF)
#define TRNG_AIS31FULL_SHIFT                TRNG_HWCONF_TRNG_AIS31FULL_FIELD_OFFSET
#define TRNG_AIS31FULL_MASK                 1UL << TRNG_AIS31FULL_SHIFT
#define TRNG_AIS31_SHIFT                    TRNG_HWCONF_TRNG_AIS31_FIELD_OFFSET
#define TRNG_AIS31_MASK                     1UL << TRNG_AIS31_SHIFT
#define TRNG_NUMBOFRINGS_SHIFT              TRNG_HWCONF_TRNG_NUMBOFRINGS_FIELD_OFFSET
#define TRNG_NUMBOFRINGS_MASK               0xFF << TRNG_NUMBOFRINGS_SHIFT

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_TRNG_HWCONF
// Register Offset : 0x8180
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_TRNG_RESERVE_MEM                 (REG_AP_APB_CRYPTOENGINE_TRNG_RESERVE_MEM)
#define REG_CE_TRNG_RESERVE_MEM_SIZE 32
#endif
