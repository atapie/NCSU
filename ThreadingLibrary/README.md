### Description:

Implement a non pre-emptive user-level threads library _mythread.a_ with the following routines.

#### Thread routines.

> MyThread **MyThreadCreate** (void(*start_funct)(void *), void *args)
> 
> This routine creates a new _MyThread_. The parameter _start\_func_ is the function in which the new thread starts executing. The parameter _args_ is passed to the start function. This routine does _not_ pre-empt the invoking thread. In others words the parent (invoking) thread will continue to run; the child thread will sit in the ready queue.
> 
> void **MyThreadYield**(void)
> 
> Suspends execution of invoking thread and yield to another thread. The invoking thread remains ready to execute—it is not blocked. Thus, if there is no other ready thread, the invoking thread will continue to execute.
> 
> int **MyThreadJoin**(MyThread thread)
> 
> Joins the invoking function with the specified child thread. If the child has already terminated, do not block. Note: A child may have terminated without the parent having joined with it. Returns 0 on success (after any necessary blocking). It returns -1 on failure. Failure occurs if specified thread is not an immediate child of invoking thread.
> 
> void **MyThreadJoinAll**(void)
> 
> Waits until all children have terminated. Returns immediately if there are no _active_ children.
> 
> void **MyThreadExit**(void)
> 
> Terminates the invoking thread. _**Note:**_ all _MyThread_s are required to invoke this function. Do not allow functions to “fall out” of the start function.

#### Semaphore routines.

> MySemaphore **MySemaphoreInit**(int initialValue)
> 
> Create a semaphore. Set the initial value to _initialValue_, which must be non-negative. A positive initial value has the same effect as invoking _MySemaphoreSignal_ the same number of times.
> 
> void **MySemaphoreSignal**(MySemaphore sem)
> 
> Signal semaphore _sem_. The invoking thread is not pre-empted.
> 
> void **MySemaphoreWait**(MySemaphore sem)
> 
> Wait on semaphore _sem_.
> 
> int **MySemaphoreDestroy**(MySemaphore sem)
> 
> Destroy semaphore _sem_. Do not destroy semaphore if any threads are blocked on the queue. Return 0 on success, -1 on failure.

#### Unix process routines.

The Unix process in which the user-level threads run is not a _MyThread_. Therefore, it will not be placed on the queue of _MyThreads._ Instead it will create the first _MyThread_ and relinquish the processor to the _MyThread_ engine.

The following routine may be executed only by the Unix process.

> void **MyThreadInit** (void(*start_funct)(void *), void *args)
> 
> This routine is called before any other _MyThread_ call. It is invoked only by the Unix process. It is similar to invoking _MyThreadCreate_ immediately followed by _MyThreadJoinAll_. The _MyThread_ created is the oldest ancestor of all _MyThread_s—it is the “main” _MyThread_. This routine can only be invoked once. It returns when there are no threads available to run (i.e., the thread ready queue is empty.
