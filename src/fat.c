/*
 * PlayStation(R)Vita Virtual Mass FAT
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

#include <string.h>
#include "fat.h"

int setFat32Header(FatHeader *pFatHeader, unsigned int sector_num){

	memset(pFatHeader, 0, sizeof(FatHeader));

	pFatHeader->fat_base.bootcode[0] = 0xEB;
	pFatHeader->fat_base.bootcode[1] = 0xFE;
	pFatHeader->fat_base.bootcode[2] = 0x90;

	memcpy(pFatHeader->fat_base.oem_name, "FAPS    ", 8);

	pFatHeader->fat_base.sector_size       = 0x200;
	pFatHeader->fat_base.allocation_sector = 0x40; // Allocation unit size, (((byte / sector_size) / num_fats))
	pFatHeader->fat_base.rsvd_sector       = 0x40;

	pFatHeader->fat_base.num_fats          = 2;
	pFatHeader->fat_base.root_entry_sector = 0;
	pFatHeader->fat_base.all_sector_num    = 0;

	pFatHeader->fat_base.media            = 0xF8;
	pFatHeader->fat_base.fat_size_16      = 0;
	pFatHeader->fat_base.sector_per_track = 0x3F;
	pFatHeader->fat_base.head_num         = 0xFF;
	pFatHeader->fat_base.hidden_sector    = 0;
	pFatHeader->fat_base.all_sector       = sector_num;

	pFatHeader->fat32.fat_size            = ((sector_num >> 13) + 0x3F) & ~0x3F;
	pFatHeader->fat32.ext_flags           = 0;
	pFatHeader->fat32.fs_version          = 0;
	pFatHeader->fat32.root_cluster        = 2;
	pFatHeader->fat32.fsinfo_sector       = 1;
	pFatHeader->fat32.boot_backup_sector  = 6;
	pFatHeader->fat32.drive_num           = 0x80;
	pFatHeader->fat32.boot_sig            = 0x29;
	pFatHeader->fat32.volume_id           = 0x287C78C1;

	memcpy(pFatHeader->fat32.volume_label, "NO NAME    ", 11);
	memcpy(pFatHeader->fat32.fs_type, "FAT32   ", 8);

	pFatHeader->fat32.sector_sig          = 0xAA55;

	return 0;
}

int setFat32FsInfo(FAT32Fsinfo *pFAT32Fsinfo, FatHeader *pFatHeader){

	memset(pFAT32Fsinfo, 0, sizeof(FAT32Fsinfo));

	pFAT32Fsinfo->sign1                  = 0x41615252;
	pFAT32Fsinfo->sign2                  = 0x61417272;
	pFAT32Fsinfo->free_cluster_num       = ((pFatHeader->fat_base.all_sector - ((pFatHeader->fat32.fat_size * 2) + pFatHeader->fat_base.allocation_sector)) >> 6) - 1;
	pFAT32Fsinfo->last_allocated_cluster = 0xFFFFFFFF;
	pFAT32Fsinfo->sector_sig             = 0xAA55;

	return 0;
}

int setFat16Header(FatHeader *pFatHeader, unsigned int sector_num){

	memset(pFatHeader, 0, sizeof(FatHeader));

	pFatHeader->fat_base.bootcode[0] = 0xEB;
	pFatHeader->fat_base.bootcode[1] = 0xFE;
	pFatHeader->fat_base.bootcode[2] = 0x90;

	memcpy(pFatHeader->fat_base.oem_name, "FAPS    ", 8);

	pFatHeader->fat_base.sector_size       = 0x200;
	pFatHeader->fat_base.allocation_sector = 8; // Allocation unit size, (((byte / sector_size) / num_fats))
	pFatHeader->fat_base.rsvd_sector       = 2;

	pFatHeader->fat_base.num_fats          = 2;
	pFatHeader->fat_base.root_entry_sector = 0x200;
	pFatHeader->fat_base.media             = 0xF8;
	pFatHeader->fat_base.fat_size_16       = (uint16_t)((sector_num >> 11) + 0x3);

	if(sector_num >= 0x10000){
		pFatHeader->fat_base.all_sector_num = 0;
		pFatHeader->fat_base.all_sector     = sector_num;
	}else{
		pFatHeader->fat_base.all_sector_num = (uint16_t)(sector_num);
		pFatHeader->fat_base.all_sector     = 0;
	}

	pFatHeader->fat_base.sector_per_track = 0x3F;
	pFatHeader->fat_base.head_num         = 0xFF;
	pFatHeader->fat_base.hidden_sector    = 0;

	pFatHeader->fat16.drive_num           = 0x80;
	pFatHeader->fat16.boot_sig            = 0x29;
	pFatHeader->fat16.volume_id           = 0x287C78C1;

	memcpy(pFatHeader->fat16.volume_label, "NO NAME    ", 11);
	memcpy(pFatHeader->fat16.fs_type, "FAT16   ", 8);

	pFatHeader->fat16.sector_sig          = 0xAA55;

	return 0;
}

int setFat12Header(FatHeader *pFatHeader, unsigned int sector_num){

	int res;

	res = setFat16Header(pFatHeader, sector_num);

	memcpy(pFatHeader->fat16.fs_type, "FAT12   ", 8);

	return res;
}
