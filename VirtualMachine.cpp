/*A virtual machine for ECS 150
	Filename: VirtualMachine.cpp
	Authors: John Garcia, Felix Ng

	In this version:
	TVMStatus VMStart -				done
	TVMStatus VMThreadCreate - 		done
	TVMStatus VMThreadDelete - 		done
	TVMStatus VMThreadActivate - 	started
	TVMStatus VMThreadTerminate - 	started
	TVMStatus VMThreadID - 			done
	TVMStatus VMThreadState - 		done
	TVMStatus VMThreadSleep - 		done
	TVMStatus VMMutexCreate - 		not started
	TVMStatus VMMutexDelete - 		not started
	TVMStatus VMMutexQuery - 		not started
	TVMStatus VMMutexAcquire - 		not started
	TVMStatus VMMutexRelease - 		not started
	TVMStatus VMFileOpen - 			not started
	TVMStatus VMFileClose - 		not started  
	TVMStatus VMFileRead - 			not started
	TVMStatus VMFileWrite - 		done
	TVMStatus VMFileSeek - 			not started
	TVMStatus VMFilePrint - 		GIVEN

	In order to remove all system V messages: 
	1. ipcs //to see msg queue
	2. type this in cmd line: ipcs | grep q | awk '{print "ipcrm -q "$2""}' | xargs -0 bash -c
	3. ipcs //should be clear now
*/

#include "VirtualMachine.h"
#include "Machine.h"
#include <vector>
#include <queue>
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
TCB *currentThread = new TCB; //global current running thread
vector<TCB*> threadList; //global vector list to hold threads
vector<TCB*> sleepList; // sleep queue
queue<TCB*> highPrio; //high priority queue
queue<TCB*> normPrio; //normal priority queue
queue<TCB*> lowPrio; //low priority queue
void pushThread(TCB*);
void Scheduler();

void AlarmCallBack(void *param, int result)
{
	for(vector<TCB*>::iterator itr = sleepList.begin(); itr != sleepList.end(); ++itr)
  	{
    	if((*itr)->ticker > 0) //if still more ticks
      		(*itr)->ticker--;
    	else
    	{
      		(*itr)->threadState = VM_THREAD_STATE_READY; //set to ready
      		idle->threadState = VM_THREAD_STATE_WAITING; //idle is waiting
      		pushThread(*itr); //push it into its proper queue
      		sleepList.erase(itr); //erase from sleeping 
            break;
    	}
  	}
  	Scheduler();
} //AlarmCallBack()

void FileCallBack(void *param, int result)
{
	currentThread->threadState = VM_THREAD_STATE_WAITING;
	pushThread((TCB*)param);
} //FileCallBack()

