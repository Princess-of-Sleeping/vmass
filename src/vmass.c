/*
 * PlayStation(R)Vita Virtual Mass
 * Copyright (C) 2020 Princess of Slepping
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
#include <taihen.h>
#include <string.h>
#include "vmass.h"
#include "fat.h"

const char umass_start_byepass_patch[] = {
	0x00, 0xBF, 0x00, 0xBF,
	0x00, 0xBF,
	0x00, 0xBF, 0x00, 0xBF,
	0x00, 0xBF,
	0x00, 0x20, 0x00, 0x00
};

#define VMASS_PAGE_SIZE 0x100000
#define VMASS_PAGE_NUM  32

void *membase[VMASS_PAGE_NUM];

SceKernelLwMutexWork lw_mtx;
SceUsbMassDevInfo dev_info;

int vmassGetDevInfo(SceUsbMassDevInfo *info){

	ksceKernelLockFastMutex(&lw_mtx);

	memcpy(info, &dev_info, sizeof(*info));

	ksceKernelUnlockFastMutex(&lw_mtx);

	return 0;
}

int vmassReadSector(SceSize sector_pos, void *data, SceSize sector_num){

	int res = 0;

	int page_idx = 0;
	SceSize off = (sector_pos << 9), size = (sector_num << 9), work_size;

	ksceKernelLockFastMutex(&lw_mtx);

	while(off >= VMASS_PAGE_SIZE){
		page_idx++;
		off -= VMASS_PAGE_SIZE;
	}

	if(off != 0){
		work_size = VMASS_PAGE_SIZE - off;
		if(work_size > size)
			work_size = size;

		memcpy(data, membase[page_idx] + off, work_size);
		size -= work_size;
		data += work_size;
		off = 0;
		page_idx++;
	}

	while(size != 0){
		work_size = (size > VMASS_PAGE_SIZE) ? VMASS_PAGE_SIZE : size;

		memcpy(data, membase[page_idx], work_size);
		size -= work_size;
		data += work_size;
		page_idx++;
	}

	ksceKernelUnlockFastMutex(&lw_mtx);

	return res;
}

int vmassWriteSector(SceSize sector_pos, const void *data, SceSize sector_num){

	int res = 0;

	int page_idx = 0;
	SceSize off = (sector_pos << 9), size = (sector_num << 9), work_size;

	ksceKernelLockFastMutex(&lw_mtx);

	while(off >= VMASS_PAGE_SIZE){
		page_idx++;
		off -= VMASS_PAGE_SIZE;
	}

	if(off != 0){
		work_size = VMASS_PAGE_SIZE - off;
		if(work_size > size)
			work_size = size;

		memcpy(membase[page_idx] + off, data, work_size);
		size -= work_size;
		data += work_size;
		off = 0;
		page_idx++;
	}

	while(size != 0){
		work_size = (size > VMASS_PAGE_SIZE) ? VMASS_PAGE_SIZE : size;

		memcpy(membase[page_idx], data, work_size);
		size -= work_size;
		data += work_size;
		page_idx++;
	}

	ksceKernelUnlockFastMutex(&lw_mtx);

	return res;
}

// Load umass.skprx to avoid taiHEN's unlinked problem
int load_umass(void){

	SceUID umass_modid, patch_uid;

	// Since bootimage cannot be mounted after the second time, load vitashell's umass.skprx
	umass_modid = ksceKernelLoadModule("ux0:VitaShell/module/umass.skprx", 0, NULL);

	patch_uid = taiInjectDataForKernel(0x10005, umass_modid, 0, 0x1546, umass_start_byepass_patch, sizeof(umass_start_byepass_patch) - 2);

	int start_res = -1;
	ksceKernelStartModule(umass_modid, 0, NULL, 0, NULL, &start_res);

	taiInjectReleaseForKernel(patch_uid);

	return 0;
}

int vmassInit(void){

	SceUID memid;
	int buf[0x200 >> 2];
	FatHeader fat_header;

	for(int i=0;i<VMASS_PAGE_NUM;i++){

		memid = ksceKernelAllocMemBlock("VmassStoragePage", 0x40404006, VMASS_PAGE_SIZE, NULL);
		if(memid < 0){

			ksceDebugPrintf("failed 0x%X\n", i);

			do {
				i--;
				ksceKernelFreeMemBlock(ksceKernelFindMemBlockByAddr(membase[i], 0));
			} while(i != 0);

			return memid;
		}

		ksceKernelGetMemBlockBase(memid, &membase[i]);
	}

	memset(&dev_info, 0, sizeof(dev_info));

	dev_info.sector_size = 0x200;
	dev_info.number_of_all_sector = (VMASS_PAGE_SIZE * VMASS_PAGE_NUM) >> 9;

	memset(buf, 0, 0x200);

	buf[0] = 0xFFFFFFF8;

	setFat16Header(&fat_header, dev_info.number_of_all_sector);

	vmassWriteSector(0, &fat_header, 1);

	// write first file/dir entry
	vmassWriteSector(2, buf, 1);
	vmassWriteSector(fat_header.fat_base.fat_size_16 + 2, buf, 1);

	ksceKernelInitializeFastMutex(&lw_mtx, "VmassMtx", 0, 0);

	load_umass();

	return 0;
}
