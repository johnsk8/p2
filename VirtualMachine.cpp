/*	A virtual machine for ECS 150
	Filename: VirtualMachine.cpp
	Authors: John Garcia, Felix Ng

	In this version:
	TVMStatus VMStart -	starting, need machineinit
	TVMMainEntry VMLoadModule -	GIVEN
	void VMUnloadModule - GIVEN
	TVMStatus VMThreadCreate - not started
	TVMStatus VMThreadDelete - not started
	TVMStatus VMThreadActivate - not started
	TVMStatus VMThreadTerminate - not started
	TVMStatus VMThreadID - not started
	TVMStatus VMThreadState - not started
	TVMStatus VMThreadSleep - starting
	TVMStatus VMMutexCreate - not started
	TVMStatus VMMutexDelete - not started
	TVMStatus VMMutexQuery - not started
	TVMStatus VMMutexAcquire - not started
	TVMStatus VMMutexRelease - not started
	TVMStatus VMFileOpen - not started
	TVMStatus VMFileClose - not started  
	TVMStatus VMFileRead - not started
	TVMStatus VMFileWrite - started, used write(), need machinefilewrite()
	TVMStatus VMFileSeek - not started
	TVMStatus VMFilePrint - GIVEN
	MachineContextSave - GIVEN
	MachineContextRestore - GIVEN
	MachineContextSwitch - GIVEN
	MachineContextCreate - GIVEN
	void MachineInitialize - GIVEN
	void MachineTerminate - GIVEN
	void MachineEnableSignals - GIVEN
	void MachineSuspendSignals - GIVEN
	void MachineResumeSignals - GIVEN
	void MachineRequestAlarm - GIVEN
	void MachineFileOpen - GIVEN
	void MachineFileRead - GIVEN
	void MachineFileWrite - GIVEN
	void MachineFileSeek - GIVEN
	void MachineFileClose - GIVEN
*/

#include "VirtualMachine.h"
#include "Machine.h"
#include <unistd.h> //standard symbolic constants and types
#include <signal.h> //C library to handle signals
#include <time.h> //time types
#include <sys/types.h> //data types
#include <sys/ipc.h> //interprocess communication access structure
#include <sys/msg.h> //message queue structures
#include <sys/stat.h> //data returned by the stat() function
#include <sys/wait.h> //declarations for waiting
#include <fcntl.h> //file control options
#include <errno.h> //system error numbers
#include <poll.h> //definitions for the poll() function
#include <string.h> //basic string handling functions
#include <stdlib.h> //standard library definitions
#include <stdint.h> //integer types
#include <stdio.h> //standard input/output
#include <vector> //vector functions
#include <map> //map functions
#include <thread> //thread functions
#include <iostream>
#include <string>
#include <stddef.h>
using namespace std;

extern "C"
{
typedef void (*TVMMain)(int argc, char *argv[]);
TVMMainEntry VMLoadModule(const char *module);
//typedef void MachineRequestAlarm(useconds_t usec, TMachineAlarmCallback callback, void *calldata);

volatile uint16_t test = 0;
volatile uint16_t globaltick = 0;

void AlarmCallback(void *params, int result){
	globaltick--;
}

TVMStatus VMStart(int tickms, int machinetickms, int argc, char *argv[])
{
	TVMMain VMMain = VMLoadModule(argv[0]); //load the module
	
	MachineInitialize(machinetickms); //initialize the machine with specified time
	
	useconds_t usec = tickms * 1000;
	
	MachineRequestAlarm(usec, (TMachineAlarmCallback)AlarmCallback, NULL); //starts the alarm tick
	MachineEnableSignals(); //start the signals

	if(VMMain == NULL) //fail to load module
		return VM_STATUS_FAILURE;
	else //load successful
	{
		cout << "opening " << argv[0] << endl;
		VMMain(argc, argv); //function call to start TVMMain
		return VM_STATUS_SUCCESS;
	}
} //TVMStatus VMStart()

TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, 
	TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid)
{} //TVMStatus VMThreadCreate()

TVMStatus VMThreadDelete(TVMThreadID thread)
{} //TVMStatus VMThreadDelete()

TVMStatus VMThreadActivate(TVMThreadID thread)
{} //TVMStatus VMThreadActivate()

TVMStatus VMThreadTerminate(TVMThreadID thread)
{} //TVMStatus VMThreadTerminate()

TVMStatus VMThreadID(TVMThreadIDRef threadref)
{} //TVMStatus VMThreadID()

TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef stateref)
{} //TVMStatus VMThreadState()

TVMStatus VMThreadSleep(TVMTick tick)
{
	if(tick == VM_TIMEOUT_INFINITE)
		return VM_STATUS_ERROR_INVALID_PARAMETER;
		
	globaltick = tick;

	while (globaltick > 0) {};
	return VM_STATUS_SUCCESS;

} //TVMStatus VMThreadSleep()

TVMStatus VMMutexCreate(TVMMutexIDRef mutexref)
{} //TVMStatus VMMutexCreate()

TVMStatus VMMutexDelete(TVMMutexID mutex)
{} //TVMStatus VMMutexDelete()

TVMStatus VMMutexQuery(TVMMutexID mutex, TVMThreadIDRef ownerref)
{} //TVMStatus VMMutexQuery()

TVMStatus VMMutexAcquire(TVMMutexID mutex, TVMTick timeout)
{} //TVMStatus VMMutexAcquire()

TVMStatus VMMutexRelease(TVMMutexID mutex)
{} //TVMStatus VMMutexRelease()

TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor)
{} //TVMStatus VMFileOpen()

TVMStatus VMFileClose(int filedescriptor)
{} //TVMStatus VMFileClose()

TVMStatus VMFileRead(int filedescriptor, void *data, int *length)
{} //TVMStatus VMFileRead()

TVMStatus VMFileWrite(int filedescriptor, void *data, int *length)
{
	
	if(data == NULL || length == NULL) //invalid input
		return VM_STATUS_ERROR_INVALID_PARAMETER;

	//TMachineFileCallback sd = Machine
	//MachineFileWrite(filedescriptor, data, *length, sd, NULL);
	
	if (write(filedescriptor, data, *length) > -1)			//write to file
		return VM_STATUS_SUCCESS;
	else {
		cout << "VM_WRITE FAULRE" << endl;
		return VM_STATUS_FAILURE;
	}



} //TVMStatus VMFileWrite()

TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, int *newoffset)
{} //TVMStatus VMFileSeek()
} //extern "C"