void Skeleton(void* param)
{
  	currentThread->threadEntry(param); //deal with thread
  	VMThreadTerminate(currentThread->threadID); //terminate thread
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

void pushThread(TCB *myThread)
{
	if(myThread->threadPrior == VM_THREAD_PRIORITY_HIGH)
		highPrio.push(myThread); //push into high prio queue
	if(myThread->threadPrior == VM_THREAD_PRIORITY_NORMAL)
		normPrio.push(myThread); //push into norm prio queue
	if(myThread->threadPrior == VM_THREAD_PRIORITY_LOW)
		lowPrio.push(myThread); //push into low prio queue
} //pushThread()

TCB *findThread(TVMThreadID thread)
{
	vector<TCB*>::iterator itr;
	for(itr = threadList.begin(); itr != threadList.end(); ++itr)
	{
		if(thread  == (*itr)->threadID)
			return (*itr); //thread does exist
	} //iterate though the list of threads
	return NULL; //thread does not exist
} //findThread()

void Scheduler()
{
	if(currentThread->threadState == VM_THREAD_STATE_WAITING 
		|| currentThread->threadState == VM_THREAD_STATE_DEAD)
	{
		int flag = 0;
		TCB *newThread = new TCB;
		if(!highPrio.empty())
    	{
    		newThread = highPrio.front();
    		highPrio.pop();
    		flag = 1;
    	} //high prior thread check

    	else if(!normPrio.empty())
    	{
    		newThread = normPrio.front();
    		normPrio.pop();
    		flag = 1;
    	} //normal prior thread check

    	else if(!lowPrio.empty())
    	{
    		newThread = lowPrio.front();
    		lowPrio.pop();
    		flag = 1;
    	} //low prior thread check

    	else
    	{
    		newThread = idle;
    		flag = 1;
    	} //if all else fails run idle

    	if(flag)
    	{
			//if(currentThread->threadState == VM_THREAD_STATE_READY)
			//	pushThread(currentThread); //push into its proper priority
			TCB *oldThread = currentThread; //findThread(currentThread->threadID);
			currentThread = newThread; //update current thread
			newThread->threadState = VM_THREAD_STATE_RUNNING; //set to running
			MachineContextSwitch(&(oldThread)->SMC, &(newThread)->SMC); //switch contexts
		}
	}
} //Scheduler()

TVMStatus VMStart(int tickms, int machinetickms, int argc, char *argv[])
{
	TVMMain VMMain = VMLoadModule(argv[0]); //load the module
	MachineInitialize(tickms); //initialize the machine with specified time
	useconds_t usec = tickms * 1000; //usec in microseconds
	MachineRequestAlarm(usec, (TMachineAlarmCallback)AlarmCallBack, NULL); //starts alarm tick
	MachineEnableSignals(); //start the signals

	if(VMMain == NULL) 
		return 0; //fail to load module
	else //load successful
	{
		TCB *VMMainTCB = new TCB; //start main thread
		VMMainTCB->threadID = 1;
		VMMainTCB->threadPrior = VM_THREAD_PRIORITY_NORMAL;
		VMMainTCB->threadState = VM_THREAD_STATE_RUNNING;
		currentThread = VMMainTCB; //current thread is now main

		idle->threadID = 0; //idle thread first in array of threads
		idle->threadState = VM_THREAD_STATE_DEAD;
		idle->threadPrior = VM_THREAD_PRIORITY_LOW;
		idle->threadEntry = idleFunction;
		uint8_t *stacker = new uint8_t[0x100000];
		idle->base = stacker; //base for idle stack
		MachineContextCreate(&(idle)->SMC, idle->threadEntry, NULL, 
			stacker, 0x100000);

		threadList.push_back(idle); //pos 0 in threadList
		threadList.push_back(VMMainTCB); //pos 1 in threadList
		VMMain(argc, argv); //function call to start TVMMain
		return VM_STATUS_SUCCESS;
	}
} //VMStart()

TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, 
	TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid)
{
	TMachineSignalState OldState; //local variable to suspend
	MachineSuspendSignals(&OldState); //suspend signals

	if(entry == NULL || tid == NULL) //invalid
		return VM_STATUS_ERROR_INVALID_PARAMETER;

	uint8_t *stack = new uint8_t[memsize]; //array of threads treated as a stack
	TCB *newThread = new TCB; //start new thread
	newThread->threadEntry = entry;
	newThread->threadMemSize = memsize;
	newThread->threadPrior = prio;
	newThread->base = stack;
	newThread->threadState = VM_THREAD_STATE_DEAD;
	newThread->threadID = *tid = threadList.size();
	threadList.push_back(newThread); //store in vector of ptrs
	
	MachineResumeSignals(&OldState); //resume signals
	return VM_STATUS_SUCCESS;
} //VMThreadCreate()

TVMStatus VMThreadDelete(TVMThreadID thread)
{
	TMachineSignalState OldState; //local variable to suspend signals
	MachineSuspendSignals(&OldState); //suspend signals
	
	vector<TCB*>::iterator itr;
	for(itr = threadList.begin(); itr != threadList.end(); ++itr)
	{
		if(thread  == (*itr)->threadID) //thread exists check
		{
			if((*itr)->threadState != VM_THREAD_STATE_DEAD)
				return VM_STATUS_ERROR_INVALID_STATE; //not in dead state check
			else
			{
				threadList.erase(itr); //delete the thread
				break; //we found it now lets get outta here
			}
		}
		if(itr == threadList.end()-1) //thread does not exist check
			return VM_STATUS_ERROR_INVALID_ID;
	} //iterate though the list of threads

	MachineResumeSignals(&OldState); //resume signals
	return VM_STATUS_SUCCESS;
} //VMThreadDelete()

TVMStatus VMThreadActivate(TVMThreadID thread)
{
	TMachineSignalState OldState; //local variable to suspend signals
	MachineSuspendSignals(&OldState); //suspend signals

	TCB *myThread = findThread(thread); //call to find the thread id
	if(myThread == NULL) //check if thread exists
		return VM_STATUS_ERROR_INVALID_ID;
	if(myThread->threadState != VM_THREAD_STATE_DEAD) //dead state check
		return VM_STATUS_ERROR_INVALID_STATE;

	MachineContextCreate(&(myThread)->SMC, Skeleton, myThread->vptr, 
		myThread->base, myThread->threadMemSize); //create context here
	myThread->threadState = VM_THREAD_STATE_READY; //set current thread to ready

	if(currentThread->threadPrior >= myThread->threadPrior)
	{
		pushThread(myThread);
		Scheduler();
	}

	MachineResumeSignals(&OldState); //resume signals
	return VM_STATUS_SUCCESS;
} //VMThreadActivate()

