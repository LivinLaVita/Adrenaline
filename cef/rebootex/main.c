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

#include <common.h>

#include "main.h"
#include "libc.h"

#define REBOOT_MODULE "/rtm.prx"

int (* sceReboot)(void *reboot_param, struct SceKernelLoadExecVSHParam *vsh_param, int api, int initial_rnd) = (void *)0x88600000;
int (* DcacheClear)(void) = (void *)0x886018AC;
int (* IcacheClear)(void) = (void *)0x88601E40;

int (* DecryptExecutable)(void *buf, int size, int *retSize);

void (* SetMemoryPartitionTable)(void *sysmem_config, SceSysmemPartTable *table);
int (* sceKernelBootLoadFile)(BootFile *file, void *a1, void *a2, void *a3, void *t0);

RebootexConfig *rebootex_config = (RebootexConfig *)0x88FB0000;

void ClearCaches() {
	DcacheClear();
	IcacheClear();
}

void SetMemoryPartitionTablePatched(void *sysmem_config, SceSysmemPartTable *table) {
	SetMemoryPartitionTable(sysmem_config, table);

	// Add partition 11
	table->extVshell.addr = 0x8A000000;
	table->extVshell.size = 20 * 1024 * 1024;
}

int PatchSysMem(void *a0, void *sysmem_config) {
	int (* module_bootstart)(SceSize args, void *sysmem_config) = (void *)_lw((u32)a0 + 0x28);

	// Patch to add new partition	
	SetMemoryPartitionTable = (void *)0x88012258;
	MAKE_CALL(0x880115E0, SetMemoryPartitionTablePatched);

	// Fake as slim model
	// _sw(1, sysmem_config + 0x14);

	ClearCaches();

	return module_bootstart(4, sysmem_config);
}

int DecryptExecutablePatched(void *buf, int size, int *retSize) {
	if (*(u16 *)((u32)buf + 0x150) == 0x8B1F) {
		*retSize = *(u32 *)((u32)buf + 0xB0);
		_memcpy(buf, (void *)((u32)buf + 0x150), *retSize);
		return 0;
	}

	return DecryptExecutable(buf, size, retSize);
}

int PatchLoadCore(int (* module_bootstart)(SceSize args, void *argp), void *argp) {
	u32 text_addr = ((u32)module_bootstart) - 0xAF8;

	// Allow custom modules
	DecryptExecutable = (void *)text_addr + 0x77B4;
	MAKE_CALL(text_addr + 0x5864, DecryptExecutablePatched);

	ClearCaches();

	return module_bootstart(8, argp);
}

int InsertModule(void *buf, char *new_module, char *module_after, int flags) {
	BtcnfHeader *header = (BtcnfHeader *)buf;

	ModuleEntry *modules = (ModuleEntry *)((u32)header + header->modulestart);
	ModeEntry *modes = (ModeEntry *)((u32)header + header->modestart);

	char *modnamestart = (char *)((u32)header + header->modnamestart);
	char *modnameend = (char *)((u32)header + header->modnameend);

	if (header->signature != BTCNF_MAGIC)
		return -1;

	int i;
	for (i = 0; i < header->nmodules; i++) {
		if (_strcmp(modnamestart + modules[i].stroffset, module_after) == 0) {
			break;
		}
	}

	if (i == header->nmodules)
		return -2;

	int len = _strlen(new_module) + 1;

	// Add new_module name at end
	_memcpy((void *)modnameend, (void *)new_module, len);

	// Move module_after forward
	_memmove(&modules[i + 1], &modules[i], (header->nmodules - i) * sizeof(ModuleEntry) + len + modnameend - modnamestart);

	// Add new_module information
	modules[i].stroffset = modnameend - modnamestart;
	modules[i].flags = flags;

	// Update header
	header->nmodules++;
	header->modnamestart += sizeof(ModuleEntry);
	header->modnameend += (len + sizeof(ModuleEntry));

	// Update modes
	int j;
	for (j = 0; j < header->nmodes; j++) {
		modes[j].maxsearch++;
	}

	return 0;
}

int sceKernelCheckPspConfigPatched(void *buf, int size, int flag) {
	if (rebootex_config->module_after) {
		InsertModule(buf, REBOOT_MODULE, rebootex_config->module_after, rebootex_config->flags);
	}

	return 0;
}

int sceKernelBootLoadFilePatched(BootFile *file, void *a1, void *a2, void *a3, void *t0) {
	if (_strcmp(file->name, "pspbtcnf.bin") == 0) {
		char *name = NULL;

		switch(rebootex_config->bootfileindex) {
			case BOOT_NORMAL:
				name = "/kd/pspbtjnf.bin";
				break;
				
			case BOOT_INFERNO:
				name = "/kd/pspbtknf.bin";
				break;
				
			case BOOT_MARCH33:
				name = "/kd/pspbtlnf.bin";
				break;
				
			case BOOT_NP9660:
				name = "/kd/pspbtmnf.bin";
				break;
				
			case BOOT_RECOVERY:
				name = "/kd/pspbtrnf.bin";
				break;
		}

		if (rebootex_config->bootfileindex == BOOT_RECOVERY) {
			rebootex_config->bootfileindex = BOOT_NORMAL;
		}

		file->name = name;
	} else if (_strcmp(file->name, REBOOT_MODULE) == 0) {
		file->buffer = (void *)0x89000000;
		file->size = rebootex_config->size;
		_memcpy(file->buffer, rebootex_config->buf, file->size);
		return 0;
	}

	sceKernelBootLoadFile(file, a1, a2, a3, t0);

	return 0; //always return 0 to allow boot with unsuccessfully loaded files
}

int _start(void *reboot_param, struct SceKernelLoadExecVSHParam *vsh_param, int api, int initial_rnd) __attribute__((section(".text.start")));
int _start(void *reboot_param, struct SceKernelLoadExecVSHParam *vsh_param, int api, int initial_rnd) {
	// Patch call to SysMem module_bootstart
	_sw(0x02402021, 0x886024F8); //move $a0, $s2
	MAKE_CALL(0x8860255C, PatchSysMem);

	// Patch call to LoadCore module_bootstart
	_sw(0x00602021, 0x8860241C); //move $a0, $v1
	MAKE_JUMP(0x88602424, PatchLoadCore);

	// Patch sceKernelCheckPspConfig
	MAKE_CALL(0x88602C04, sceKernelCheckPspConfigPatched);

	// Patch sceKernelBootLoadFile
	sceKernelBootLoadFile = (void *)0x886020BC;
	MAKE_CALL(0x886022FC, sceKernelBootLoadFilePatched);

	ClearCaches();

	// Call original function
	return sceReboot(reboot_param, vsh_param, api, initial_rnd);
}