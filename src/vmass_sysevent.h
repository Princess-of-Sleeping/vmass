/*
 * PlayStation(R)Vita Virtual Mass SysEvemt Handler Header
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

#ifndef _VMASS_SYSEVENT_H_
#define _VMASS_SYSEVENT_H_

int vmassSysEventHandler(int resume, int eventid, void *args, void *opt);

#endif	/* _VMASS_SYSEVENT_H_ */
