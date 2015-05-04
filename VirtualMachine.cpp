/*	A virtual machine for ECS 150
	Filename: VirtualMachine.cpp
	Authors: John Garcia, Felix Ng

	In this version:
	TVMStatus VMStart -				done
	TVMMainEntry VMLoadModule -		GIVEN
	void VMUnloadModule - 			GIVEN
	TVMStatus VMThreadCreate - 		done
	TVMStatus VMThreadDelete - 		done
	TVMStatus VMThreadActivate - 	done
	TVMStatus VMThreadTerminate - 	done
	TVMStatus VMThreadID - 			done
	TVMStatus VMThreadState - 		done
	TVMStatus VMThreadSleep - 		done
	TVMStatus VMMutexCreate - 		done
	TVMStatus VMMutexDelete - 		done
	TVMStatus VMMutexQuery - 		done
	TVMStatus VMMutexAcquire - 		done
	TVMStatus VMMutexRelease - 		done
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
		int fileResult;//possibly need something to hold file return type
		//possibly hold a pointer or ID of mutex waiting on
		//possibly hold a list of held mutexes
}; //class TCB

class MB 
{
	public:
		TVMMutexID mutexID; 	// holds mutex ID
		TVMMutexIDRef mutexIDRef;
		TCB *ownerThread;
		TVMTick ticker;
		queue<TCB*> highQ;
		queue<TCB*> medQ;
		queue<TCB*> lowQ;
}; //class mutex Block


void pushThread(TCB*);
void pushMutex(MB*);
void Scheduler();
typedef void (*TVMMain)(int argc, char *argv[]); //function ptr
TVMMainEntry VMLoadModule(const char *module); //load module spec
TCB *idle = new TCB; //global idle thread
TCB *currentThread = new TCB; //global current running thread
vector<MB*> mutexList;
vector<TCB*> threadList; //global ptr list to hold threads
queue<TCB*> highPrio; //high priority queue
queue<TCB*> normPrio; //normal priority queue
queue<TCB*> lowPrio; //low priority queue
vector<TCB*> sleepList; // sleep queue
vector<MB*> mutexSleepList;

void AlarmCallBack(void *param, int result)
{
	//cout << "sleepList size: " << sleepList.size() << endl;

	//check sleep thread
	for(vector<TCB*>::iterator itr = sleepList.begin(); itr != sleepList.end(); ++itr)
	{
		if((*itr)->ticker > 0)		// if still more ticks
			(*itr)->ticker--;
		else {
			(*itr)->threadState = VM_THREAD_STATE_READY;
			idle->threadState = VM_THREAD_STATE_WAITING;
			pushThread(*itr);
			sleepList.erase(itr);
			break;
		}
	}


	//check sleep mutex
	for(vector<MB*>::iterator itr = mutexSleepList.begin(); itr != mutexSleepList.end(); ++itr)
	{
		if((*itr)->ticker == VM_TIMEOUT_INFINITE) {		// if infinite, break iff ownerThread == NULL
			if((*itr)->ownerThread == NULL) {
				idle->threadState = VM_THREAD_STATE_WAITING;
				pushMutex(*itr);
				mutexSleepList.erase(itr);
				break;
			}
		} else {	// finite
			if((*itr)->ticker > 0 && (*itr)->ownerThread != NULL)
				(*itr)->ticker--;
			else {
				idle->threadState = VM_THREAD_STATE_WAITING;
				pushMutex(*itr);
				mutexSleepList.erase(itr);
				break;
			}
		}
	}


	Scheduler();
} //AlarmCallBack()

void FileCallBack(void *param, int result) { 
	//cout << "here" << endl;
	//cout << result << endl;
	//cout << "previous thread " << ((TCB*)param)->threadID << endl;
	((TCB*)param)->fileResult = result;

	currentThread->threadState = VM_THREAD_STATE_WAITING;
	pushThread((TCB*)param);
}

void Skeleton(void* param)
{
	MachineEnableSignals();
	//cout << "Skeleton threadID " << currentThread->threadID << endl;
	currentThread->threadEntry(param); //deal with thread

	//cout << "Skeleton terminating " << currentThread->threadID << endl;
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
} //void pushThread()

TCB *findThread(TVMThreadID thread)
{
	for(vector<TCB*>::iterator itr = threadList.begin(); itr != threadList.end(); ++itr)
	{
		if((*itr)->threadID == thread)
			return (*itr); //thread does exist
	}
	return NULL; //thread does not exist
} //TCB *findThread()

MB *findMutex(TVMMutexID mutex) {
	for(vector<MB*>::iterator itr = mutexList.begin(); itr != mutexList.end(); ++itr) {
		if((*itr)->mutexID == mutex)
			return *itr;		// mutex exists
	}
	return NULL;			// mutex does not exist
}

void removeFromMutex(TCB* myThread){
//check and make sure not in any Mutex queues
	for(vector<MB*>::iterator itr = mutexList.begin(); itr != mutexList.end(); ++itr) {
		for(unsigned int i = 0; i < (*itr)->highQ.size(); i++) {
			if((*itr)->highQ.front() != myThread)
				(*itr)->highQ.push((*itr)->highQ.front());
			(*itr)->highQ.pop();
		}
		for(unsigned int i = 0; i < (*itr)->medQ.size(); i++) {
			if((*itr)->medQ.front() != myThread)
				(*itr)->medQ.push((*itr)->medQ.front());
			(*itr)->medQ.pop();
		}
		for(unsigned int i = 0; i < (*itr)->lowQ.size(); i++) {
			if((*itr)->lowQ.front() != myThread)
				(*itr)->lowQ.push((*itr)->lowQ.front());
			(*itr)->lowQ.pop();
		}
	}
}

void Scheduler()
{
	if(currentThread->threadState == VM_THREAD_STATE_WAITING || currentThread->threadState == VM_THREAD_STATE_DEAD) {
		//cout << "current thread waiting" << endl;
		TCB *newThread = new TCB;
		int flag = 0;

    	if(!highPrio.empty()) {
			newThread = highPrio.front();
			highPrio.pop();
			flag = 1;
			//cout << "high" << endl;
    	} else if(!normPrio.empty()) {
			newThread = normPrio.front();
			normPrio.pop();
			flag = 1;
			//cout << "normal" << endl;
		}
		else if(!lowPrio.empty()) {
			newThread = lowPrio.front();
			lowPrio.pop();
			flag = 1;
			//cout << "low" << endl;
		} else {
			newThread = idle;
			flag = 1;
			//cout << "idle" << endl;
		}

		if(flag) {			// something in the queues
			TCB *oldThread = currentThread; //get cur threads tcb
			currentThread = newThread; //update current thread
			newThread->threadState = VM_THREAD_STATE_RUNNING; //set to running
			//cout << "switching to threadID: " << currentThread->threadID << endl;
			MachineContextSwitch(&(oldThread)->SMC, &(currentThread)->SMC); //switch contexts
		}
	} // if currentthread waiting
} //void Scheduler()

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
		uint8_t *stack = new uint8_t[0x100000]; //array of threads treated as a stack
		idle->threadID = 0; //idle thread first in array of threads
		idle->threadState = VM_THREAD_STATE_DEAD;
		idle->threadPrior = VM_THREAD_PRIORITY_LOW;
		idle->threadEntry = idleFunction;
		idle->base = stack;
		MachineContextCreate(&(idle)->SMC, Skeleton, NULL, stack, 0x100000);

		TCB *VMMainTCB = new TCB; //start main thread
		VMMainTCB->threadID = 1;
		VMMainTCB->threadPrior = VM_THREAD_PRIORITY_NORMAL;
		VMMainTCB->threadState = VM_THREAD_STATE_RUNNING;
		currentThread = VMMainTCB; //current thread is now main

		threadList.push_back(idle);
		threadList.push_back(VMMainTCB);
		VMMain(argc, argv); //function call to start TVMMain
		return VM_STATUS_SUCCESS;
	}
} //TVMStatus VMStart()

TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid)
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
	newThread->threadID =   *tid   = threadList.size();

	threadList.push_back(newThread); //store in vector of ptrs
	
	MachineResumeSignals(&OldState); //resume signals
	return VM_STATUS_SUCCESS;
} //TVMStatus VMThreadCreate()

TVMStatus VMThreadDelete(TVMThreadID thread)
{
	TMachineSignalState OldState; //local variable to suspend signals
	MachineSuspendSignals(&OldState); //suspend signals
	
	TCB *myThread = findThread(thread);
	if(myThread == NULL)
		return VM_STATUS_ERROR_INVALID_ID;
	if(myThread->threadState != VM_THREAD_STATE_DEAD) 
		return VM_STATUS_ERROR_INVALID_STATE;
		

	removeFromMutex(myThread);

	vector<TCB*>::iterator itr;
	for(itr = threadList.begin(); itr != threadList.end(); ++itr) {
		if((*itr) == myThread)
			break;
	}
	threadList.erase(itr);

	MachineResumeSignals(&OldState); //resume signals
	return VM_STATUS_SUCCESS;
} //TVMStatus VMThreadDelete()

TVMStatus VMThreadActivate(TVMThreadID thread)
{
	TMachineSignalState OldState; //local variable to suspend signals
	MachineSuspendSignals(&OldState); //suspend signals

	TCB *myThread = findThread(thread); //call to find the thread ptr
	if(myThread == NULL) //check if thread exists
		return VM_STATUS_ERROR_INVALID_ID;
	if(myThread->threadState != VM_THREAD_STATE_DEAD) // if not dead, error
		return VM_STATUS_ERROR_INVALID_STATE;

	MachineContextCreate(&(myThread)->SMC, Skeleton, (myThread)->vptr, (myThread)->base, (myThread)->threadMemSize); //create context here
	myThread->threadState = VM_THREAD_STATE_READY; //set current thread to running

	pushThread(myThread);
	Scheduler();

	MachineResumeSignals(&OldState); //resume signals
	return VM_STATUS_SUCCESS;
} //TVMStatus VMThreadActivate()

TVMStatus VMThreadTerminate(TVMThreadID thread)
{
	TMachineSignalState OldState; //local variable to suspend signals
	MachineSuspendSignals(&OldState); //suspend signals
	

	TCB *myThread = findThread(thread);
	if(myThread == NULL) //check if thread exists
		return VM_STATUS_ERROR_INVALID_ID;
	if(myThread->threadState == VM_THREAD_STATE_DEAD) //dead state check
		return VM_STATUS_ERROR_INVALID_STATE;

	myThread->threadState = VM_THREAD_STATE_DEAD;


	// check and make sure not in thread queue
	for(unsigned int i = 0; i < highPrio.size(); i++) {
		if(highPrio.front() != myThread)
			highPrio.push(highPrio.front());
		highPrio.pop();
	}
	for(unsigned int i = 0; i < normPrio.size(); i++) {
		if(normPrio.front() != myThread)
			normPrio.push(normPrio.front());
		normPrio.pop();
	}
	for(unsigned int i = 0; i < lowPrio.size(); i++) {
		if(lowPrio.front() != myThread)
			lowPrio.push(lowPrio.front());
		lowPrio.pop();
	}

	removeFromMutex(myThread);

	Scheduler();

	MachineResumeSignals(&OldState); //resume signals
	return VM_STATUS_SUCCESS;
} //TVMStatus VMThreadTerminate()

TVMStatus VMThreadID(TVMThreadIDRef threadref)
{
	if(threadref == NULL) //invalid
		return VM_STATUS_ERROR_INVALID_PARAMETER;

	*threadref = currentThread->threadID;

	return VM_STATUS_SUCCESS; //successful retrieval
} //TVMStatus VMThreadID()

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
} //TVMStatus VMThreadState()

TVMStatus VMThreadSleep(TVMTick tick) {
	TMachineSignalState OldState; //local variable to suspend signals
	MachineSuspendSignals(&OldState); //suspend signals

	if(tick == VM_TIMEOUT_INFINITE) //invalid
		return VM_STATUS_ERROR_INVALID_PARAMETER;

	currentThread->threadState = VM_THREAD_STATE_WAITING;
	currentThread->ticker = tick; //set tick as globaltick

	sleepList.push_back(currentThread);
	//cout << "ThreadID " << currentThread->threadID << " sleeping for " << tick << " ticks" << endl;

	Scheduler();

	MachineResumeSignals(&OldState); //resume signals
	return VM_STATUS_SUCCESS; //success sleep after reaches zero
} //TVMStatus VMThreadSleep()

void scheduleMutex(MB *myMutex) {
	if(myMutex->ownerThread == NULL){
		if(!myMutex->highQ.empty()) {
			myMutex->ownerThread = myMutex->highQ.front();
			myMutex->highQ.pop();
			//cout << "high" << endl;
		} else if(!myMutex->medQ.empty()) {
			myMutex->ownerThread = myMutex->medQ.front();
			myMutex->medQ.pop();
			//cout << "normal" << endl;
		}
		else if(!myMutex->lowQ.empty()) {
			myMutex->ownerThread = myMutex->lowQ.front();
			myMutex->lowQ.pop();
			//cout << "low" << endl;
		}
	}
}

void pushMutex(MB *myMutex) {
	if(currentThread->threadPrior == VM_THREAD_PRIORITY_HIGH)
		myMutex->highQ.push(currentThread);
	else if (currentThread->threadPrior == VM_THREAD_PRIORITY_NORMAL)
		myMutex->medQ.push(currentThread);
	else if (currentThread->threadPrior == VM_THREAD_PRIORITY_LOW)
		myMutex->lowQ.push(currentThread);
} // pushMutex

TVMStatus VMMutexCreate(TVMMutexIDRef mutexref)
{
	TMachineSignalState OldState; //local variable to suspend signals
	MachineSuspendSignals(&OldState); //suspend signals	

	if(mutexref == NULL)
		return VM_STATUS_ERROR_INVALID_PARAMETER;

	MB *newMutex = new MB;
	newMutex->mutexID = mutexList.size();

	mutexList.push_back(newMutex);

	*mutexref = newMutex->mutexID;

	MachineResumeSignals(&OldState);
	return VM_STATUS_SUCCESS;
} //TVMStatus VMMutexCreate()

TVMStatus VMMutexDelete(TVMMutexID mutex)
{
	TMachineSignalState OldState; //local variable to suspend signals
	MachineSuspendSignals(&OldState); //suspend signals

	MB *myMutex = findMutex(mutex);
	if(myMutex == NULL)
  		return VM_STATUS_ERROR_INVALID_ID; //mutex does not exist
  	if(myMutex->ownerThread != NULL)		// if not unlocked
  		return VM_STATUS_ERROR_INVALID_STATE;

  	vector<MB*>::iterator itr;
	for(itr = mutexList.begin(); itr != mutexList.end(); ++itr)
	{
		if((*itr) == myMutex)
			break;
	}

	mutexList.erase(itr); //erase mutex from list

	MachineResumeSignals(&OldState); //resume signals
	return VM_STATUS_SUCCESS;
} //TVMStatus VMMutexDelete()

TVMStatus VMMutexQuery(TVMMutexID mutex, TVMThreadIDRef ownerref)
{
	TMachineSignalState OldState; //local variable to suspend signals
	MachineSuspendSignals(&OldState); //suspend signals

	if(ownerref == NULL)
		return VM_STATUS_ERROR_INVALID_PARAMETER;

	MB *myMutex = findMutex(mutex);
	if(myMutex == NULL)
		return VM_STATUS_ERROR_INVALID_ID;

	if(myMutex->ownerThread == NULL)
		return VM_THREAD_ID_INVALID;

	*ownerref = myMutex->ownerThread->threadID;

	MachineResumeSignals(&OldState);
	return VM_STATUS_SUCCESS;
} //TVMStatus VMMutexQuery()

TVMStatus VMMutexAcquire(TVMMutexID mutex, TVMTick timeout)
{
	TMachineSignalState OldState; //local variable to suspend signals
	MachineSuspendSignals(&OldState); //suspend signals

	MB *myMutex = findMutex(mutex);

	if(myMutex == NULL)
		return VM_STATUS_ERROR_INVALID_ID;

	pushMutex(myMutex);

	// block timeout
	myMutex->ticker = timeout;
	if(myMutex->ticker == VM_TIMEOUT_IMMEDIATE && myMutex->ownerThread != NULL)
		return VM_STATUS_FAILURE;

	if(myMutex->ticker > 0){
		currentThread->threadState = VM_THREAD_STATE_WAITING;
		mutexSleepList.push_back(myMutex);
		Scheduler();
	}

	if(myMutex->ownerThread != NULL)
		return VM_STATUS_FAILURE;

	// mutex scheduler
	scheduleMutex(myMutex);

	MachineResumeSignals(&OldState);
	return VM_STATUS_SUCCESS;
} //TVMStatus VMMutexAcquire()

TVMStatus VMMutexRelease(TVMMutexID mutex)
{
	TMachineSignalState OldState; //local variable to suspend signals
	MachineSuspendSignals(&OldState); //suspend signals

	MB *myMutex = findMutex(mutex);

	if(myMutex == NULL)
		return VM_STATUS_ERROR_INVALID_ID;
	if(myMutex->ownerThread != currentThread)
		return VM_STATUS_ERROR_INVALID_STATE;

	myMutex->ownerThread = NULL;
	scheduleMutex(myMutex);


	MachineResumeSignals(&OldState);
	return VM_STATUS_SUCCESS;
} //TVMStatus VMMutexRelease()

TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor) {
	TMachineSignalState OldState; //local variable to suspend signals
	MachineSuspendSignals(&OldState); //suspend signals

	if(filename == NULL || filedescriptor == NULL)
		return VM_STATUS_ERROR_INVALID_PARAMETER;

	//MachineFileOpen(const char *filename, int flags, int mode, TMachineFileCallback callback, void *calldata);
	//cout << "opening file" << endl;
	MachineFileOpen(filename, flags, mode, FileCallBack, currentThread);

	//cout << "scheduling thread: " << currentThread->threadID << endl;
	currentThread->threadState = VM_THREAD_STATE_WAITING;
	Scheduler();


	*filedescriptor = currentThread->fileResult;

	MachineResumeSignals(&OldState); //resume signals
	if(currentThread->fileResult < 0)
		return VM_STATUS_FAILURE;
	return VM_STATUS_SUCCESS;
} //TVMStatus VMFileOpen()

TVMStatus VMFileClose(int filedescriptor) {
	TMachineSignalState OldState; //local variable to suspend signals
	MachineSuspendSignals(&OldState); //suspend signals

	//MachineFileClose(int fd, TMachineFileCallback callback, void *calldata);
	MachineFileClose(filedescriptor, FileCallBack, currentThread);

	currentThread->threadState = VM_THREAD_STATE_WAITING;
	Scheduler();

	MachineResumeSignals(&OldState); //resume signals
	return VM_STATUS_SUCCESS;
} //TVMStatus VMFileClose()

TVMStatus VMFileRead(int filedescriptor, void *data, int *length) {
	TMachineSignalState OldState; //local variable to suspend signals
	MachineSuspendSignals(&OldState); //suspend signals

	if(data == NULL || length == NULL) //invalid input
		return VM_STATUS_ERROR_INVALID_PARAMETER;

	//MachineFileRead(int fd, void *data, int length, TMachineFileCallback callback, void *calldata);
	MachineFileRead(filedescriptor, data, *length, FileCallBack, currentThread);

	currentThread->threadState = VM_THREAD_STATE_WAITING;
	Scheduler();

	*length = currentThread->fileResult;

	MachineResumeSignals(&OldState); //resume signals
	if(currentThread->fileResult < 0)
		return VM_STATUS_FAILURE;
	return VM_STATUS_SUCCESS;
} //TVMStatus VMFileRead()

TVMStatus VMFileWrite(int filedescriptor, void *data, int *length) {
	TMachineSignalState OldState; //local variable to suspend signals
	MachineSuspendSignals(&OldState); //suspend signals

	if(data == NULL || length == NULL) //invalid input
		return VM_STATUS_ERROR_INVALID_PARAMETER;

	//MachineFileWrite(int fd, void *data, int length, TMachineFileCallback callback, void *calldata);
	MachineFileWrite(filedescriptor, data, *length, FileCallBack, currentThread);

	currentThread->threadState = VM_THREAD_STATE_WAITING;
	Scheduler();

	*length = currentThread->fileResult;

	MachineResumeSignals(&OldState); //resume signals
	if(currentThread->fileResult < 0)
		return VM_STATUS_FAILURE;
	return VM_STATUS_SUCCESS;
} //TVMStatus VMFileWrite() 

TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, int *newoffset){
	TMachineSignalState OldState; //local variable to suspend signals
	MachineSuspendSignals(&OldState); //suspend signals

	//MachineFileSeek(int fd, int offset, int whence, TMachineFileCallback callback, void *calldata);
	MachineFileSeek(filedescriptor, offset, whence, FileCallBack, currentThread);

	currentThread->threadState = VM_THREAD_STATE_WAITING;
	Scheduler();

	*newoffset = currentThread->fileResult;

	MachineResumeSignals(&OldState); //resume signals
	if(currentThread->fileResult < 0)
		return VM_STATUS_FAILURE;
	return VM_STATUS_SUCCESS;
} //TVMStatus VMFileSeek()


} //extern "C"
