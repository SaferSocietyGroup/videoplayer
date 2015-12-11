#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdlib>

// from: http://old.honeynet.org/scans/scan33/sols/2-Bilbo/no_garbage.c
// written by "bilbo" for the honeynet projects' scan of the month #33
// assumed to be public domain

// returns the size of the x86 instruction func points to
// note: might not be perfect, but dll entries usually don't contain any 'exotic' instrucitons

int instruction_size_x86(unsigned char *func)
{ 
	unsigned char* old = func;

	// TODO what's this?
	if(*func == 0xcc)
		return 0;

	// Skip prefixes F0h, F2h, F3h, 66h, 67h, D8h-DFh, 2Eh, 36h, 3Eh, 26h, 64h and 65h
	int operandSize = 4; 
	int FPU = 0; 
	while(*func == 0xF0 || 
			*func == 0xF2 || 
			*func == 0xF3 || 
			(*func & 0xFC) == 0x64 || 
			(*func & 0xF8) == 0xD8 ||
			(*func & 0x7E) == 0x62)
	{ 
		if(*func == 0x66) 
		{ 
			operandSize = 2; 
		}
		else if((*func & 0xF8) == 0xD8) 
		{
			FPU = *func++;
			break;
		}

		func++;
	}

	// Skip two-byte opcode byte 
	bool twoByte = false; 
	if(*func == 0x0F) 
	{ 
		twoByte = true; 
		func++; 
	} 

	// Skip opcode byte 
	unsigned char opcode = *func++; 

	// Skip mod R/M byte 
	unsigned char modRM = 0xFF; 
	if(FPU) 
	{ 
		if((opcode & 0xC0) != 0xC0) 
		{ 
			modRM = opcode; 
		} 
	} 
	else if(!twoByte) 
	{ 
		if((opcode & 0xC4) == 0x00 || 
				((opcode & 0xF4) == 0x60 && ((opcode & 0x0A) == 0x02 || (opcode & 0x09) == 0x9)) || 
				(opcode & 0xF0) == 0x80 || 
				((opcode & 0xF8) == 0xC0 && (opcode & 0x0E) != 0x02) || 
				(opcode & 0xFC) == 0xD0 || 
				(opcode & 0xF6) == 0xF6) 
		{ 
			modRM = *func++; 
		} 
	} 
	else 
	{ 
		if(((opcode & 0xF0) == 0x00 && (opcode & 0x0F) >= 0x04 && (opcode & 0x0D) != 0x0D) || 
				(opcode & 0xF0) == 0x30 || 
				opcode == 0x77 || 
				(opcode & 0xF0) == 0x80 || 
				((opcode & 0xF0) == 0xA0 && (opcode & 0x07) <= 0x02) || 
				(opcode & 0xF8) == 0xC8) 
		{ 
			// No mod R/M byte 
		} 
		else 
		{ 
			modRM = *func++; 
		} 
	} 

	// Skip SIB
	if((modRM & 0x07) == 0x04 &&
			(modRM & 0xC0) != 0xC0)
	{
		func += 1;   // SIB
	}

	// Skip displacement
	if((modRM & 0xC5) == 0x05) func += 4;   // Dword displacement, no base 
	if((modRM & 0xC0) == 0x40) func += 1;   // Byte displacement 
	if((modRM & 0xC0) == 0x80) func += 4;   // Dword displacement 

	// Skip immediate 
	if(FPU) 
	{ 
		// Can't have immediate operand 
	} 
	else if(!twoByte) 
	{ 
		if((opcode & 0xC7) == 0x04 || 
				(opcode & 0xFE) == 0x6A ||   // PUSH/POP/IMUL 
				(opcode & 0xF0) == 0x70 ||   // Jcc 
				opcode == 0x80 || 
				opcode == 0x83 || 
				(opcode & 0xFD) == 0xA0 ||   // MOV 
				opcode == 0xA8 ||            // TEST 
				(opcode & 0xF8) == 0xB0 ||   // MOV
				(opcode & 0xFE) == 0xC0 ||   // RCL 
				opcode == 0xC6 ||            // MOV 
				opcode == 0xCD ||            // INT 
				(opcode & 0xFE) == 0xD4 ||   // AAD/AAM 
				(opcode & 0xF8) == 0xE0 ||   // LOOP/JCXZ 
				opcode == 0xEB || 
				(opcode == 0xF6 && (modRM & 0x30) == 0x00))   // TEST 
		{ 
			func += 1; 
		} 
		else if((opcode & 0xF7) == 0xC2) 
		{ 
			func += 2;   // RET 
		} 
		else if((opcode & 0xFC) == 0x80 || 
				(opcode & 0xC7) == 0x05 || 
				(opcode & 0xF8) == 0xB8 ||
				(opcode & 0xFE) == 0xE8 ||      // CALL/Jcc 
				(opcode & 0xFE) == 0x68 || 
				(opcode & 0xFC) == 0xA0 || 
				(opcode & 0xEE) == 0xA8 || 
				opcode == 0xC7 || 
				(opcode == 0xF7 && (modRM & 0x30) == 0x00))
		{ 
			func += operandSize; 
		} 
	} 
	else 
	{ 
		if(opcode == 0xBA ||            // BT 
				opcode == 0x0F ||            // 3DNow! 
				(opcode & 0xFC) == 0x70 ||   // PSLLW 
				(opcode & 0xF7) == 0xA4 ||   // SHLD 
				opcode == 0xC2 || 
				opcode == 0xC4 || 
				opcode == 0xC5 || 
				opcode == 0xC6) 
		{ 
			func += 1; 
		} 
		else if((opcode & 0xF0) == 0x80) 
		{
			func += operandSize;   // Jcc -i
		}
	}

	return (intptr_t)func - (intptr_t)old;
}


