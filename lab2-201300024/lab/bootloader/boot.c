#include "boot.h"

#define SECTSIZE 512
#define PT_LOAD 1
/*
void bootMain(void) {
	int i = 0;
	void (*elf)(void);
	elf = (void(*)(void))0x100000; // kernel is loaded to location 0x100000
	for (i = 0; i < 200; i ++) {
		//readSect((void*)elf + i*512, i+1);
		readSect((void*)elf + i*512, i+9);
	}
	elf(); // jumping to the loaded program
}
*/

void bootMain(void) {
	unsigned int phoff = 0x34;
	unsigned int elf = 0x500000;
	void (*kMainEntry)(void);
	kMainEntry = (void(*)(void))0x100000;

	for (int i = 0; i < 200; i++) {
		readSect((void*)(elf + i*512), 1+i);
	}

	// DONE: 填写kMainEntry、phoff、offset
	// 这个地方的框架代码有些问题，导致框架代码的运行与否依赖于gcc版本，故修改了框架代码；详细细节见实验报告
	struct ELFHeader *header = (struct ELFHeader *)elf;
	kMainEntry = (void(*)(void))header->entry;
	phoff = header->phoff;
	unsigned short phnum = header->phnum;
	struct ProgramHeader * pheader =(struct ProgramHeader *)(elf + phoff);

	for(int i = 0; i < phnum; i++){
		if((pheader + i)->type == PT_LOAD){
			unsigned int off = (pheader + i)->off;
			unsigned int addr = (pheader + i)->vaddr;
			unsigned int filesz = (pheader + i)->filesz;
			unsigned int memsz = (pheader + i)->memsz;
			for(int j = 0; j< filesz; j++){
				*(unsigned char *)(addr + j) = *(unsigned char *)(elf + off + j);
			}
			for(int j = filesz; j < memsz; j++){
				*(unsigned char *)(addr + j) = (unsigned char)0;
			}
		}
	}

	kMainEntry();
}

void waitDisk(void) { // waiting for disk
	while((inByte(0x1F7) & 0xC0) != 0x40);
}

void readSect(void *dst, int offset) { // reading a sector of disk
	int i;
	waitDisk();
	outByte(0x1F2, 1);
	outByte(0x1F3, offset);
	outByte(0x1F4, offset >> 8);
	outByte(0x1F5, offset >> 16);
	outByte(0x1F6, (offset >> 24) | 0xE0);
	outByte(0x1F7, 0x20);

	waitDisk();
	for (i = 0; i < SECTSIZE / 4; i ++) {
		((int *)dst)[i] = inLong(0x1F0);
	}
}
