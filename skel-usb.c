/*
        Thymio-II Firmware

        Copyright (C) 2013 Philippe Retornaz <philippe dot retornaz at epfl dot ch>,
        Mobots group (http://mobots.epfl.ch), Robotics system laboratory (http://lsro.epfl.ch)
        EPFL Ecole polytechnique federale de Lausanne (http://www.epfl.ch)

        See authors.txt for more details about other contributors.

        This program is free software: you can redistribute it and/or modify
        it under the terms of the GNU Lesser General Public License as published
        by the Free Software Foundation, version 3 of the License.

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU Lesser General Public License for more details.

        You should have received a copy of the GNU Lesser General Public License
        along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <string.h>

// Molole include
#include <flash/flash.h>
#include <error/error.h>

#include "thymio-buffer.h"
#include <vm/natives.h>
#include <common/consts.h>
#include <transport/buffer/vm-buffer.h>
#include <usb/usb.h>
#include "usb_function_cdc.h"
#include "usb_uart.h"
#include "rf.h"
#include "log.h"
#include "skel-usb.h"
#include "memory_layout.h"


static unsigned char update_calib;

void AsebaResetIntoBootloader(AsebaVMState *vm) {
	// If we are called it mean that USB is preset because we got the aseba packet ...
	// FIXME: No the case anymore !
	
	/* Switching protocol:
	 * First ask the glue to switch off everything (a bit like a powerdown)
	 * Wait until we don't have any USB data to send
	 * Then mask the USB interrupt
	 * Then jump to the bootloader entry point
	 */
	 
	 /* Ask the glue to poweroff */
	 
	 switch_off();
	 
	 /* Wait util aseba FIFO is empty */
	 while(AsebaFifoTxBusy()) barrier();
	 
	 /* Wait until last USB packet is sent  */
	 while(USBTXBusy()) barrier();
	 
	 USBDisableInterrupts();
	 asm __volatile("goto 0x14800"); // Switch to the bootloader ... 
	 
	 // Small comments:
	 // The bootloader when he has done his job will jump to the __reset
	 // entry point, but WITH usb enabled ... (if 5V present)
	 // Thus in the aseba init we need to check if usb need to be init or not ! 
}

static AsebaNativeFunctionDescription AsebaNativeDescription__system_reboot = 
{
	"_system.reboot",
	"Reboot the microcontroller",
	{
		{0,0}
	}
};

void AsebaNative__system_reboot(AsebaVMState *vm) {
	asm __volatile__("reset");
}

static AsebaNativeFunctionDescription AsebaNativeDescription__system_settings_read = 
{
	"_system.settings.read",
	"Read a setting", 
	{
		{1, "address"},
		{1,	"value"},
		{0,0}
	}
};

static void AsebaNative__system_settings_read(AsebaVMState *vm) {
	uint16_t address = vm->variables[AsebaNativePopArg(vm)];
	uint16_t destidx = AsebaNativePopArg(vm);
	
	if(address > sizeof(settings)/2 - 1) {
		AsebaVMEmitNodeSpecificError(vm, "Address out of settings");
		return;
	}
	vm->variables[destidx] = ((unsigned int *) &settings)[address];
}


static AsebaNativeFunctionDescription AsebaNativeDescription__system_settings_write =
{
	"_system.settings.write",
	"Write a setting",
	{
		{ 1, "address"},
		{ 1, "value"},
		{ 0, 0 }
	}
};

static void AsebaNative__system_settings_write(AsebaVMState *vm) {
	uint16_t address = vm->variables[AsebaNativePopArg(vm)];
	uint16_t sourceidx = AsebaNativePopArg(vm);
	if(address > sizeof(settings)/2 - 1) {
		AsebaVMEmitNodeSpecificError(vm, "Address out of settings");
		return;
	}
	((unsigned int *) &settings)[address] = vm->variables[sourceidx];
}

static AsebaNativeFunctionDescription AsebaNativeDescription__system_settings_flash =
{
	"_system.settings.flash",
	"Write the settings into flash",
	{
		{0,0}
	}
};

void AsebaPutVmToSleep(AsebaVMState *vm) {
// FIXME: Should we support this ? 
	AsebaVMEmitNodeSpecificError(vm, "Sleep mode not supported");
}

#include <skel-usb-user.c>

unsigned int events_flags[2];

struct private_settings __attribute__((aligned(2))) settings;
struct thymio_device_info __attribute__((aligned(1))) thymio_info;

