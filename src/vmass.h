/*
 * PlayStation(R)Vita Virtual Mass Header
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

#ifndef _VMASS_H_
#define _VMASS_H_

#include <stdint.h>
#include <psp2/types.h>

typedef struct SceUsbMassDevInfo {
	SceSize number_of_all_sector;
	int data_04;
	SceSize sector_size;
	int data_0C;
} SceUsbMassDevInfo;

int vmassInit(void);

int vmassGetDevInfo(SceUsbMassDevInfo *info);
int vmassReadSector(SceSize sector_pos, void *data, SceSize sector_num);
int vmassWriteSector(SceSize sector_pos, const void *data, SceSize sector_num);

#endif	/* _VMASS_H_ */
