/*	A virtual machine for ECS 150
	Filename: VirtualMachine.cpp
	Authors: John Garcia, Felix Ng

	In this version:
	TVMStatus VMStart -	started
	TVMMainEntry VMLoadModule -	GIVEN
	void VMUnloadModule - GIVEN
	TVMStatus VMThreadCreate - not started
	TVMStatus VMThreadDelete - not started
	TVMStatus VMThreadActivate - not started
	TVMStatus VMThreadTerminate - not started
	TVMStatus VMThreadID - not started
	TVMStatus VMThreadState - not started
	TVMStatus VMThreadSleep - started
	TVMStatus VMMutexCreate - not started
	TVMStatus VMMutexDelete - not started
	TVMStatus VMMutexQuery - not started
	TVMStatus VMMutexAcquire - not started
	TVMStatus VMMutexRelease - not started
	TVMStatus VMFileOpen - not started
	TVMStatus VMFileClose - not started  
	TVMStatus VMFileRead - not started
	TVMStatus VMFileWrite - started
	TVMStatus VMFileSeek - not started
	TVMStatus VMFilePrint - GIVEN
	In order to remove all system V messages: 
	1. ipcs //to see msg queue
	2. ipcs | grep q | awk '{print "ipcrm -q "$2""}' | xargs -0 bash -c
	3. ipcs //should be clear now
*/

#include "VirtualMachine.h"
#include "Machine.h"
#include <vector>
#include <iostream>
using namespace std;

extern "C"
{
typedef void (*TVMMain)(int argc, char *argv[]); //function ptr
TVMMainEntry VMLoadModule(const char *module); //load module spec
volatile uint16_t globaltick = 0; //vol ticks

class TCB
{
	public:
		TVMThreadID threadID; //to hold the threads ID, may be redundant, but might be easier to get
		TVMThreadPriority threadPrior; //for the threads priority
		TVMThreadState threadState; //for thread stack
		TVMMemorySize threadMemSize; //for stack size
		uint8_t *ptr; //this or another byte size type pointer for base of stack
		TVMThreadEntry threadEntry; //for the threads entry function
		void *vptr; //for the threads entry parameter
		SMachineContext SMC; //for the context to switch to/from the thread
		TVMTick ticker; //for the ticks that thread needs to wait
		//possibly need something to hold file return type
		//possibly hold a pointer or ID of mutex waiting on
		//possibly hold a list of held mutexes
}; //class TCB

vector<TCB> threadList; //list to hold threads

void AlarmCallBack(void *param, int result)
{
	globaltick--; //dec tick time
} //myFileCallBack()

void Skeleton(void* thread)
{
  printf("inside skeleton\n");
  //deal with thread, call VMThreadTerminate when thread returns
} //Skeleton()

TVMStatus VMStart(int tickms, int machinetickms, int argc, char *argv[])
{
	TVMMain VMMain = VMLoadModule(argv[0]); //load the module
	MachineInitialize(tickms); //initialize the machine with specified time
	useconds_t usec = tickms * 1000; //usec in microseconds
	MachineRequestAlarm(usec, (TMachineAlarmCallback)AlarmCallBack, NULL); //starts the alarm tick
	MachineEnableSignals(); //start the signals

	if(VMMain == NULL) //fail to load module
		return 0;

	else //load successful
	{
		//TCB mainThread; //create thread for VMMain
		VMMain(argc, argv); //function call to start TVMMain
		return VM_STATUS_SUCCESS;
	}
} //TVMStatus VMStart()

TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, 
	TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid)
{
	if(entry == NULL || tid == NULL) //invalid
		return VM_STATUS_ERROR_INVALID_PARAMETER;

	TMachineSignalState OldState; //local variable to suspend
	MachineSuspendSignals(&OldState); //suspend signals in order to create thread

	TCB VMMainThread; //start main thread
	VMMainThread.threadEntry = entry;
	VMMainThread.threadMemSize = memsize;
	VMMainThread.threadPrior = prio;
	VMMainThread.threadID = *tid;
	cout<<"before vector store" << endl;
	//threadList[0] = VMMainThread; //put it into the vector to store ERROR HERE
	cout << "did the thread class & vector store" << endl;
	
	SMachineContext context;
	uint8_t* stackaddr = new uint8_t[memsize];
	MachineContextCreate(&context, Skeleton, NULL, stackaddr, memsize);
	//MachineContextSwitch(&context, (void*)Skeleton); //switch to the new context here
	
	MachineResumeSignals(&OldState); //resume signals after creating thread
	return VM_STATUS_SUCCESS;
	//return 0;
} //TVMStatus VMThreadCreate()

