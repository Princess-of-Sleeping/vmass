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
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/dmac.h>
#include <psp2kern/io/fcntl.h>
#include "sysevent.h"
#include "vmass.h"
#include "vmass_sysevent.h"
#include "fat.h"

#define SIZE_2MiB   0x200000
#define SIZE_4MiB   0x400000
#define SIZE_6MiB   0x600000
#define SIZE_10MiB  0xA00000
#define SIZE_16MiB 0x1000000

typedef struct VmassPageInfo {
	void   *base;
	SceSize size;
} VmassPageInfo;

#define USE_MEMORY_10MiB  1
#define USE_MEMORY_32MiB  0
#define USE_DEVKIT_MEMORY 0

#define DEVKIT_MEM_1MiB  {0x10F0D006, 0x00000000, 0x100000}
#define DEVKIT_MEM_4MiB  DEVKIT_MEM_1MiB, DEVKIT_MEM_1MiB, DEVKIT_MEM_1MiB, DEVKIT_MEM_1MiB
#define DEVKIT_MEM_16MiB DEVKIT_MEM_4MiB, DEVKIT_MEM_4MiB, DEVKIT_MEM_4MiB, DEVKIT_MEM_4MiB

SceUID sysevent_id;
SceSize g_vmass_size;

#define VMASS_PAGE_MAX_NUMBER (0x20)

VmassPageInfo vmass_page_list[VMASS_PAGE_MAX_NUMBER];


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

	while(off >= vmass_page_list[page_idx].size){
		off -= vmass_page_list[page_idx].size;
		page_idx++;
	}

	if(off != 0){
		work_size = vmass_page_list[page_idx].size - off;
		if(work_size > size)
			work_size = size;

		memcpy(data, vmass_page_list[page_idx].base + off, work_size);
		size -= work_size;
		data += work_size;
		off = 0;
		page_idx++;
	}

	while(size != 0){
		work_size = (size > vmass_page_list[page_idx].size) ? vmass_page_list[page_idx].size : size;

		memcpy(data, vmass_page_list[page_idx].base, work_size);
		size -= work_size;
		data += work_size;
		page_idx++;
	}

	return 0;
}

