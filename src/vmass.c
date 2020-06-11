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

#define SIZE_2MiB   0x200000
#define SIZE_4MiB   0x400000
#define SIZE_10MiB  0xA00000
#define SIZE_16MiB 0x1000000

SceSize g_vmass_size;

typedef struct VmassPageAllocInfo {
	unsigned int mem_type;
	unsigned int paddr;
	SceSize size;
} VmassPageAllocInfo;

const VmassPageAllocInfo page_alloc_list[] = {
	// ScePhyMemPartGameCdram
	{0x40408006, 0x00000000, SIZE_10MiB},

	// ScePhyMemPartPhyCont
	{0x30808006, 0x00000000, SIZE_4MiB},

	// SceDisplay
	{0x10208006, 0x1C000000, SIZE_2MiB}
};

#define VMASS_PAGE_NUM (sizeof(page_alloc_list) / sizeof(VmassPageAllocInfo))

void *membase[VMASS_PAGE_NUM];

SceKernelLwMutexWork lw_mtx;

int _vmassGetDevInfo(SceUsbMassDevInfo *pInfo){

	if(pInfo == NULL)
		return -1;

	pInfo->sector_size          = 0x200;
	pInfo->data_04              = 0;
	pInfo->number_of_all_sector = g_vmass_size >> 9;
	pInfo->data_0C              = 0;

	return 0;
}

int _vmassReadSector(SceSize sector_pos, void *data, SceSize sector_num){

	int page_idx = 0;
	SceSize off = (sector_pos << 9), size = (sector_num << 9), work_size;

	if((size == 0) || ((off + size) > g_vmass_size) || (off >= g_vmass_size) || (size > g_vmass_size))
		return -1;

	while(off >= page_alloc_list[page_idx].size){
		off -= page_alloc_list[page_idx].size;
		page_idx++;
	}

	if(off != 0){
		work_size = page_alloc_list[page_idx].size - off;
		if(work_size > size)
			work_size = size;

		memcpy(data, membase[page_idx] + off, work_size);
		size -= work_size;
		data += work_size;
		off = 0;
		page_idx++;
	}

	while(size != 0){
		work_size = (size > page_alloc_list[page_idx].size) ? page_alloc_list[page_idx].size : size;

		memcpy(data, membase[page_idx], work_size);
		size -= work_size;
		data += work_size;
		page_idx++;
	}

	return 0;
}

int _vmassWriteSector(SceSize sector_pos, const void *data, SceSize sector_num){

	int page_idx = 0;
	SceSize off = (sector_pos << 9), size = (sector_num << 9), work_size;

	if((size == 0) || ((off + size) > g_vmass_size) || (off >= g_vmass_size) || (size > g_vmass_size))
		return -1;

	while(off >= page_alloc_list[page_idx].size){
		off -= page_alloc_list[page_idx].size;
		page_idx++;
	}

	if(off != 0){
		work_size = page_alloc_list[page_idx].size - off;
		if(work_size > size)
			work_size = size;

		memcpy(membase[page_idx] + off, data, work_size);
		size -= work_size;
		data += work_size;
		off = 0;
		page_idx++;
	}

	while(size != 0){
		work_size = (size > page_alloc_list[page_idx].size) ? page_alloc_list[page_idx].size : size;

		memcpy(membase[page_idx], data, work_size);
		size -= work_size;
		data += work_size;
		page_idx++;
	}

	return 0;
}

int vmassGetDevInfo(SceUsbMassDevInfo *info){

	int res;

	ksceKernelLockFastMutex(&lw_mtx);

	res = _vmassGetDevInfo(info);

	ksceKernelUnlockFastMutex(&lw_mtx);

	return res;
}

int vmassReadSector(SceSize sector_pos, void *data, SceSize sector_num){

	int res;

	ksceKernelLockFastMutex(&lw_mtx);

	res = _vmassReadSector(sector_pos, data, sector_num);

	ksceKernelUnlockFastMutex(&lw_mtx);

	return res;
}

int vmassWriteSector(SceSize sector_pos, const void *data, SceSize sector_num){

	int res;

	ksceKernelLockFastMutex(&lw_mtx);

	res = _vmassWriteSector(sector_pos, data, sector_num);

	ksceKernelUnlockFastMutex(&lw_mtx);

	return res;
}

// Load umass.skprx to avoid taiHEN's unlinked problem
int load_umass(void){

	int res;
	SceUID umass_modid, patch_uid;

	// Since bootimage cannot be mounted after the second time, load vitashell's umass.skprx
	umass_modid = ksceKernelLoadModule("ux0:VitaShell/module/umass.skprx", 0, NULL);
	if(umass_modid < 0)
		return umass_modid;

	patch_uid = taiInjectDataForKernel(0x10005, umass_modid, 0, 0x1546, umass_start_byepass_patch, sizeof(umass_start_byepass_patch) - 2);

	int start_res = -1;
	res = ksceKernelStartModule(umass_modid, 0, NULL, 0, NULL, &start_res);
	if(res >= 0)
		res = start_res;

	taiInjectReleaseForKernel(patch_uid);

	return res;
}

int vmassAllocStoragePage(void){

	SceUID memid;
	SceKernelAllocMemBlockKernelOpt opt, *pOpt;

	for(int i=0;i<VMASS_PAGE_NUM;i++){

		pOpt = NULL;

		if(page_alloc_list[i].paddr != 0){
			memset(&opt, 0, sizeof(opt));
			opt.size = sizeof(opt);
			opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
			opt.paddr = page_alloc_list[i].paddr;
			pOpt = &opt;
		}

		memid = ksceKernelAllocMemBlock("VmassStoragePage", page_alloc_list[i].mem_type, page_alloc_list[i].size, pOpt);
		if(memid < 0){

			ksceDebugPrintf("failed 0x%X\n", i);

			i = VMASS_PAGE_NUM;

			do {
				i--;
				if(membase[i] != NULL)
					ksceKernelFreeMemBlock(ksceKernelFindMemBlockByAddr(membase[i], 0));
			} while(i != 0);

			return memid;
		}

		ksceKernelGetMemBlockBase(memid, &membase[i]);

		g_vmass_size += page_alloc_list[i].size;
	}

	return 0;
}

int vmassInit(void){

	int res, buf[0x200 >> 2];
	FatHeader fat_header;

	res = ksceKernelInitializeFastMutex(&lw_mtx, "VmassMutex", 0, 0);
	if(res < 0)
		return res;

	res = vmassAllocStoragePage();
	if(res < 0)
		return res;

	memset(buf, 0, 0x200);

	if(g_vmass_size >= SIZE_16MiB){

		buf[0] = 0xFFFFFFF8;

		setFat16Header(&fat_header, g_vmass_size >> 9);
	}else{

		buf[0] = 0xFFFFF8;

		setFat12Header(&fat_header, g_vmass_size >> 9);
	}

	vmassWriteSector(0, &fat_header, 1);

	// write first file/dir entry
	vmassWriteSector(2, buf, 1);
	vmassWriteSector(fat_header.fat_base.fat_size_16 + 2, buf, 1);

	res = load_umass();

	return res;
}
