/**************************************************************************/ /*!
@File
@Title          Software Command Processor header
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Defines the interface for the software command processor
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /***************************************************************************/

#ifndef SCP_H
#define SCP_H

#include "img_types.h"
#include "img_defs.h"
#include "pvrsrv_error.h"
#include "sync_server.h"


typedef struct _SCP_CONTEXT_ SCP_CONTEXT;	/*!< Opaque handle to a software command processor context */

typedef IMG_BOOL (*SCPReady)(void *pvReadyData);
typedef void (*SCPDo)(void *pvReadyData, void *pvCompleteData);

/*************************************************************************/ /*!
@Function       SCPCreate

@Description    Create a software command processor

@Input          psDevNode            Device node pointer

@Input          ui32CCBSizeLog2      Log2 of the CCB size

@Output         ppsContext           Pointer to SCP context created

@Return         PVRSRV_OK if the software command processor was created
*/
/*****************************************************************************/
PVRSRV_ERROR SCPCreate(PVRSRV_DEVICE_NODE *psDevNode,
									IMG_UINT32 ui32CCBSizeLog2,
									SCP_CONTEXT **ppsContext);

/*************************************************************************/ /*!
@Function       SCPAllocCommand

@Description    Allocate space in the software command processor and return
                the data pointers for the callback data.

                Once any command ready data and command complete have been setup
                the command can be submitted for processing by calling
                SCPSubmitCommand.

                When any fences the command has have been meet then the command
                ready callback will be called with the command ready data.
                Once the command has completed the command complete callback will
                be called with the command complete data.

@Input          psSCPContext            Context to allocate from

@Input          ui32SyncPrimCount       Number of Sync Prim operations

@Input          papsSync                Pointer to array of pointers to server syncs

@Input          iAcquireFence           The fence that must be signalled
                                        before the command will be actioned

@Input          pfnCommandReady         Callback to call if the command is ready

@Input          pfnCommandDo            Callback to the function to run

@Input          ui32ReadyDataSize       Size of command ready data to allocate in bytes

@Input          pfnCommandComplete      Callback to call when the command has completed

@Input          ui32CompleteDataSize    Size of command complete data to allocate

@Output         ppvReadyData            Pointer to memory allocated for command
                                        ready callback data

@Output         ppvCompleteData         Pointer to memory allocated for command
                                        complete callback data

@Input          iReleaseFenceTimeline   SW Timeline on which the release fence
                                        should be created

@Output         piReleaseFence          The release fence that is signalled
                                        when the command has been released

@Return         PVRSRV_OK if the allocate was successful
*/
/*****************************************************************************/
PVRSRV_ERROR SCPAllocCommand(SCP_CONTEXT *psSCPContext,
										  PVRSRV_FENCE iAcquireFence,
										  SCPReady pfnCommandReady,
										  SCPDo pfnCommandDo,
										  size_t ui32ReadyDataByteSize,
										  size_t ui32CompleteDataByteSize,
										  void **ppvReadyData,
										  void **ppvCompleteData,
										  PVRSRV_TIMELINE iReleaseFenceTimeline,
										  PVRSRV_FENCE *piReleaseFence);

/*************************************************************************/ /*!
@Function       SCPSubmitCommand

@Description    Submit a command for processing. We don't actually try to
                run the command in this call as it might not be valid to do
                from the same thread that this function is being called from

@Input          psSCPContext            Context to allocate on which to submit
                                        the command

@Return         PVRSRV_OK if the command was submitted
*/
/*****************************************************************************/
PVRSRV_ERROR SCPSubmitCommand(SCP_CONTEXT *psContext);


/*************************************************************************/ /*!
@Function       SCPRun

@Description    Run the software command processor to see if any commands are
                now ready.

@Input          psSCPContext            Context to process

@Return         PVRSRV_OK if the software command processor was run
*/
/*****************************************************************************/
PVRSRV_ERROR SCPRun(SCP_CONTEXT *psContext);

/*************************************************************************/ /*!
@Function       SCPCommandComplete

@Description    Complete a command which the software command processor
                has previously issued.
                Note: Commands _MUST_ be completed in order

@Input          psSCPContext            Context to process

@Input          bIgnoreFences           Do not respect any fence checks.

@Return         PVRSRV_OK if the software command processor was run
*/
/*****************************************************************************/
void SCPCommandComplete(SCP_CONTEXT *psContext,
                        IMG_BOOL bIgnoreFences);

/*************************************************************************/ /*!
@Function       SCPFlush

@Description    Flush the software command processor.

@Input          psSCPContext            Context to process

@Return         PVRSRV_OK if all commands have been completed, otherwise
				PVRSRV_ERROR_RETRY
*/
/*****************************************************************************/
PVRSRV_ERROR SCPFlush(SCP_CONTEXT *psContext);

/*************************************************************************/ /*!
@Function       SCPHasPendingCommand

@Description    Check the software command processor for pending commands.

@Input          psContext               Context to process

@Return         IMG_TRUE if there is at least one pending command
				IMG_FALSE if there are no pending commands
*/
/*****************************************************************************/
IMG_BOOL SCPHasPendingCommand(SCP_CONTEXT *psContext);

/*************************************************************************/ /*!
@Function       SCPDumpStatus

@Description    Dump the status of the provided software command processor.

@Input          psSCPContext            Context to dump

@Input          pfnDumpDebugPrintf      Debug print function

@Return         None
*/
/*****************************************************************************/
void SCPDumpStatus(SCP_CONTEXT *psContext,
				DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
				void *pvDumpDebugFile);

/*************************************************************************/ /*!
@Function       SCPDestroy

@Description    Destroy a software command processor.

@Input          psSCPContext            Context to destroy

@Return         None
*/
/*****************************************************************************/
void SCPDestroy(SCP_CONTEXT *psContext);


#endif /* SCP_H */

/******************************************************************************
 End of file (queue.h)
******************************************************************************/
