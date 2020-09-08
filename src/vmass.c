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
#include <psp2kern/kernel/dmac.h>
#include <psp2kern/io/fcntl.h>
#include <psp2kern/io/stat.h>
#include <psp2kern/ctrl.h>
#include <psp2kern/display.h>
#include <taihen.h>
#include <string.h>
#include "vmass.h"
#include "fat.h"

typedef int (* SceSysEventCallback)(int resume, int eventid, void *args, void *opt);

SceUID ksceKernelRegisterSysEventHandler(const char *name, SceSysEventCallback cb, void *argp);
int ksceKernelUnregisterSysEventHandler(SceUID id);

#define SCE_SYS_EVENT_STATE_SUSPEND  (0)
#define SCE_SYS_EVENT_STATE_POWEROFF (1)
#define SCE_SYS_EVENT_STATE_REBOOT   (2)

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

typedef struct VmassPageAllocInfo {
	unsigned int mem_type;
	unsigned int paddr;
	SceSize size;
} VmassPageAllocInfo;

#define USE_MEMORY_10MiB  1
#define USE_MEMORY_32MiB  0
#define USE_DEVKIT_MEMORY 0

#define DEVKIT_MEM_1MiB  {0x10F0D006, 0x00000000, 0x100000}
#define DEVKIT_MEM_4MiB  DEVKIT_MEM_1MiB, DEVKIT_MEM_1MiB, DEVKIT_MEM_1MiB, DEVKIT_MEM_1MiB
#define DEVKIT_MEM_16MiB DEVKIT_MEM_4MiB, DEVKIT_MEM_4MiB, DEVKIT_MEM_4MiB, DEVKIT_MEM_4MiB

const VmassPageAllocInfo page_alloc_list[] = {

#if USE_DEVKIT_MEMORY != 0
	DEVKIT_MEM_16MiB,
	DEVKIT_MEM_16MiB,
#endif

	// ScePhyMemPartGameCdram
#if USE_MEMORY_32MiB != 0
	{0x40404006, 0x00000000, SIZE_16MiB},
	{0x40404006, 0x00000000, SIZE_16MiB},
#endif

#if USE_MEMORY_10MiB != 0
	{0x40404006, 0x00000000, SIZE_10MiB},
#endif

	// ScePhyMemPartPhyCont
	{0x1080D006, 0x00000000, SIZE_4MiB},

	// SceDisplay
	{0x6020D006, 0x1C000000, SIZE_2MiB}
};

#define VMASS_PAGE_NUM (sizeof(page_alloc_list) / sizeof(VmassPageAllocInfo))

SceUID sysevent_id;
SceSize g_vmass_size;

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

#define VMASS_CAPTURE_SPEED (0)

#if VMASS_CAPTURE_SPEED != 0

#include <psp2kern/kernel/cpu.h>

#define VMASS_PERF_S() SceInt64 time_s, time_e; \
			{ \
			time_s = ksceKernelGetSystemTimeWide(); \
			}

#define VMASS_PERF_E(type, sector) { \
			time_e = ksceKernelGetSystemTimeWide(); \
			int BytePerSecond = 0; \
			getBytePerSecond(time_e - time_s, (sector << 9), &BytePerSecond); \
			ksceDebugPrintf("[%-7s] %d:Time %9dMB/s\n", type, ksceKernelCpuGetCpuId(), BytePerSecond); \
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
#define VMASS_PERF_E(type, sector)

