/*
 * Virtual mass storage
 * Copyright (C) 2020 Princess of Sleeping
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/iofilemgr.h>
#include "vmass.h"

void _start() __attribute__ ((weak, alias("module_start")));
int module_start(SceSize args, void *argp){

	SceIoStat stat;

	if(ksceKernelSearchModuleByName("SceUsbMass") > 0)
		return SCE_KERNEL_START_NO_RESIDENT;

	if(ksceIoGetstat("uma0:", &stat) == 0)
		return SCE_KERNEL_START_NO_RESIDENT;

	if(vmassInit() < 0)
		return SCE_KERNEL_START_FAILED;

	// if ((FAT_Base *))->all_sector is 0, sceUsbstorVStorStart is return to 0x80244112

	void *data = ksceKernelAllocHeapMemory(0x1000B, 0x200);

	vmassReadSector(0, data, 1);

	if(*(uint32_t *)(data + 0x20) == 0){
		*(uint32_t *)(data + 0x20) = *(uint16_t *)(data + 0x13);
		vmassWriteSector(0, data, 1);
	}

	ksceKernelFreeHeapMemory(0x1000B, data);

	ksceIoMount(0xF00, NULL, 2, 0, 0, 0);

	return SCE_KERNEL_START_SUCCESS;
}
