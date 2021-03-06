/*	A virtual machine for ECS 150
	Filename: VirtualMachine.cpp
	Authors: John Garcia, Felix Ng

	In this version:
	TVMStatus VMStart -				done
	TVMStatus VMThreadCreate - 		done
	TVMStatus VMThreadDelete - 		done
	TVMStatus VMThreadActivate - 	done
	TVMStatus VMThreadTerminate - 	done
	TVMStatus VMThreadID - 			done
	TVMStatus VMThreadState - 		done
	TVMStatus VMThreadSleep - 		done
	TVMStatus VMMutexCreate - 		not started
	TVMStatus VMMutexDelete - 		not started
	TVMStatus VMMutexQuery - 		not started
	TVMStatus VMMutexAcquire - 		not started
	TVMStatus VMMutexRelease - 		not started
	TVMStatus VMFileOpen - 			done
	TVMStatus VMFileClose - 		done  
	TVMStatus VMFileRead - 			done
	TVMStatus VMFileWrite - 		done
	TVMStatus VMFileSeek - 			done
	TVMStatus VMFilePrint - 		GIVEN

	In order to remove all system V messages: 
	1. ipcs //to see msg queue
	2. type this in cmd line: ipcs | grep q | awk '{print "ipcrm -q "$2""}' | xargs -0 bash -c
	3. ipcs //should be clear now

	In order to remove vm from activity moniter:
	type in command line: killall -9 vm
*/