// Nice hack to do a compilation assert with
#define COMPILATION_ASSERT(e) do { enum { assert_static__ = 1/(e) };} while(0)

const AsebaLocalEventDescription * AsebaGetLocalEventsDescriptions(AsebaVMState *vm)
{
	return localEvents;
}

	/* buffer for usb uart */
static __attribute((far)) unsigned char sendQueue[SEND_QUEUE_SIZE];
static __attribute((far)) unsigned char recvQueue[RECV_QUEUE_SIZE];

struct __attribute((far)) _vmVariables vmVariables;
static __attribute((far)) uint16_t vmBytecode[VM_BYTECODE_SIZE];
static __attribute((far)) int16_t vmStack[VM_STACK_SIZE];

AsebaVMState vmState = {
	0,
	VM_BYTECODE_SIZE,
	vmBytecode,
	sizeof(vmVariables) / sizeof(int16_t),
	(int16_t*) &vmVariables,
	
	VM_STACK_SIZE,
	vmStack,
};

/* Callback */
void AsebaIdle(void) {
	// Should never be called
}

void AsebaNativeFunction(AsebaVMState * vm, uint16_t id)
{
	nativeFunctions[id](vm);
}

const AsebaVMDescription * AsebaGetVMDescription(AsebaVMState *vm) {
	return &vmDescription;
}

const AsebaNativeFunctionDescription * const * AsebaGetNativeFunctionsDescriptions(AsebaVMState *vm) {
	return nativeFunctionsDescription;
}

// START of Bytecode into Flash section -----

unsigned char  aseba_flash[PAGE_PER_CHUNK*3][INSTRUCTIONS_PER_PAGE * 2] __attribute__ ((space(prog), aligned(INSTRUCTIONS_PER_PAGE * 2), section(".aseba_bytecode")/*, address(FLASH_END - 0x800*/ /* bootloader */ /*- 0x400 *//* settings */ /*- NUMBER_OF_CHUNK*0x400L*PAGE_PER_CHUNK)*//*, noload*/));

// Put the settings page at a fixed position so it's independant of linker mood.
unsigned char aseba_settings_flash[INSTRUCTIONS_PER_PAGE * 2] __attribute__ ((space(prog), noload, section(".aseba_settings"), address(ASEBA_SETTINGS_ADDRESS)));
#warning "the settings page is NOT initialised"


unsigned char thymio_settings_flash[INSTRUCTIONS_PER_PAGE * 2] __attribute__ ((space(prog), noload, section(".thymio_settings"), address(THYMIO_SETTINGS_ADDRESS)));
#warning "the settings page is NOT initialised"

void AsebaWriteBytecode(AsebaVMState *vm) {
	// Look for the lowest number of use
	unsigned long min = 0xFFFFFFFF;
	unsigned long min_addr = 0;
	unsigned long count;
	unsigned int i;
	unsigned long temp_addr = __builtin_tbladdress(aseba_flash);
	unsigned int instr_count;
	unsigned char * bcptr = (unsigned char *) vm->bytecode;

	log_set_flag(LOG_FLAG_FLASHVMCODE);

	// take the first minimum value
	for (i = 0; i < NUMBER_OF_CHUNK; i++, temp_addr += INSTRUCTIONS_PER_PAGE * 2 * PAGE_PER_CHUNK) {
		count = flash_read_instr(temp_addr);
		if(count < min) {
			min = count;
			min_addr = temp_addr;
		}
	}
	if(min == 0xFFFFFFFF) {
		AsebaVMEmitNodeSpecificError(vm, "Error: min == 0xFFFFFFFF !");
		return;
	}
	min++;
	
	// Now erase the pages
	for(i = 0; i < PAGE_PER_CHUNK; i++)
		flash_erase_page(min_addr + i*INSTRUCTIONS_PER_PAGE * 2);
	
	// Then write the usage count and the bytecode
	flash_prepare_write(min_addr);
	flash_write_instruction(min);
	flash_write_buffer((unsigned char *) vm->bytecode, VM_BYTECODE_SIZE*2);
	flash_complete_write();
	
	
	// Now, check the data
	
	if(min != flash_read_instr(min_addr)) {
		AsebaVMEmitNodeSpecificError(vm, "Error: Unable to flash bytecode (1) !");
		return;
	}
	min_addr += 2;
	instr_count = (VM_BYTECODE_SIZE*2) / 3;
	for(i = 0; i < instr_count; i++) {
		unsigned char data[3];
		flash_read_chunk(min_addr, 3, data);
	
		if(memcmp(data, bcptr, 3)) {
			AsebaVMEmitNodeSpecificError(vm, "Error: Unable to flash bytecode (2) !");
			return;
		}
		bcptr += 3;
		min_addr += 2;
	}
	
	i = (VM_BYTECODE_SIZE * 2) % 3;
	
	if(i != 0) {
		unsigned char data[2];
		flash_read_chunk(min_addr, i, data);
		if(memcmp(data, bcptr, i)) {
			AsebaVMEmitNodeSpecificError(vm, "Error: Unable to flash bytecode (3) !");
			return;
		}
	}

	AsebaVMEmitNodeSpecificError(vm, "Flashing OK");

}