// Patches a specific function in a DLL to instead jump to a proxy function.
// It basically serves the same function as patching the IAT, but it also works for
// other dlls where the function pointer is loaded at runtime. Not just code in the immediate program. 
// 
// (so eg. if a.dll retrieves the address of b.dll!func() and calls it, which in turn is proxied in the 
// application as proxy_func(), the a.dll call will also be to proxy_func()).

//   func_ptr       function to patch
//   new_func_ptr   proxy function to rpleace it with
//   returns        a buffer with an equivalent function of the original

void* patch_function(void* func_ptr, void* new_func_ptr)
{
	// save part of original function to be overwritten
	int size = 0;

	// step through the code, instruction per instruction, until at least
	// 5 bytes have been accounted for (needed for the absolute jump redirect)

	while(size < 5){
		size += instruction_size_x86((unsigned char*)func_ptr + size);
	}

	// allocate a buffer that will hold a copy of the code that will be overwritten
	// plus an additional jump to the original code at the offset [size]

	char* buffer = (char*)calloc(1, size + 16);
	memcpy(buffer, func_ptr, size);

	// generate a jump to the original function + size bytes
	intptr_t rel_addr = ((intptr_t)(func_ptr) + size) - ((intptr_t)(buffer) + size + 5);
	char* b = buffer + size;

	*(b++) = 0xe9;                  // far JMP
	*((intptr_t*)b) = rel_addr;     // relative address of original function + [size] bytes
	
	DWORD oldProt;
	MEMORY_BASIC_INFORMATION mbi;

	// make the buffer executable
	VirtualQuery(buffer, &mbi, sizeof(MEMORY_BASIC_INFORMATION));
	VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &oldProt);

	// make the original function writeable
	// this will trigger the copy on write, so we're not making changes globally in the OS

	VirtualQuery(func_ptr, &mbi, sizeof(MEMORY_BASIC_INFORMATION));
	VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_READWRITE, &oldProt);
	
	// overwrite the original function with a jump to the proxy function

	rel_addr = (intptr_t)new_func_ptr - ((intptr_t)func_ptr + 5);
					
	*((char*)func_ptr) = 0xe9;                         // far JMP
	*((intptr_t*)(((char*)func_ptr) + 1)) = rel_addr;  // relative address to the proxy function
	
	// re-set the original protection the memory page had
	VirtualProtect(mbi.BaseAddress, mbi.RegionSize, oldProt, &oldProt);
	
	// return the prepared buffer that contains the first few instructions of the original
	// function plus a jump to the actual original function
	return (void*)buffer;
}

// Proxy and new implementation of MessageBoxA from user32.dll

int WINAPI (*OrigMessageBoxA)(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType);
int WINAPI ProxyMessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
	// display the message box if the caption starts with '!',
	// to allow the program to still use message boxes.
	if(lpCaption != NULL && lpCaption[0] == '!'){
		return OrigMessageBoxA(hWnd, lpText, lpCaption + 1, uType);
	}

	return 0;
}

void PatchMessageBox()
{
	void** orig = (void**)&OrigMessageBoxA;
	*orig = patch_function((void*)MessageBoxA, (void*)&ProxyMessageBoxA); 
}