TVMStatus VMThreadTerminate(TVMThreadID thread)
{
	TMachineSignalState OldState; //local variable to suspend signals
	MachineSuspendSignals(&OldState); //suspend signals
	
	TCB *myThread = findThread(thread);
	if(myThread == NULL) //check if thread exists
		return VM_STATUS_ERROR_INVALID_ID;
	if(myThread->threadState == VM_THREAD_STATE_DEAD) //dead state check
		return VM_STATUS_ERROR_INVALID_STATE;
	myThread->threadState = VM_THREAD_STATE_DEAD; //set to die
	Scheduler();

	MachineResumeSignals(&OldState); //resume signals
	return VM_STATUS_SUCCESS;
} //VMThreadTerminate()

TVMStatus VMThreadID(TVMThreadIDRef threadref)
{
	if(threadref == NULL) //invalid
		return VM_STATUS_ERROR_INVALID_PARAMETER;

	*threadref = currentThread->threadID;

	/*vector<TCB*>::iterator itr;
	for(itr = threadList.begin(); itr != threadList.end(); ++itr)
	{
		if(*threadref  == (*itr)->threadID) //thread does exist
			break;

		else if(itr == threadList.end()-1) //thread does not exist
			return VM_STATUS_ERROR_INVALID_ID;
	} //iterate though the list of threads*/

	return VM_STATUS_SUCCESS; //successful retrieval
} //VMThreadID()

TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef stateref)
{
	if(stateref == NULL) //invalid
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	
	vector<TCB*>::iterator itr;
	for(itr = threadList.begin(); itr != threadList.end(); ++itr)
	{
		if((*itr)->threadID == thread)
		{
			*stateref = (*itr)->threadState; //assign thread state here
			return VM_STATUS_SUCCESS;
		}
	} //iterate through the entire thread list
	
	return VM_STATUS_ERROR_INVALID_ID; //thread does not exist
} //VMThreadState()

TVMStatus VMThreadSleep(TVMTick tick)
{
	TMachineSignalState OldState; //local variable to suspend signals
  	MachineSuspendSignals(&OldState); //suspend signals

  	if(tick == VM_TIMEOUT_INFINITE) //invalid
    	return VM_STATUS_ERROR_INVALID_PARAMETER;
 
  	currentThread->threadState = VM_THREAD_STATE_WAITING;
  	currentThread->ticker = tick; //set tick as globaltick
  	sleepList.push_back(currentThread);
  	Scheduler();

  	MachineResumeSignals(&OldState); //resume signals
  	return VM_STATUS_SUCCESS; //success sleep after reaches zero
} //VMThreadSleep()

TVMStatus VMMutexCreate(TVMMutexIDRef mutexref)
{return 0;} //VMMutexCreate()

TVMStatus VMMutexDelete(TVMMutexID mutex)
{return 0;} //VMMutexDelete()

TVMStatus VMMutexQuery(TVMMutexID mutex, TVMThreadIDRef ownerref)
{return 0;} //VMMutexQuery()

TVMStatus VMMutexAcquire(TVMMutexID mutex, TVMTick timeout)
{return 0;} //VMMutexAcquire()

TVMStatus VMMutexRelease(TVMMutexID mutex)
{return 0;} //VMMutexRelease()

TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor)
{
	TMachineSignalState OldState; //local variable to suspend signals
  	MachineSuspendSignals(&OldState); //suspend signals

	if(filename == NULL || filedescriptor == NULL)
		return VM_STATUS_ERROR_INVALID_PARAMETER;

	MachineFileOpen(filename, flags, mode, FileCallBack, currentThread); //machine opens file
	currentThread->threadState = VM_THREAD_STATE_WAITING;
	Scheduler();

	MachineResumeSignals(&OldState); //resume signals
	return VM_STATUS_SUCCESS;
} //VMFileOpen()

TVMStatus VMFileClose(int filedescriptor)
{return 0;} //VMFileClose()

TVMStatus VMFileRead(int filedescriptor, void *data, int *length)
{return 0;} //VMFileRead()

TVMStatus VMFileWrite(int filedescriptor, void *data, int *length)
{
	if(data == NULL || length == NULL) //invalid input
		return VM_STATUS_ERROR_INVALID_PARAMETER;

	if(write(filedescriptor, data, *length) > -1) //write to file
		return VM_STATUS_SUCCESS;

	else //failed to write
		return VM_STATUS_FAILURE;
} //VMFileWrite()

TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, int *newoffset)
{return 0;} //VMFileSeek()
} //extern "C"