const static unsigned int _magic_[8] = {0xDE, 0xAD, 0xCA, 0xFE, 0xBE, 0xEF, 0x04, 0x02};


void write_page_to_flash(unsigned long page_addr, void * const data, unsigned size) {
	// Look for the last "Magic" we know, this is the most up to date conf
	// Then write the next row with the correct magic.
	// If no magic is found, erase the page, and then write the first one
	// If the last magic is found, erase the page and then write the first one
	int i = 0;
	for(i = 0; i < 8; i++) {
		unsigned int mag = flash_read_low(page_addr + INSTRUCTIONS_PER_ROW * 2 * i);
		if(mag != _magic_[i])
			break;
	}

	if(i == 0 || i == 8) {
		flash_erase_page(page_addr);
		i = 0;
	}

	page_addr += INSTRUCTIONS_PER_ROW * 2 * i;

	flash_prepare_write(page_addr);
	unsigned long temp = (((unsigned long) *((unsigned char * ) data)) << 16) | _magic_[i];

	flash_write_instruction(temp);
	flash_write_buffer(((unsigned char *) data) + 1, size - 1);
	flash_complete_write();
}


int load_page_from_flash(unsigned long page_addr, void * const data, int sizeofdata) {
	// The the last "known" magic found
	int i = 0;
	for(i = 0; i < 8; i++) {
		unsigned mag = flash_read_low(page_addr + INSTRUCTIONS_PER_ROW * 2 * i);
		if(mag != _magic_[i])
			break;
	}
	if(i == 0)
		// No settings found
		return -1;
	i--;
	page_addr += INSTRUCTIONS_PER_ROW * 2 * i;
	*((unsigned char *) data) = (unsigned char) (flash_read_high(page_addr) & 0xFF);
	flash_read_chunk(page_addr + 2, sizeofdata - 1, ((unsigned char *) data) + 1);
	return 0;
}
// END of bytecode into flash section


void AsebaNative__system_settings_flash(AsebaVMState* vm) {
	write_page_to_flash(__builtin_tbladdress(aseba_settings_flash), &settings, sizeof(settings));
}

static int load_code_from_flash(AsebaVMState *vm) {
	// Find the last maximum value
	unsigned long max = 0;
	unsigned long max_addr = 0;
	unsigned long temp_addr = __builtin_tbladdress(aseba_flash);
	unsigned long count;
	unsigned int i;
	// take the last maximum value
	for (i = 0; i < NUMBER_OF_CHUNK; i++, temp_addr += INSTRUCTIONS_PER_PAGE * 2 * PAGE_PER_CHUNK) {
		count = flash_read_instr(temp_addr);
		if(count >= max) {
			max = count;
			max_addr = temp_addr;
		}
	}
	if(!max)
		// Nothing to load
		return 0;
	
	flash_read_chunk(max_addr + 2, VM_BYTECODE_SIZE*2, (unsigned char *) vm->bytecode);

	// Bytecode[0] is the event "irq table" size + 1.
	// If no event is setup, the bytecode is empty, thus no bytecode exist.
	if (vm->bytecode[0] <= 1)
		return 0;
	
	return 1;
}

int load_settings_from_flash(void) {
	// Max size 95 int, min 1 int
	COMPILATION_ASSERT(sizeof(settings) < ((INSTRUCTIONS_PER_ROW*3) - 2));
	COMPILATION_ASSERT(sizeof(settings) > 1);
    return load_page_from_flash(__builtin_tbladdress(aseba_settings_flash), &settings, sizeof(settings));
}
// END of bytecode into flash section


