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
	2. type this in cmd line: ipcs | grep q | awk '{print "ipcrm -q "$2""}' | xargs -0 bash -c
	3. ipcs //should be clear now
*/

#include "VirtualMachine.h"
#include "Machine.h"
#include <vector>
#include <iostream>
using namespace std;

extern "C"
{
class TCB
{
	public:
		TVMThreadID threadID; //to hold the threads ID
		TVMThreadPriority threadPrior; //for the threads priority
		TVMThreadState threadState; //for thread stack
		TVMMemorySize threadMemSize; //for stack size
		uint8_t *base; //this or another byte size type pointer for base of stack
		TVMThreadEntry threadEntry; //for the threads entry function
		void *vptr; //for the threads entry parameter
		SMachineContext SMC; //for the context to switch to/from the thread
		TVMTick ticker; //for the ticks that thread needs to wait
		//possibly need something to hold file return type
		//possibly hold a pointer or ID of mutex waiting on
		//possibly hold a list of held mutexes
}; //class TCB

typedef void (*TVMMain)(int argc, char *argv[]); //function ptr
TVMMainEntry VMLoadModule(const char *module); //load module spec
volatile uint16_t globaltick = 0; //vol ticks
TCB *idle = new TCB; //global idle thread
vector<TCB*> threadList; //global ptr list to hold threads

void AlarmCallBack(void *param, int result)
{
	globaltick--; //dec tick time
} //myFileCallBack()

void Skeleton(void* param)
{
  printf("inside skeleton bitch\n");
  //deal with thread, call VMThreadTerminate when thread returns
} //Skeleton()

void idleFunction(void* TCBref)
{
	TMachineSignalState OldState; //a state
    MachineEnableSignals(); //start the signals
    while(1)
    {
        MachineSuspendSignals(&OldState);
        MachineResumeSignals(&OldState);
    } //this is idling while we are in the idle state
} //idleFunction()

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
		TCB *VMMainTCB = new TCB; //start main thread
		VMMainTCB->threadID = 1;
		VMMainTCB->threadPrior = VM_THREAD_PRIORITY_NORMAL;
		VMMainTCB->threadState = VM_THREAD_STATE_RUNNING;

		idle->threadID = 0; //idle thread first in array of threads
		idle->threadState = VM_THREAD_STATE_DEAD;
		idle->threadEntry = idleFunction;

		threadList.push_back(idle);
		threadList.push_back(VMMainTCB);
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

	uint8_t *stack = new uint8_t[memsize]; //array of threads treated as a stack

	TCB *newThread = new TCB; //start new thread
	newThread->threadEntry = entry;
	newThread->threadMemSize = memsize;
	newThread->threadPrior = prio;
	newThread->base = stack;
	newThread->threadID = *tid;
	threadList.push_back(newThread); //store in vector of ptrs
	
	MachineResumeSignals(&OldState); //resume signals after creating thread
	return VM_STATUS_SUCCESS;
} //TVMStatus VMThreadCreate()

TVMStatus VMThreadDelete(TVMThreadID thread)
{return 0;} //TVMStatus VMThreadDelete()

TVMStatus VMThreadActivate(TVMThreadID thread)
{
	vector<TCB*>::iterator itr;
	for(itr = threadList.begin(); itr != threadList.end(); ++itr)
	{
		if(thread  == (*itr)->threadID) //thread does exist
		{
			if((*itr)->threadState == VM_THREAD_STATE_DEAD) //dead state check
				return VM_STATUS_ERROR_INVALID_STATE;
			break;
		}

		else if(itr == threadList.end()-1) //thread does not exist
			return VM_STATUS_ERROR_INVALID_ID;
	} //iterate through the entire thread list

	TMachineSignalState OldState; //local variable to suspend signals
	MachineSuspendSignals(&OldState); //suspend signals in order to create thread

	//MachineContextCreate(&TCB->SMC, Skeleton, TCB, TCB->stack, TCB->size); //prof
	MachineContextCreate(&threadList.at(thread)->SMC, Skeleton, NULL, 
		threadList.at(thread)->base, threadList.at(thread)->threadMemSize);
	threadList.at(thread)->threadState = VM_THREAD_STATE_RUNNING; //set current thread running
	MachineContextSwitch(&threadList[0]->SMC, &threadList.at(thread)->SMC); //switch to new context here

	MachineResumeSignals(&OldState); //resume signals after creating thread
	return VM_STATUS_SUCCESS;
} //TVMStatus VMThreadActivate()

TVMStatus VMThreadTerminate(TVMThreadID thread)
{return 0;} //TVMStatus VMThreadTerminate()

TVMStatus VMThreadID(TVMThreadIDRef threadref)
{
	if(threadref == NULL) //invalid
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	return 0;
} //TVMStatus VMThreadID()

TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef stateref)
{
	if(stateref == NULL) //invalid
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	
	vector<TCB*>::iterator itr;
	for(itr = threadList.begin(); itr != threadList.end(); ++itr)
	{			// iterate through entire threadlist
		if((*itr)->threadID == thread)
		{
			stateref = &(*itr)->threadState;
			return VM_STATUS_SUCCESS;
		}
	} //iterate through the entire thread list
	
	return VM_STATUS_ERROR_INVALID_ID; //thread does not exist
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