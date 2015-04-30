/*	A virtual machine for ECS 150
	Filename: VirtualMachine.cpp
	Authors: John Garcia, Felix Ng

	In this version:
	TVMStatus VMStart -	started, done?
	TVMMainEntry VMLoadModule -	GIVEN
	void VMUnloadModule - GIVEN
	TVMStatus VMThreadCreate - not started
	TVMStatus VMThreadDelete - not started
	TVMStatus VMThreadActivate - not started
	TVMStatus VMThreadTerminate - not started
	TVMStatus VMThreadID - not started
	TVMStatus VMThreadState - not started
	TVMStatus VMThreadSleep - starting, need multi threads
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
	In order to remove all system V messages: 
	1. ipcs //to see msg queue
	2. type this in cmd line: ipcs | grep q | awk '{print "ipcrm -q "$2""}' | xargs -0 bash -c
	3. ipcs //should be clear now
*/

#include "VirtualMachine.h"
#include "Machine.h"
#include <iostream>
#include <vector>
#include <string>
#include <stddef.h>
using namespace std;

extern "C" {
	
class TCB {
public:
	TVMThreadID threadID;// to hold the threads ID, may be redundant, but might be easier to get
	TVMThreadPriority threadPriority; // for the threads priority
	TVMThreadState threadState; // for thread stack
	TVMMemorySize memSize;// for stack size
	uint8_t * base;// this or another byte size type pointer for base of stack
	TVMThreadEntry threadEntry;// for the threads entry function
	void * threadEntryParam;// for the threads entry parameter
	SMachineContext machineContext;// for the context to switch to/from the thread
	TVMTick tick;// for the ticks that thread needs to wait
	// Possibly need something to hold file return type
	// Possibly hold a pointer or ID of mutex waiting on
	// Possibly hold a list of held mutexes
};

typedef void (*TVMMain)(int argc, char *argv[]);
TVMMainEntry VMLoadModule(const char *module);

volatile uint16_t globaltick = 0;
vector<TCB*> threadList;

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
		TCB *VMMainTCB = new TCB; //start main thread
		VMMainTCB->threadID = 1;
		VMMainTCB->threadPriority = VM_THREAD_PRIORITY_NORMAL;
		VMMainTCB->threadState = VM_THREAD_STATE_RUNNING;
		
		threadList.push_back(VMMainTCB);
		VMMain(argc, argv);				//function call to start TVMMain
		return VM_STATUS_SUCCESS;
	}
} //TVMStatus VMStart()

TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid)
{
	if(entry == NULL || tid == NULL)
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	
	TMachineSignalState OldState; //local variable to suspend
	MachineSuspendSignals(&OldState); //suspend signals in order to create thread
	
	uint8_t *stack = new uint8_t[memsize];
	
	TCB *newThread = new(TCB); //start new thread
	newThread->threadID = *tid;
	newThread->threadPriority = prio;
	newThread->threadState = VM_THREAD_STATE_DEAD;
	newThread->base = stack;
	newThread->memSize = memsize;
	newThread->threadEntry = entry;
	threadList.push_back(newThread); //store in vector of ptrs
	
	MachineResumeSignals(&OldState); //resume signals after creating thread
	return VM_STATUS_SUCCESS;
} //TVMStatus VMThreadCreate()

TVMStatus VMThreadDelete(TVMThreadID thread)
{
	return 0;
} //TVMStatus VMThreadDelete()

TVMStatus VMThreadActivate(TVMThreadID thread)
{
	return 0;
} //TVMStatus VMThreadActivate()

TVMStatus VMThreadTerminate(TVMThreadID thread)
{
	return 0;
} //TVMStatus VMThreadTerminate()

TVMStatus VMThreadID(TVMThreadIDRef threadref)
{
	return 0;
} //TVMStatus VMThreadID()

TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef stateref)
{
	if(stateref == NULL) //invalid
		return VM_STATUS_ERROR_INVALID_PARAMETER;
		
	for(vector<TCB*>::iterator it = threadList.begin(); it != threadList.end(); ++it) {			// iterate through entire threadlist
		if((*it)->threadID == thread){
			stateref = &(*it)->threadState;
			return VM_STATUS_SUCCESS;
		}
	}
	
	//thread does not exist
	return VM_STATUS_ERROR_INVALID_ID;
	
} //TVMStatus VMThreadState()

TVMStatus VMThreadSleep(TVMTick tick)
{
	if(tick == VM_TIMEOUT_INFINITE)
		return VM_STATUS_ERROR_INVALID_PARAMETER;

	globaltick = tick;

	while (globaltick > 0) {};
	return VM_STATUS_SUCCESS;

} //TVMStatus VMThreadSleep()

TVMStatus VMMutexCreate(TVMMutexIDRef mutexref)
{
	return 0;
} //TVMStatus VMMutexCreate()

TVMStatus VMMutexDelete(TVMMutexID mutex)
{
	return 0;
} //TVMStatus VMMutexDelete()

TVMStatus VMMutexQuery(TVMMutexID mutex, TVMThreadIDRef ownerref)
{
	return 0;
} //TVMStatus VMMutexQuery()

TVMStatus VMMutexAcquire(TVMMutexID mutex, TVMTick timeout)
{
	return 0;
} //TVMStatus VMMutexAcquire()

TVMStatus VMMutexRelease(TVMMutexID mutex)
{
	return 0;
} //TVMStatus VMMutexRelease()

TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor)
{
	return 0;
} //TVMStatus VMFileOpen()

TVMStatus VMFileClose(int filedescriptor)
{
	return 0;
} //TVMStatus VMFileClose()

TVMStatus VMFileRead(int filedescriptor, void *data, int *length)
{
	return 0;
} //TVMStatus VMFileRead()

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
{
	return 0;
} //TVMStatus VMFileSeek()
} //extern "C"
