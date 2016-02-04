#include "mythread.h"
#include <ucontext.h>
#include <stdlib.h>
#include <stdio.h>
#include <queue>
#include <set>

#define THREAD_STACK 1024*8

typedef struct {
	ucontext_t context;
	void *parent;
	std::set<void*> children;
	std::set<void*> blocked_by;
} _MyThread;

typedef struct {
	int value;
	std::queue<_MyThread*> block_queue;
} _MySem;

ucontext_t main_context; // the context outside mythread library
_MyThread *current_thread = 0;
std::queue<_MyThread*> ready_queue;

// dummy context to handle thread switching
// primary purpose is to cleanup the exited thread's stack
ucontext_t dummy_context;
char dummy_stack[THREAD_STACK];
void dummy_func(void)
{
	while(true)
	{
		// free resources
		free(current_thread->context.uc_stack.ss_sp);
		delete current_thread;

		// simply exit if ready queue is empty
		if(ready_queue.empty())
		{
			current_thread = 0;
			setcontext(&main_context);
		}

		// get next thread
		_MyThread *next = ready_queue.front();
		ready_queue.pop();
		current_thread = next;
		swapcontext(&dummy_context, &next->context);
	}
}

// Call in the front thread of ready queue
// swap = true will save current thread context
void _popNextThread()
{
	// simply exit if ready queue is empty
	if(ready_queue.empty())
	{
		current_thread = 0;
		setcontext(&main_context);
	}

	// get next thread
	_MyThread *next = ready_queue.front();
	ready_queue.pop();

	// now swap to next thread
	_MyThread *tmp = current_thread;
	current_thread = next;
	swapcontext(tmp? &tmp->context : &main_context, &next->context);
}

/*
 * This routine creates a new MyThread.
 * The parameter start_func is the function in which the new thread starts executing.
 * The parameter args is passed to the start function.
 * This routine does not pre-empt the invoking thread.
 * In others words the parent (invoking) thread will continue to run; the child thread will sit in the ready queue.
 */
MyThread MyThreadCreate (void(*start_funct)(void *), void *args)
{
	_MyThread *new_thread = new _MyThread();
	new_thread->parent = current_thread;
	if(current_thread)
	{ // newly created is not the "main" thread, so add its to current_thread's children list
		current_thread->children.insert(new_thread);
	}
	ready_queue.push(new_thread);

	// make context for the new thread
	getcontext(&new_thread->context);
	new_thread->context.uc_link = &dummy_context;
	new_thread->context.uc_stack.ss_sp = malloc(THREAD_STACK);
	new_thread->context.uc_stack.ss_size = THREAD_STACK;
	new_thread->context.uc_stack.ss_flags = 0;
	makecontext(&new_thread->context, (void(*)())start_funct, 1, args);

	return (MyThread) new_thread;
}

/*
 * Terminates the invoking thread.
 * Note: all MyThreads are required to invoke this function.
 * Do not allow functions to “fall out” of the start function.
 */
void MyThreadExit(void)
{
	_MyThread *parent = (_MyThread*)current_thread->parent;
	if(parent)
	{ // parent thread is active
		parent->children.erase(current_thread); // delete thread from parent's children list
		if(!parent->blocked_by.empty())
		{ // unblock parent if needed
			parent->blocked_by.erase(current_thread);
			if(parent->blocked_by.empty())
			{ // unblock parent thread, i.e. push it to the end of ready queue
				ready_queue.push(parent);
			}
		}
	}

	// all children no longer have parent
	if(!current_thread->children.empty())
	{
		std::set<void*>::iterator it;
		_MyThread *child;
		for(it = current_thread->children.begin(); it != current_thread->children.end(); ++it)
		{
			child = (_MyThread*)*it;
			child->parent = 0;
		}
	}
}

/*
 * Suspends execution of invoking thread and yield to another thread.
 * The invoking thread remains ready to execute—it is not blocked.
 * Thus, if there is no other ready thread, the invoking thread will continue to execute.
 */
void MyThreadYield(void)
{
	if(!ready_queue.empty())
	{ // there are other threads in the ready queue, may swap
		ready_queue.push(current_thread); // back to the ready queue and live another day
		_popNextThread();
	}
}

/*
 * Joins the invoking function with the specified child thread.
 * If the child has already terminated, do not block.
 * Note: A child may have terminated without the parent having joined with it.
 * Returns 0 on success (after any necessary blocking).
 * It returns -1 on failure. Failure occurs if specified thread is not an immediate child of invoking thread.
 */
int MyThreadJoin(MyThread thread)
{
	if(current_thread->children.count(thread) != 0)
	{ // thread is a child of current thread
		// we should flag current thread as blocked by input thread, and then call next thread in the ready queue
		current_thread->blocked_by.insert(thread);
		_popNextThread();
		return 0;
	}
	else
	{ // not a child, return error
		return -1;
	}
}

/*
 * Waits until all children have terminated. Returns immediately if there are no active children.
 */
void MyThreadJoinAll(void)
{
	if(!current_thread->children.empty())
	{
		std::set<void*>::iterator it;
		for(it = current_thread->children.begin(); it != current_thread->children.end(); ++it)
		{ // loop through all children
			current_thread->blocked_by.insert(*it);
		}
		_popNextThread();
	}
}

bool init = false;
void MyThreadInit(void(*start_funct)(void *), void *args)
{
	if(!init)
	{
		init = true;

		// create a context served as uc_link for all threads
		getcontext(&dummy_context);
		dummy_context.uc_link = 0;
		dummy_context.uc_stack.ss_sp = dummy_stack;
		dummy_context.uc_stack.ss_size = THREAD_STACK;
		dummy_context.uc_stack.ss_flags = 0;
		makecontext(&dummy_context, dummy_func, 0);

		MyThreadCreate(start_funct, args);
		_popNextThread();
	}
}

/*
 * Create a semaphore. Set the initial value to initialValue, which must be non-negative.
 * A positive initial value has the same effect as invoking MySemaphoreSignal the same number of times.
 */
MySemaphore MySemaphoreInit(int initialValue)
{
	if(initialValue < 0) return 0;
	_MySem *sem = new _MySem();
	sem->value = initialValue;
	return (MySemaphore)sem;
}

// Signal semaphore sem. The invoking thread is not pre-empted.
void MySemaphoreSignal(MySemaphore sem)
{
	_MySem *ms = (_MySem*)sem;
	if(ms)
	{
		if(ms->block_queue.empty()) ++ms->value;
		else
		{ // put the front thread back to ready queue, semaphore value stays the same
			ready_queue.push(ms->block_queue.front());
			ms->block_queue.pop();
		}
	}
}

// Wait on semaphore sem.
void MySemaphoreWait(MySemaphore sem)
{
	_MySem *ms = (_MySem*)sem;
	if(ms)
	{
		if(ms->value > 0) --ms->value; // no need to block
		else
		{
			ms->block_queue.push(current_thread);
			_popNextThread();
		}
	}
}

// Destroy semaphore sem. Do not destroy semaphore if any threads are blocked on the queue. Return 0 on success, -1 on failure.
int MySemaphoreDestroy(MySemaphore sem)
{
	_MySem *ms = (_MySem*)sem;
	if(ms && ms->block_queue.empty())
	{
		delete ms;
		return 0;
	}
	return -1;
}