#endif

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

	int res, prev_priority;
	unsigned int opcode;

	while(1){
		opcode = 0;
		res = ksceKernelWaitEventFlag(evf_id, VMASS_REQ_READ | VMASS_REQ_WRITE | VMASS_REQ_EXIT, SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR_PAT, &opcode, NULL);
		if(res < 0)
			continue;

		if(opcode == VMASS_REQ_READ){

			prev_priority = ksceKernelGetThreadCurrentPriority();

			ksceKernelChangeThreadPriority(ksceKernelGetThreadId(), 0x28);

			_vmassReadSector(g_sector_pos, g_pDataForRead, g_sector_num);
			ksceKernelSetEventFlag(evf_id, VMASS_REQ_DONE);

			ksceKernelChangeThreadPriority(ksceKernelGetThreadId(), prev_priority);

		}else if(opcode == VMASS_REQ_WRITE){

			prev_priority = ksceKernelGetThreadCurrentPriority();

			ksceKernelChangeThreadPriority(ksceKernelGetThreadId(), 0x28);

			_vmassWriteSector(g_sector_pos, g_pDataForWrite, g_sector_num);
			ksceKernelSetEventFlag(evf_id, VMASS_REQ_DONE);

			ksceKernelChangeThreadPriority(ksceKernelGetThreadId(), prev_priority);

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

	if((size == 0) || ((off + size) > g_vmass_size) || (off >= g_vmass_size) || (size > g_vmass_size))
		return -1;

	ksceKernelLockFastMutex(&lw_mtx);

	s1 = sector_num & 1;
	sector_num >>= 1;

	VMASS_PERF_S();

	if(sector_num != 0){
		g_sector_pos = sector_pos;
		g_sector_num = sector_num;
		g_pDataForRead = data;
		ksceKernelSetEventFlag(evf_id, VMASS_REQ_READ);
	}

	_vmassReadSector(sector_pos + sector_num, data + (sector_num << 9), sector_num + s1);

	if(sector_num != 0)
		ksceKernelWaitEventFlag(evf_id, VMASS_REQ_DONE, SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR_PAT, NULL, NULL);

	VMASS_PERF_E("Read", ((sector_num << 1) + s1));

	ksceKernelUnlockFastMutex(&lw_mtx);

	return res;
}

int vmassWriteSector(SceSize sector_pos, const void *data, SceSize sector_num){

	int res = 0, s1;

	SceSize off = (sector_pos << 9), size = (sector_num << 9);

	if((size == 0) || ((off + size) > g_vmass_size) || (off >= g_vmass_size) || (size > g_vmass_size))
		return -1;

	ksceKernelLockFastMutex(&lw_mtx);

	s1 = sector_num & 1;
	sector_num >>= 1;

	VMASS_PERF_S();

	if(sector_num != 0){
		g_sector_pos = sector_pos;
		g_sector_num = sector_num;
		g_pDataForWrite = data;
		ksceKernelSetEventFlag(evf_id, VMASS_REQ_WRITE);
	}

	_vmassWriteSector(sector_pos + sector_num, data + (sector_num << 9), sector_num + s1);

	if(sector_num != 0)
		ksceKernelWaitEventFlag(evf_id, VMASS_REQ_DONE, SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR_PAT, NULL, NULL);

	VMASS_PERF_E("Write", ((sector_num << 1) + s1));

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

int vmassFreeStoragePage(void){

	int i = VMASS_PAGE_NUM;

	do {
		i--;
		if(membase[i] != NULL)
			ksceKernelFreeMemBlock(ksceKernelFindMemBlockByAddr(membase[i], 0));
	} while(i != 0);

	return 0;
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
			vmassFreeStoragePage();

			return memid;
		}

		ksceKernelGetMemBlockBase(memid, &membase[i]);

		if(page_alloc_list[i].paddr != 0)
			ksceDmacMemset(membase[i], 0, page_alloc_list[i].size);

		g_vmass_size += page_alloc_list[i].size;
	}

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

	/*
	 * using the bootlogo area, so may see garbage, so clear the kernel framebuffer
	 */
	ksceDisplaySetFrameBuf(NULL, SCE_DISPLAY_SETBUF_NEXTFRAME);

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
		work_size = (size > page_alloc_list[page_idx].size) ? page_alloc_list[page_idx].size : size;

		ksceIoRead(fd, membase[page_idx], work_size);

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
		work_size = (size > page_alloc_list[page_idx].size) ? page_alloc_list[page_idx].size : size;

		ksceIoWrite(fd, membase[page_idx], work_size);

		size -= work_size;
		page_idx++;
	}

	res = 0;

io_close:
	ksceIoClose(fd);

	return res;
}

int sysevent_handler(int resume, int eventid, void *args, void *opt){

	if(resume == 0 && eventid == 0x204 && *(int *)(args + 0x00) == 0x18 && *(int *)(args + 0x04) != SCE_SYS_EVENT_STATE_SUSPEND){

		SceCtrlData pad;
		if(ksceCtrlPeekBufferPositive(0, &pad, 1) < 0)
			goto end;

		if((pad.buttons & SCE_CTRL_START) == 0)
			goto end;

		/*
		 * Unmount uma0: and remove file cache etc.
		 */
		ksceIoUmount(0xF00, 1, 0, 0);

		vmassCreateImage();
	}

end:
	return 0;
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

	thid = ksceKernelCreateThread("SceVmassRWThread", sceVmassRWThread, 0x6E, 0x1000, 0, 1 << 0, NULL);
	if(thid < 0){
		res = thid;
		goto del_evf;
	}

	res = ksceKernelStartThread(thid, 0, NULL);
	if(res < 0)
		goto del_thread;

	sysevent_id = ksceKernelRegisterSysEventHandler("SceSysEventVmass", sysevent_handler, NULL);
	if(sysevent_id < 0){
		res = sysevent_id;
		goto del_thread;
	}

	res = vmassAllocStoragePage();
	if(res < 0)
		goto unregister_sys_event;

	res = vmassLoadImage();
	if(res < 0)
		res = vmassInitImageHeader();

	if(res < 0)
		goto free_storage_page;

	res = load_umass();
	if(res < 0)
		goto free_storage_page;

end:
	return res;

free_storage_page:
	vmassFreeStoragePage();

unregister_sys_event:
	ksceKernelUnregisterSysEventHandler(sysevent_id);

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