/*	Mutex output:
	VMMain creating threads.
	VMMain creating mutexes.
	VMMain locking mutexes.
	VMMain activating processes.
	VMThreadHigh Alive
	VMThreadMedium Alive
	VMMain releasing mutexes.
	VMThreadLow Alive
	VMThreadHigh Awake
	VMThreadMedium Awake
	VMMain acquiring main mutex.
	VMThreadLow Awake
	VMMain acquired main mutex.
	Goodbye
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
	int fileResult; //to hold file return type
}; //class TCB - Thread Control Block

class MB
{
	public:
	TVMMutexID id;
	TVMMutexIDRef idref;
	TVMTick ticker;
	queue<TCB*> high;
	queue<TCB*> medium;
	queue<TCB*> low;
	TCB *ownerThread;
}; //class MB - Mutex Block

typedef void (*TVMMain)(int argc, char *argv[]); //function ptr
TVMMainEntry VMLoadModule(const char *module); //load module spec
volatile uint16_t globaltick = 0; //vol ticks
TCB *idle = new TCB; //global idle thread
TCB *currentThread = new TCB; //global current running thread
vector<TCB*> threadList; //global vector list to hold threads
vector<TCB*> sleepList; // sleep queue
vector<MB*> mutexList; //list for mutex
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
	((TCB*)param)->fileResult = result;
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
	else if(myThread->threadPrior == VM_THREAD_PRIORITY_NORMAL)
		normPrio.push(myThread); //push into norm prio queue
	else if(myThread->threadPrior == VM_THREAD_PRIORITY_LOW)
		lowPrio.push(myThread); //push into low prio queue
} //pushThread()

void pushMutex(MB *myMutex)
{
	if(currentThread->threadPrior == VM_THREAD_PRIORITY_HIGH)
		myMutex->high.push(currentThread); //push into high prio queue
	else if(currentThread->threadPrior == VM_THREAD_PRIORITY_NORMAL)
		myMutex->medium.push(currentThread); //push into norm prio queue
	else if(currentThread->threadPrior == VM_THREAD_PRIORITY_LOW)
		myMutex->low.push(currentThread); //push into low prio queue
} //pushMutex()

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

MB *findMutex(TVMMutexID mid)
{
	vector<MB*>::iterator itr;
	for(itr = mutexList.begin(); itr != mutexList.end(); ++itr)
	{
		if((*itr)->id == mid)
			return (*itr); //mutex does exits
	} //iterate through the mutex list
	return NULL; //mutex does not exist
} //findMutex()

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

void SchedulerMutex(MB *myMutex)
{
	if(myMutex->ownerThread == NULL)
	{
		if(!myMutex->high.empty())
    	{
    		myMutex->ownerThread = myMutex->high.front();
    		myMutex->high.pop();
    		cout << "high" << endl;
    	} //high prior thread check

    	else if(!myMutex->medium.empty())
    	{
    		myMutex->ownerThread = myMutex->medium.front();
    		myMutex->medium.pop();
    		cout << "medium" << endl;
    	} //normal prior thread check

    	else if(!lowPrio.empty())
    	{
    		myMutex->ownerThread = myMutex->low.front();
    		myMutex->low.pop();
    		cout << "low" << endl;
    	} //low prior thread check
	}
}

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

	vector<TCB*>::iterator itr;
	for(itr = threadList.begin(); itr != threadList.end(); ++itr)
	{
		if(*threadref  == (*itr)->threadID) //thread does exist
			break;

		else if(itr == threadList.end()-1) //thread does not exist
			return VM_STATUS_ERROR_INVALID_ID;
	} //iterate though the list of threads

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
{
	TMachineSignalState OldState; //local variable to suspend signals
  	MachineSuspendSignals(&OldState); //suspend signals

  	if(mutexref == NULL)
  		return VM_STATUS_ERROR_INVALID_PARAMETER;

	MB *Mutex = new MB;
	Mutex->id = mutexList.size(); //allocate size for mutex
	mutexList.push_back(Mutex);
	*mutexref = Mutex->id; //set to have same id

	MachineResumeSignals(&OldState); //resume signals
	return VM_STATUS_SUCCESS;
} //VMMutexCreate()

TVMStatus VMMutexDelete(TVMMutexID mutex)
{return 0;} //VMMutexDelete()

TVMStatus VMMutexQuery(TVMMutexID mutex, TVMThreadIDRef ownerref)
{return 0;} //VMMutexQuery()

TVMStatus VMMutexAcquire(TVMMutexID mutex, TVMTick timeout)
{
	TMachineSignalState OldState; //local variable to suspend signals
  	MachineSuspendSignals(&OldState); //suspend signals

  	MB *myMutex = findMutex(mutex); //finds the mutex id
  	if(myMutex == NULL)
  		return VM_STATUS_ERROR_INVALID_ID; //mutex does not exist

	myMutex->ticker = timeout;
  	pushMutex(myMutex); //push the currentThread into its proper mutex

  	if(myMutex->ticker == VM_TIMEOUT_INFINITE)
  	{
  		while(myMutex->ownerThread != NULL) {}; //wait here until id acquired
  	}

  	else
  	{
  		while(myMutex->ticker > 0) {}; //just keep waiting then until done
  	}

  	SchedulerMutex(myMutex);

  	MachineResumeSignals(&OldState); //resume signals
	return VM_STATUS_SUCCESS;
} //VMMutexAcquire()

TVMStatus VMMutexRelease(TVMMutexID mutex)
{
	TMachineSignalState OldState; //local variable to suspend signals
  	MachineSuspendSignals(&OldState); //suspend signals

  	MB *myMutex = findMutex(mutex); //finds the mutex id
	if(myMutex == NULL)
  		return VM_STATUS_ERROR_INVALID_ID; //mutex does not exist
  	if(myMutex->ownerThread != currentThread)
  		return VM_STATUS_ERROR_INVALID_STATE;//needs to be currently held by running thread
  	myMutex->ownerThread = NULL; //set to NULL
  	SchedulerMutex(myMutex); //schedule for next mutex

  	MachineResumeSignals(&OldState); //resume signals
	return 0;
} //VMMutexRelease()

TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor)
{
	TMachineSignalState OldState; //local variable to suspend signals
  	MachineSuspendSignals(&OldState); //suspend signals

	if(filename == NULL || filedescriptor == NULL)
		return VM_STATUS_ERROR_INVALID_PARAMETER;

	MachineFileOpen(filename, flags, mode, FileCallBack, currentThread); //machine opens file
	currentThread->threadState = VM_THREAD_STATE_WAITING;
	Scheduler();
	*filedescriptor = currentThread->fileResult;

	MachineResumeSignals(&OldState); //resume signals
	if(currentThread->fileResult < 0) //failure check
		return VM_STATUS_FAILURE;
	return VM_STATUS_SUCCESS;
} //VMFileOpen()

TVMStatus VMFileClose(int filedescriptor)
{
	TMachineSignalState OldState; //local variable to suspend signals
  	MachineSuspendSignals(&OldState); //suspend signals

  	MachineFileClose(filedescriptor, FileCallBack, currentThread);
  	currentThread->threadState = VM_THREAD_STATE_WAITING;
  	Scheduler();

  	MachineResumeSignals(&OldState); //resume signals
	return VM_STATUS_SUCCESS;
} //VMFileClose()

TVMStatus VMFileRead(int filedescriptor, void *data, int *length)
{
	TMachineSignalState OldState; //local variable to suspend signals
  	MachineSuspendSignals(&OldState); //suspend signals

	if(data == NULL || length == NULL)
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	
	MachineFileRead(filedescriptor, data, *length, FileCallBack, currentThread);
	currentThread->threadState = VM_THREAD_STATE_WAITING;
	Scheduler();
	*length = currentThread->fileResult;

	MachineResumeSignals(&OldState); //resume signals
	if(currentThread->fileResult < 0)
		return VM_STATUS_FAILURE;
	return VM_STATUS_SUCCESS;
} //VMFileRead()

TVMStatus VMFileWrite(int filedescriptor, void *data, int *length)
{
	TMachineSignalState OldState; //local variable to suspend signals
  	MachineSuspendSignals(&OldState); //suspend signals

  	if(data == NULL || length == NULL)
		return VM_STATUS_ERROR_INVALID_PARAMETER;

  	MachineFileWrite(filedescriptor, data, *length, FileCallBack, currentThread);
  	currentThread->threadState = VM_THREAD_STATE_WAITING;
  	Scheduler();
  	*length = currentThread->fileResult;
  	
  	MachineResumeSignals(&OldState); //resume signals
  	if(currentThread->fileResult < 0)
		return VM_STATUS_FAILURE;
	return VM_STATUS_SUCCESS;
} //VMFileWrite()

TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, int *newoffset)
{
	TMachineSignalState OldState; //local variable to suspend signals
  	MachineSuspendSignals(&OldState); //suspend signals

  	MachineFileSeek(filedescriptor, offset, whence, FileCallBack, currentThread);
  	currentThread->threadState = VM_THREAD_STATE_WAITING;
  	Scheduler();
  	*newoffset = currentThread->fileResult;
  	
  	MachineResumeSignals(&OldState); //resume signals
  	if(currentThread->fileResult < 0)
		return VM_STATUS_FAILURE;
	return VM_STATUS_SUCCESS;
} //VMFileSeek()
} //extern "C"