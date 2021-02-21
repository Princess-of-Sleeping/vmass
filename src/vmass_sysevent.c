/*
 * PlayStation(R)Vita Virtual Mass SysEvent Handler
 * Copyright (C) 2021 Princess of Slepping
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
#include <psp2kern/kernel/iofilemgr.h>
#include <psp2kern/syscon.h>
#include "sysevent.h"
#include "vmass.h"

int vmassSysEventHandler(int resume, int eventid, void *args, void *opt){

	if(resume == 0 && eventid == 0x204 && *(int *)(args + 0x00) == 0x18 && *(int *)(args + 0x04) != SCE_SYS_EVENT_STATE_SUSPEND){

		SceUInt32 ctrl = 0x74FFFFFF;

		ksceSysconGetControlsInfo(&ctrl);

		if((~ctrl & SCE_SYSCON_CTRL_START) != 0){

			/*
			 * Unmount uma0: and remove file cache etc.
			 */
			ksceIoUmount(0xF00, 1, 0, 0);

			vmassCreateImage();
		}
	}

	return 0;
}
