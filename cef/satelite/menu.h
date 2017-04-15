/*
	Adrenaline
	Copyright (C) 2016-2017, TheFloW

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __MENU_H__
#define __MENU_H__

typedef struct {

	char *name;
	void (* function)();
	char **options;
	int size_options;
	int *value;
	int exit;
} Entry;

void MenuReset(Entry *entries, int size_entries);
void MenuExitFunction(int exit_mode);
int MenuCtrl();
int MenuDisplay();

#endif