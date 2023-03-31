#ifndef __RPC_DEFINE_H__
#define __RPC_DEFINE_H__

//generic register using at boot stage for sync status between cores
#define SEC_WRITE_REG (0x38400000 + (0x6c << 10))
#define SAF_WRITE_REG (0x38400000 + (0x68 << 10))
#define AP_WRITE_REG  (0x38400000 + (0x64 << 10))

typedef enum rpc_op_type {
    RPC_NOTIFY = 0,
    RPC_READ = 1,
    RPC_CONFIRM = 2,
    RPC_RESPONSE = 3,
    RPC_OP_MAX
}rpc_op_type;

typedef enum rpc_commands {
    COM_DDR_STATUS = 0,
    COM_PORT_STATUS = 1,
    COM_PLL_CLK_STATUS = 2,
    COM_HANDOVER_STATUS = 3,
    COM_DC_STATUS = 4,
    COM_VDSP_STATUS = 5,
    COM_VPU_STATUS = 6,
    COM_MAX_CNT
}rpc_commands_type;

/* register usage define for secure core: SEC_WRITE_REG
 * bit 0: ddr status, 0 - not inited, 1 - inited
 * bit 1: port status, 0 - not inited, 1 - inited
 * bit 2: pll/clk status, 0 - not inited, 1 - inited
 */

/* register usage define for safety core: SAF_WRITE_REG
 * bit 0: safety core boot status, 0 - not boot or boot fail, 1 - boot safety
 * bit 1: handover status, 0 - not handover, 1 - handover done
 * bit 2: dc status, 0 - can be release, 1 - using
 * bit 3: vdsp status, 0 - can be release, 1 - using
 * bit 4: vpu status, 0 - can be release, 1 - using
 */

/* register usage define for ap core: AP_WRITE_REG
 * bit 0: dc status, 0 - not ready, 1 - wait for dc resources
 * bit 1: vdsp status, 0 - not ready, 1 - wait for vdsp resources
 */

#endif