int _vmassWriteSector(SceSize sector_pos, const void *data, SceSize sector_num){

	int page_idx = 0;
	SceSize off = (sector_pos << 9), size = (sector_num << 9), work_size;

	while(off >= vmass_page_list[page_idx].size){
		off -= vmass_page_list[page_idx].size;
		page_idx++;
	}

	if(off != 0){
		work_size = vmass_page_list[page_idx].size - off;
		if(work_size > size)
			work_size = size;

		memcpy(vmass_page_list[page_idx].base + off, data, work_size);
		size -= work_size;
		data += work_size;
		off = 0;
		page_idx++;
	}

	while(size != 0){
		work_size = (size > vmass_page_list[page_idx].size) ? vmass_page_list[page_idx].size : size;

		memcpy(vmass_page_list[page_idx].base, data, work_size);
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

#define VMASS_CAPTURE_SPEED (0)

#if VMASS_CAPTURE_SPEED != 0

#include <psp2kern/kernel/cpu.h>

#define VMASS_PERF_S() SceInt64 time_s, time_e; \
			{ \
			time_s = ksceKernelGetSystemTimeWide(); \
			}

#define VMASS_PERF_E(type, sector_pos, sector) { \
			time_e = ksceKernelGetSystemTimeWide(); \
			int BytePerSecond = 0; \
			getBytePerSecond(time_e - time_s, ((sector) << 9), &BytePerSecond); \
			ksceDebugPrintf("[%-7s] %d:Sector 0x%08X:0x%08X Time %9dMB/s\n", type, ksceKernelCpuGetCpuId(), (sector_pos), (sector), BytePerSecond); \
			}

int getBytePerSecond(int time, int byte, int *dst){

	asm volatile (
		// s0 : time
		// s3 : byte
		"vmov s0, %0\n"
		"vmov s3, %1\n"
		"vcvt.f32.u32 s0, s0\n"
		"vcvt.f32.u32 s3, s3\n"

		// 1000000.0f
		"movw r3, #:lower16:0x49742400\n"
		"movt r3, #:upper16:0x49742400\n"
		"vmov s1, r3\n"

		// 1000.0f
		"movw r3, #:lower16:0x447A0000\n"
		"movt r3, #:upper16:0x447A0000\n"
		"vmov s2, r3\n"

		// s0 = 1000000.0f / time
		"vdiv.f32 s0, s1, s0\n"

		// s0 = byte * s0
		"vmul.f32 s0, s3, s0\n"

		// byte to KB
		"vdiv.f32 s0, s0, s2\n"

		// KB to MB
		"vdiv.f32 s0, s0, s2\n"

		// float to int
		"vcvt.u32.f32 s0, s0\n"
		"vstr s0, [%2]\n"
		:
		: "r"(time), "r"(byte), "r"(dst)
		: "r3", "s0", "s1", "s2", "s3"
	);

	return 0;
}

#else

#define VMASS_PERF_S()
#define VMASS_PERF_E(type, sector_pos, sector)

#endif

#define VMASS_RW_THREAD_PRIORITY_DEF (0x6E)
#define VMASS_RW_THREAD_PRIORITY_WRK (0x28)

#define VMASS_REQ_READ  (1 << 0)
#define VMASS_REQ_WRITE (1 << 1)
#define VMASS_REQ_DONE  (1 << 30)
#define VMASS_REQ_EXIT  (1 << 31)

SceSize g_sector_pos;
SceSize g_sector_num;
void *g_pDataForRead;
const void *g_pDataForWrite;
SceUID thid, evf_id;

int sceVmassRWThread(SceSize args, void *argp){

	int res;
	unsigned int opcode;

	while(1){
		ksceKernelChangeThreadPriority(0, VMASS_RW_THREAD_PRIORITY_DEF);

		opcode = 0;
		res = ksceKernelWaitEventFlag(evf_id, VMASS_REQ_READ | VMASS_REQ_WRITE | VMASS_REQ_EXIT, SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR_PAT, &opcode, NULL);
		if(res < 0)
			continue;

		ksceKernelChangeThreadPriority(0, VMASS_RW_THREAD_PRIORITY_WRK);

		if(opcode == VMASS_REQ_READ){
			_vmassReadSector(g_sector_pos, g_pDataForRead, g_sector_num);
			ksceKernelSetEventFlag(evf_id, VMASS_REQ_DONE);

		}else if(opcode == VMASS_REQ_WRITE){
			_vmassWriteSector(g_sector_pos, g_pDataForWrite, g_sector_num);
			ksceKernelSetEventFlag(evf_id, VMASS_REQ_DONE);

		}else if(opcode == VMASS_REQ_EXIT){
			ksceKernelSetEventFlag(evf_id, VMASS_REQ_DONE);
			break;
		}
	}

	return 0;
}

int vmassReadSector(SceSize sector_pos, void *data, SceSize sector_num){

	int res = 0, s1;

	SceSize off = (sector_pos << 9), size = (sector_num << 9);

	if(((size - 1) > g_vmass_size) || (off >= g_vmass_size) || ((off + size) > g_vmass_size))
		return -1;

	ksceKernelLockFastMutex(&lw_mtx);

	s1 = sector_num & 1;
	sector_num >>= 1;

	VMASS_PERF_S();

	if(sector_num >= 0x50){
		g_sector_pos = sector_pos;
		g_sector_num = sector_num;
		g_pDataForRead = data;
		ksceKernelSetEventFlag(evf_id, VMASS_REQ_READ);
	}else{
		sector_num = (sector_num << 1) + s1;
		s1 = -1;
	}

	if(s1 >= 0){
		_vmassReadSector(sector_pos + sector_num, data + (sector_num << 9), sector_num + s1);

		if(sector_num != 0){
			ksceKernelWaitEventFlag(evf_id, VMASS_REQ_DONE, SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR_PAT, NULL, NULL);
		}
		sector_num = (sector_num << 1) + s1;
	}else{
		_vmassReadSector(sector_pos, data, sector_num);
	}

	VMASS_PERF_E("Read", sector_pos, sector_num);

	ksceKernelUnlockFastMutex(&lw_mtx);

	return res;
}

int vmassWriteSector(SceSize sector_pos, const void *data, SceSize sector_num){

	int res = 0, s1;

	SceSize off = (sector_pos << 9), size = (sector_num << 9);

	if(((size - 1) > g_vmass_size) || (off >= g_vmass_size) || ((off + size) > g_vmass_size))
		return -1;

	ksceKernelLockFastMutex(&lw_mtx);

	s1 = sector_num & 1;
	sector_num >>= 1;

	VMASS_PERF_S();

	if(sector_num >= 0x20){
		g_sector_pos = sector_pos;
		g_sector_num = sector_num;
		g_pDataForWrite = data;
		ksceKernelSetEventFlag(evf_id, VMASS_REQ_WRITE);
	}else{
		sector_num = (sector_num << 1) + s1;
		s1 = -1;
	}

	if(s1 >= 0){
		_vmassWriteSector(sector_pos + sector_num, data + (sector_num << 9), sector_num + s1);

		if(sector_num != 0){
			ksceKernelWaitEventFlag(evf_id, VMASS_REQ_DONE, SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR_PAT, NULL, NULL);
		}
		sector_num = (sector_num << 1) + s1;
	}else{
		_vmassWriteSector(sector_pos, data, sector_num);
	}

	VMASS_PERF_E("Write", sector_pos, sector_num);

	ksceKernelUnlockFastMutex(&lw_mtx);

	return res;
}

int sceUsbMassIntrHandler(int intr_code, void *userCtx){

	if(intr_code != 0xF)
		return 0x80010016;

	return -1;
}

int SceUsbMassForDriver_3C821E99(int a1, int a2){

	if(a1 != 0xF)
		return 0x80010016;

	return 0;
}

int SceUsbMassForDriver_7833D935(int a1, int a2){

	if(a1 != 0xF)
		return 0x80010016;

	return 0;
}

int vmassFreeStoragePage(void){

	int i = VMASS_PAGE_MAX_NUMBER;

	do {
		i--;
		if(vmass_page_list[i].base != NULL)
			ksceKernelFreeMemBlock(ksceKernelFindMemBlockByAddr(vmass_page_list[i].base, 0));
	} while(i != 0);

	return 0;
}

int vmassPageRegister(VmassPageInfo *info, void *base, SceSize size){

	if(base != NULL)
		info->base = base;
	info->size = size;

	g_vmass_size += size;

	return 0;
}

int vmassPageAlloc(VmassPageInfo *info, SceUInt32 memtype, SceSize size){

	SceUID memid = ksceKernelAllocMemBlock("VmassStoragePage", memtype, size, NULL);
	if(memid < 0){
		vmassFreeStoragePage();

		return memid;
	}

	ksceKernelGetMemBlockBase(memid, &(info->base));

	ksceDmacMemset(info->base, 0, size);

	vmassPageRegister(info, NULL, size);

	return 0;
}

int vmassAllocStoragePage(void){

	// ScePhyMemPartPhyCont
	vmassPageAlloc(&vmass_page_list[0], 0x1080D006, SIZE_6MiB);

	// ScePhyMemPartGameCdram
	// vmassPageAlloc(&vmass_page_list[1], 0x40404006, SIZE_10MiB);

	return 0;
}

int vmassInitImageHeader(void){

	int buf[0x200 >> 2];
	FatHeader fat_header;

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

	return 0;
}

int vmassLoadImage(void){

	int res;
	SceIoStat stat;
	SceUID fd;

	fd = ksceIoOpen("sd0:vmass.img", SCE_O_RDONLY, 0);
	if(fd < 0)
		fd = ksceIoOpen("ux0:data/vmass.img", SCE_O_RDONLY, 0);

	if(fd < 0)
		return fd;

	res = ksceIoGetstatByFd(fd, &stat);
	if(res < 0)
		goto io_close;

	if(stat.st_size > (SceOff)(g_vmass_size)){
		res = -1;
		goto io_close;
	}

	g_vmass_size = (SceSize)stat.st_size;

	SceSize page_idx = 0, size = g_vmass_size, work_size;

	while(size != 0){
		work_size = (size > vmass_page_list[page_idx].size) ? vmass_page_list[page_idx].size : size;

		ksceIoRead(fd, vmass_page_list[page_idx].base, work_size);

		size -= work_size;
		page_idx++;
	}

	res = 0;

io_close:
	ksceIoClose(fd);

	return res;
}

int vmassCreateImage(void){

	int res;
	SceIoStat stat;
	SceUID fd;

	fd = ksceIoOpen("sd0:vmass.img", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
	if(fd < 0)
		fd = ksceIoOpen("ux0:data/vmass.img", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);

	if(fd < 0)
		return fd;

	memset(&stat, 0, sizeof(stat));
	stat.st_size = (SceOff)g_vmass_size;

#define SCE_CST_SIZE        0x0004

	res = ksceIoChstatByFd(fd, &stat, SCE_CST_SIZE);
	if(res < 0)
		goto io_close;

	SceSize page_idx = 0, size = g_vmass_size, work_size;

	while(size != 0){
		work_size = (size > vmass_page_list[page_idx].size) ? vmass_page_list[page_idx].size : size;

		ksceIoWrite(fd, vmass_page_list[page_idx].base, work_size);

		size -= work_size;
		page_idx++;
	}

	res = 0;

io_close:
	ksceIoClose(fd);

	return res;
}

int vmassInit(void){

	int res;

	res = ksceKernelInitializeFastMutex(&lw_mtx, "VmassMutex", 0, 0);
	if(res < 0)
		return res;

	evf_id = ksceKernelCreateEventFlag("VmassEvf", SCE_EVENT_WAITMULTIPLE, 0, NULL);
	if(evf_id < 0){
		res = evf_id;
		goto del_mtx;
	}

	thid = ksceKernelCreateThread("SceVmassRWThread", sceVmassRWThread, 0x6E, 0x1000, 0, 1 << 3, NULL);
	if(thid < 0){
		res = thid;
		goto del_evf;
	}

	res = ksceKernelStartThread(thid, 0, NULL);
	if(res < 0)
		goto del_thread;

	sysevent_id = ksceKernelRegisterSysEventHandler("SceSysEventVmass", vmassSysEventHandler, NULL);
	if(sysevent_id < 0){
		res = sysevent_id;
		goto stop_thread;
	}

	res = vmassAllocStoragePage();
	if(res < 0)
		goto unregister_sys_event;

	res = vmassLoadImage();
	if(res < 0)
		res = vmassInitImageHeader();

	if(res < 0)
		goto free_storage_page;

end:
	return res;

free_storage_page:
	vmassFreeStoragePage();

unregister_sys_event:
	ksceKernelUnregisterSysEventHandler(sysevent_id);

stop_thread:
	ksceKernelSetEventFlag(evf_id, VMASS_REQ_EXIT);
	ksceKernelWaitEventFlag(evf_id, VMASS_REQ_DONE, SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR_PAT, NULL, NULL);

	ksceKernelWaitThreadEnd(thid, NULL, NULL);

del_thread:
	ksceKernelDeleteThread(thid);

del_evf:
	ksceKernelDeleteEventFlag(evf_id);

del_mtx:
	ksceKernelDeleteFastMutex(&lw_mtx);

	goto end;
}