TVMStatus VMThreadDelete(TVMThreadID thread)
{return 0;} //TVMStatus VMThreadDelete()

TVMStatus VMThreadActivate(TVMThreadID thread)
{
	//TMachineSignalState OldState; //local variable to suspend
	//MachineSuspendSignals(&OldState); //suspend signals in order to create thread
	//MachineResumeSignals(&OldState); //resume signals after creating thread
	return 0;
} //TVMStatus VMThreadActivate()

TVMStatus VMThreadTerminate(TVMThreadID thread)
{return 0;} //TVMStatus VMThreadTerminate()

TVMStatus VMThreadID(TVMThreadIDRef threadref)
{
	if(threadref) //successful
		return VM_STATUS_SUCCESS;

	else if(!threadref) //thread does not exist
		return VM_STATUS_ERROR_INVALID_ID;

	else if(threadref == NULL) //invalid
		return VM_STATUS_ERROR_INVALID_PARAMETER;

	return 0;
} //TVMStatus VMThreadID()

TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef stateref)
{
	if(thread) //successful
		//place thead into spec thread state
		return VM_STATUS_SUCCESS;

	else if(!thread) //thread does not exist
		return VM_STATUS_ERROR_INVALID_ID;

	else if(stateref == NULL) //invalid
		return VM_STATUS_ERROR_INVALID_PARAMETER;

	return 0;
} //TVMStatus VMThreadState()

TVMStatus VMThreadSleep(TVMTick tick)
{
	if(tick == VM_TIMEOUT_INFINITE) //invalid
		return VM_STATUS_ERROR_INVALID_PARAMETER;

	globaltick = tick; //set tick as globaltick

	while (globaltick > 0) {}; //go to sleep until reaches zero
	return VM_STATUS_SUCCESS; //success sleep after reaches zero
} //TVMStatus VMThreadSleep()

TVMStatus VMMutexCreate(TVMMutexIDRef mutexref)
{return 0;} //TVMStatus VMMutexCreate()

TVMStatus VMMutexDelete(TVMMutexID mutex)
{return 0;} //TVMStatus VMMutexDelete()

TVMStatus VMMutexQuery(TVMMutexID mutex, TVMThreadIDRef ownerref)
{return 0;} //TVMStatus VMMutexQuery()

TVMStatus VMMutexAcquire(TVMMutexID mutex, TVMTick timeout)
{return 0;} //TVMStatus VMMutexAcquire()

TVMStatus VMMutexRelease(TVMMutexID mutex)
{return 0;} //TVMStatus VMMutexRelease()

TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor)
{return 0;} //TVMStatus VMFileOpen()

TVMStatus VMFileClose(int filedescriptor)
{return 0;} //TVMStatus VMFileClose()

TVMStatus VMFileRead(int filedescriptor, void *data, int *length)
{return 0;} //TVMStatus VMFileRead()

TVMStatus VMFileWrite(int filedescriptor, void *data, int *length)
{
	if(data == NULL || length == NULL) //invalid input
		return VM_STATUS_ERROR_INVALID_PARAMETER;

	int writer = write(filedescriptor, data, *length); //write to file

	if(writer == -1) //fail to write
		return VM_STATUS_FAILURE;

	else //write successful
		return VM_STATUS_SUCCESS;
} //TVMStatus VMFileWrite()

TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, int *newoffset)
{return 0;} //TVMStatus VMFileSeek()
} //extern "C"