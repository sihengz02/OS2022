#include "x86.h"
#include "device.h"

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
extern int bufferHead;
extern int bufferTail;

int tail = 0;

void GProtectFaultHandle(struct TrapFrame *tf);

void KeyboardHandle(struct TrapFrame *tf);

void syscallHandle(struct TrapFrame *tf);
void syscallWrite(struct TrapFrame *tf);
void syscallPrint(struct TrapFrame *tf);
void syscallRead(struct TrapFrame *tf);
void syscallGetChar(struct TrapFrame *tf);
void syscallGetStr(struct TrapFrame *tf);


void irqHandle(struct TrapFrame *tf) { // pointer tf = esp
	/*
	 * 中断处理程序
	 */
	/* Reassign segment register */
	asm volatile("movw %%ax, %%ds"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%es"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%fs"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%gs"::"a"(KSEL(SEG_KDATA)));
	switch(tf->irq) {
		// DONE: 填好中断处理程序的调用
		case -1:
			break;
		case 0xd:
			GProtectFaultHandle(tf);
			break;
		case 0x21:
			KeyboardHandle(tf);
			break;
		case 0x80:
			syscallHandle(tf);
			break;
		default:assert(0);
	}
}

void GProtectFaultHandle(struct TrapFrame *tf){
	assert(0);
	return;
}

void KeyboardHandle(struct TrapFrame *tf){
	uint32_t code = getKeyCode();
	if(code == 0xe){ // 退格符
		// DONE: 要求只能退格用户键盘输入的字符串，且最多退到当行行首
		if(displayCol>0 && displayCol>tail){
			displayCol --;
			uint16_t data = 0 | (0x0c << 8);
			int pos = (80*displayRow + displayCol)*2;
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));
		}
	}else if(code == 0x1c){ // 回车符
		// DONE: 处理回车情况
		keyBuffer[bufferTail ++] = '\n';
		bufferTail %= MAX_KEYBUFFER_SIZE;
		displayRow ++;
		displayCol = 0;
		tail = 0;
		if(displayRow == 25){
			scrollScreen();
			displayRow = 24;
			displayCol = 0;
		}
	}else if(code < 0x81){ // 正常字符
		// DONE: 注意输入的大小写的实现、不可打印字符的处理
		char ch = getChar(code);
		if(ch != 0){
			putChar(ch);
			uint16_t data = ch | (0x0c<<8);
			keyBuffer[bufferTail ++] = ch;
			bufferTail %= MAX_KEYBUFFER_SIZE;
			int pos = (80*displayRow + displayCol)*2;
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));
			displayCol ++;
			if(displayCol == 80){
				displayCol = 0;
				displayRow ++;
				if(displayRow == 25){
					scrollScreen();
					displayRow = 24;
					displayCol = 0;
				}
			}
		}
	}
	updateCursor(displayRow, displayCol);
}

void syscallHandle(struct TrapFrame *tf) {
	switch(tf->eax) { // syscall number
		case 0:
			syscallWrite(tf);
			break; // for SYS_WRITE
		case 1:
			syscallRead(tf);
			break; // for SYS_READ
		default:break;
	}
}

void syscallWrite(struct TrapFrame *tf) {
	switch(tf->ecx) { // file descriptor
		case 0:
			syscallPrint(tf);
			break; // for STD_OUT
		default:break;
	}
}

void syscallPrint(struct TrapFrame *tf) {
	int sel = USEL(SEG_UDATA); //DONE: segment selector for user data, need further modification
	char *str = (char*)tf->edx;
	int size = tf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	for (i = 0; i < size; i++) {
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str+i));
		// DONE: 完成光标的维护和打印到显存
		if(character !='\n'){
			data = character | (0x0c << 8);
			pos = (80*displayRow + displayCol)*2;
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));
			displayCol ++;
			if(displayCol == 80){
				displayCol = 0;
				displayRow ++;
				if(displayRow == 25){
					scrollScreen();
					displayRow = 24;
					displayCol = 0;
				}
			}
		}else{
			displayCol = 0;
			displayRow ++;
			if(displayRow == 25){
				scrollScreen();
				displayRow = 24;
				displayCol = 0;
			}
		}
	}
	tail = displayCol;
	updateCursor(displayRow, displayCol);
}

void syscallRead(struct TrapFrame *tf){
	switch(tf->ecx){ //file descriptor
		case 0:
			syscallGetChar(tf);
			break; // for STD_IN
		case 1:
			syscallGetStr(tf);
			break; // for STD_STR
		default:break;
	}
}

void syscallGetChar(struct TrapFrame *tf){
	// DONE: 自由实现
	bufferHead = 0;
	bufferTail = 0;
	keyBuffer[bufferHead] = 0;
	keyBuffer[bufferHead + 1] = 0;
	char get = 0;
	
	while(get == 0){
		enableInterrupt();
		get = keyBuffer[bufferHead];
		//putChar(get);
		disableInterrupt();
	}
	tf->eax = get;

	char end = 0;
	while(end == 0){
		enableInterrupt();
		end = keyBuffer[bufferHead + 1];
		disableInterrupt();
	} 
}

void syscallGetStr(struct TrapFrame *tf){
	// DONE: 自由实现
	bufferHead = 0;
	bufferTail = 0;
	char *str = (char *)tf->edx;
	int size = (int)tf->ebx;

	for(int i = 0; i < MAX_KEYBUFFER_SIZE; i++){
		keyBuffer[i] = 0;
	}

	int i = 0;
	char end = 0;
	while(end !='\n' && i < size){
		while(keyBuffer[i] == 0){
			enableInterrupt();
		}
		end = keyBuffer[i];
		i ++;
		disableInterrupt();
	}

	int sel = USEL(SEG_UDATA);
	asm volatile("movw %0, %%es"::"m"(sel));

	for(int j = bufferHead; j < i-1; j ++){
		asm volatile("movb %0, %%es:(%1)"::"r"(keyBuffer[j]),"r"(str + j - bufferHead));
	}
	asm volatile("movb $0x00, %%es:(%0)"::"r"(str + i));
}