/* Thymio Device info */
int load_thymio_device_info_from_flash(void) {
    //Make sure the memory is 0-out in case it does not exist
    memset(&thymio_info, 0, sizeof(thymio_info));

    // Max size 95 int, min 1 int
    COMPILATION_ASSERT(sizeof(thymio_info) < ((INSTRUCTIONS_PER_ROW*3) - 2));
    COMPILATION_ASSERT(sizeof(thymio_info) > 1);
    return load_page_from_flash(__builtin_tbladdress(thymio_settings_flash), &thymio_info, sizeof(thymio_info));
}

void save_thymio_device_info_to_flash(void) {
    // Max size 95 int, min 1 int
    COMPILATION_ASSERT(sizeof(thymio_info) < ((INSTRUCTIONS_PER_ROW*3) - 2));
    COMPILATION_ASSERT(sizeof(thymio_info) > 1);
    write_page_to_flash(__builtin_tbladdress(thymio_settings_flash), &thymio_info, sizeof(thymio_info));
}


#ifdef ASEBA_ASSERT
void AsebaAssert(AsebaVMState *vm, AsebaAssertReason reason) {
        AsebaVMEmitNodeSpecificError(vm, "VM ASSERT !");
}
#endif

void __attribute__((noreturn)) error_handler(const char * file, int line, int id, void * arg) {
	while(1) asm __volatile__ ("reset");
}

// return 1 if code has been loaded from flash
// return 0 if not the case
int init_aseba_and_fifo(void) {
	int ret;
	
	// If RF module is there, take it's nodeId
	if(rf_get_status() & RF_PRESENT) {
		vmState.nodeId = rf_get_node_id();
		rf_set_link(RF_PRESENCE_ONLY); // We do not want full-aseba traffic now.
	} else
		vmState.nodeId = 1;

	COMPILATION_ASSERT(SEND_QUEUE_SIZE >= (ASEBA_MAX_PACKET_SIZE + 4));
	COMPILATION_ASSERT(RECV_QUEUE_SIZE >= (ASEBA_MAX_PACKET_SIZE + 4));


	AsebaFifoInit(sendQueue, SEND_QUEUE_SIZE, recvQueue, RECV_QUEUE_SIZE);
	
	if(U1CONbits.USBEN) {
		// Usb already enabled ! we can skip the init
		USBEnableInterrupts();
	} else
		usb_uart_init();
	
	_USB1IP = PRIO_COMMUNICATION;

	AsebaVMInit(&vmState);
	vmVariables.id = vmState.nodeId;
	vmVariables.productid = PRODUCT_ID;
	
	ret = load_code_from_flash(&vmState);
	
	error_register_callback(error_handler);
	
	return ret;
}

void __attribute((noreturn)) run_aseba_main_loop(void) {
	// Clear the event mask.
	events_flags[0] = 0;
	events_flags[1] = 0;
	
	// Tell the VM to init
	AsebaVMSetupEvent(&vmState, ASEBA_EVENT_INIT);
	
	while(1)
	{
		update_aseba_variables_read();
		
		AsebaVMRun(&vmState, 1000);
		
		AsebaProcessIncomingEvents(&vmState);
		
		update_aseba_variables_write();
		
		if (AsebaMaskIsSet(vmState.flags, ASEBA_VM_STEP_BY_STEP_MASK) || AsebaMaskIsClear(vmState.flags, ASEBA_VM_EVENT_ACTIVE_MASK))
		{
                    unsigned i;
#if 0
#define FF1R(word, pos) asm("ff1r [%[w]], %[b]" : [b] "=x" (pos) : [w] "r" (word) : "cc")
                    
                    _IPL = 6;
                    FF1R(&events_flags[0], i);
                    if(!i) {
                        FF1R(&events_flags[1], i);
                        if(!i && AsebaFifoRecvBufferEmpty())
                            clock_idle();
                        else
                            i += 16;
                    }
                    _IPL = 0;
#else
                    asm __volatile__ ("mov #SR, w0\r\n"
                                      "mov #0xC0, w1\r\n"
                                      "ior.b w1, [w0],[w0]\r\n"
                                      "ff1r [%[word]++], %[b]\r\n"
                                      "bra nc, 2f\r\n"
                                      "ff1r [%[word]], %[b]\r\n"
                                      "bra nc, 1f\r\n"
                                      "rcall _AsebaFifoRecvBufferEmpty\r\n"
                                      "cp0 w0\r\n"
                                      "bra z, 2f\r\n"
                                      "call _clock_idle\r\n"
                                      "bra 2f\r\n"
                                      "1:\r\n"
                                      "add %[b],#16,%[b]\r\n"
                                      "2:\r\n"
                                      "dec2 %[word], %[word]\n"
                                      "mov #SR, w0\r\n"
                                      "mov #0x1F, w1\r\n"
                                      "and.b w1, [w0],[w0]\r\n"
                                      : [b] "=&x" (i) : [word] "r" (events_flags) : "cc", "w0", "w1", "w2", "w3", "w4", "w5", "w6", "w7");
#endif
			if(i && !(AsebaMaskIsSet(vmState.flags, ASEBA_VM_STEP_BY_STEP_MASK) &&  AsebaMaskIsSet(vmState.flags, ASEBA_VM_EVENT_ACTIVE_MASK))) {
				i--;
				CLEAR_EVENT(i);
				vmVariables.source = vmState.nodeId;
				AsebaVMSetupEvent(&vmState, ASEBA_EVENT_LOCAL_EVENTS_START - i);
			}
		}
	}
}

