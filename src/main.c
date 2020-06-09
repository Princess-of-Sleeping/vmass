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
#include <psp2kern/io/fcntl.h>
#include <taihen.h>
#include "vmass.h"

#define HookExport(module_name, library_nid, func_nid, func_name) taiHookFunctionExportForKernel(0x10005, &func_name ## _ref, module_name, library_nid, func_nid, func_name ## _patch)
#define HookImport(module_name, library_nid, func_nid, func_name) taiHookFunctionImportForKernel(0x10005, &func_name ## _ref, module_name, library_nid, func_nid, func_name ## _patch)
#define HookOffset(modid, offset, thumb, func_name) taiHookFunctionOffsetForKernel(0x10005, &func_name ## _ref, modid, 0, offset, thumb, func_name ## _patch)

#define HookRelease(hook_uid, hook_func_name) ({ \
	(hook_uid > 0) ? taiHookReleaseForKernel(hook_uid, hook_func_name ## _ref) : -1; \
})

#define GetExport(modname, lib_nid, func_nid, func) module_get_export_func(KERNEL_PID, modname, lib_nid, func_nid, (uintptr_t *)func)

int module_get_export_func(SceUID pid, const char *modname, uint32_t lib_nid, uint32_t func_nid, uintptr_t *func);

SceUID hook[6];

tai_hook_ref_t sceUsbMassGetDevInfo_ref;
tai_hook_ref_t sceUsbMassWriteSector_ref;
tai_hook_ref_t sceUsbMassReadSector_ref;
tai_hook_ref_t sceUsbMassIntrHandler_ref;
tai_hook_ref_t SceUsbMassForDriver_3C821E99_ref;
tai_hook_ref_t SceUsbMassForDriver_7833D935_ref;

int sceUsbMassGetDevInfo_patch(SceUsbMassDevInfo *info){
	return vmassGetDevInfo(info);
}

int sceUsbMassWriteSector_patch(SceSize sector_pos, const void *data, SceSize sector_num){
	return vmassWriteSector(sector_pos, data, sector_num);
}

int sceUsbMassReadSector_patch(SceSize sector_pos, void *data, SceSize sector_num){
	return vmassReadSector(sector_pos, data, sector_num);
}

int sceUsbMassIntrHandler_patch(int intr_code, void *userCtx){

	if(intr_code != 0xF)
		return 0x80010016;

	return -1;
}

int SceUsbMassForDriver_3C821E99_patch(int a1, int a2){

	if(a1 != 0xF)
		return 0x80010016;

	return 0;
}

int SceUsbMassForDriver_7833D935_patch(int a1, int a2){

	if(a1 != 0xF)
		return 0x80010016;

	return 0;
}

void _start() __attribute__ ((weak, alias("module_start")));
int module_start(SceSize args, void *argp){

	if(ksceKernelSearchModuleByName("SceUsbMass") > 0)
		return SCE_KERNEL_START_NO_RESIDENT;

	if(vmassInit() < 0)
		return SCE_KERNEL_START_FAILED;

	hook[0] = HookImport("SceSdstor", 0x15243ec5, 0xd989a9f6, sceUsbMassGetDevInfo);
	hook[1] = HookImport("SceSdstor", 0x15243ec5, 0xb80d1df8, sceUsbMassReadSector);
	hook[2] = HookImport("SceSdstor", 0x15243ec5, 0x081ca197, sceUsbMassWriteSector);
	hook[3] = HookImport("SceSdstor", 0x15243ec5, 0xf2bab182, sceUsbMassIntrHandler);
	hook[4] = HookImport("SceSdstor", 0x15243ec5, 0x3c821e99, SceUsbMassForDriver_3C821E99);
	hook[5] = HookImport("SceSdstor", 0x15243ec5, 0x7833d935, SceUsbMassForDriver_7833D935);

	ksceIoMount(0xF00, NULL, 2, 0, 0, 0);

	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize args, void *argp){
	return SCE_KERNEL_STOP_CANCEL;
}
