/*
 * PlayStation(R)Vita Virtual Mass FAT Header
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

#ifndef _VMASS_FAT_H_
#define _VMASS_FAT_H_

#include <stdint.h>

typedef struct FAT_Base {
	char bootcode[3];           // 0xEB, 0xFE, 0x90
	char oem_name[8];
	uint16_t sector_size;
	uint8_t allocation_sector;  // * sector_size
	uint16_t rsvd_sector;

	// off:0x10
	uint8_t num_fats;           // 2
	uint16_t root_entry_sector; // for FAT12/16
	uint16_t all_sector_num;    // for FAT12/16, num < 0x10000
	uint8_t media;              // 0xF8
	uint16_t fat_size_16;       // maybe ((all_sector >> 11) + 0x3)

	// 0x18
	uint16_t sector_per_track;  // 0x3F
	uint16_t head_num;          // 0xFF
	uint32_t hidden_sector;

	// offset:0x20
	uint32_t all_sector;
} __attribute__((packed)) FAT_Base; // size is 0x24

typedef struct FAT32_t {
	uint32_t fat_size;           // (all_sector >>> 13) + 0x3F & ~0x3F
	uint16_t ext_flags;          // 0
	uint16_t fs_version;         // 0
	uint32_t root_cluster;       // 2
	uint16_t fsinfo_sector;      // 1
	uint16_t boot_backup_sector; // 6
	char rsvd1[12];
	uint8_t drive_num;           // 0x80
	char rsvd2;
	char boot_sig;               // 0x29
	uint32_t volume_id;          // random val
	char volume_label[11];
	char fs_type[8];             // "FAT32   "
	char bootcode32[420];
	uint16_t sector_sig;         // 0xAA55
} __attribute__((packed)) FAT32_t;

typedef struct FAT16_t {
	uint8_t drive_num;           // 0x80
	char rsvd1;
	char boot_sig;               // 0x29
	uint32_t volume_id;          // random val
	char volume_label[11];
	char fs_type[8];             // "FAT16   "
	char bootcode[448];
	uint16_t sector_sig;         // 0xAA55
} __attribute__((packed)) FAT16_t;

typedef struct FatHeader {
	FAT_Base fat_base;

	union {
		FAT16_t fat16;
		FAT32_t fat32;
	};
} __attribute__((packed)) FatHeader;

typedef struct FAT32Fsinfo {
	uint32_t sign1;
	char rsvd1[480];
	uint32_t sign2;
	uint32_t free_cluster_num;	// ((all_sector - ((fat_size * 2) + allocation_sector)) >> 6) - 1
	uint32_t last_allocated_cluster;
	char rsvd2[14];
	uint16_t sector_sig;
} __attribute__((packed)) FAT32Fsinfo;

int setFat12Header(FatHeader *pFatHeader, unsigned int sector_num);

int setFat16Header(FatHeader *pFatHeader, unsigned int sector_num);

int setFat32Header(FatHeader *pFatHeader, unsigned int sector_num);
int setFat32FsInfo(FAT32Fsinfo *pFAT32Fsinfo, FatHeader *pFatHeader);

#endif	/* _VMASS_FAT_H_ */