void save_settings(void) {
// if calibration is new, and Vbat > 3.3V, then flash
	if(update_calib && vmVariables.vbat[0] > 655) {
		AsebaNative__system_settings_flash(NULL);
		update_calib=0;
	}
}

void set_save_settings(void) {
	update_calib=1;
}

//Called from the vm, escape hatch to handle messages specific to thymio.
//Returns true if a message was handled
int AsebaHandleDeviceInfoMessages(AsebaVMState* vm, uint16_t id, uint16_t* data, uint16_t dataLength) {
	if(id == ASEBA_MESSAGE_GET_DEVICE_INFO || id == ASEBA_MESSAGE_SET_DEVICE_INFO) {
		if(dataLength < 1) // We need at least an info type
			return 0;
		uint16_t type = bswap16(data[0]);
		if(type > DEVICE_INFO_ENUM_COUNT) // Send an error ?
			return 0;
		if (id == ASEBA_MESSAGE_GET_DEVICE_INFO) {
			uint8_t size = 0;
			const uint8_t* buffer = NULL;
			switch(type) {
				case DEVICE_INFO_UUID:
					buffer = thymio_info.uuid;
					size   = sizeof(thymio_info.uuid);
					break;
				case DEVICE_INFO_NAME:
					buffer = thymio_info.friendly_name + 1;
					size   = *thymio_info.friendly_name;
					if(size > sizeof(thymio_info.friendly_name) -1)
						size = 0;
					break;
			}
			if(size >= 0 && size < (ASEBA_MAX_PACKET_SIZE + 6)) { // Send an error otherwise ?
				uint16_t payload_size = 2 + size;
				uint8_t payload[payload_size];
				payload[0] = type;
				payload[1] = size;
				memcpy(payload + 2, buffer, size);
				AsebaSendMessage(vm, ASEBA_MESSAGE_DEVICE_INFO, payload, payload_size);
			}
		}
		else if (id == ASEBA_MESSAGE_SET_DEVICE_INFO) {
			if(dataLength < 2) // We need at least an info type (1) + size (2)
				return 0;
			uint8_t payload_size = (uint8_t)bswap16(data[1]);
			const uint8_t* buffer = (uint8_t*)data + 2 * sizeof(uint16_t);
			if(dataLength * 2 > (ASEBA_MAX_PACKET_SIZE + 6) || payload_size  > ((dataLength - 2) * 2) + 1 ) {
				return 0;
			}
			switch(type) {
				case DEVICE_INFO_UUID:
					if(payload_size == 0) {
						memset(thymio_info.uuid, 0, sizeof(thymio_info.uuid));
					}
					else if(payload_size == sizeof(thymio_info.uuid)) {
						memcpy(thymio_info.uuid, buffer, payload_size);
					}
				break;
				case DEVICE_INFO_NAME:
					thymio_info.friendly_name[0] = payload_size;
					if(payload_size > 0 && payload_size < sizeof(thymio_info.friendly_name)) {
						memcpy(thymio_info.friendly_name + 1, buffer, payload_size);
					}
				break;
			}
			save_thymio_device_info_to_flash();
		}
		return 1;
	}
	return 0;
}
