#include <iostream>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

using namespace std;

void * producer(void * x);
void * consumer(void * x);
void * scheduler(void *x);
void * reporter(void *x);
void signal_handler(int sig);
int access_buff(int opr, int val);

#define QUANTUM 1

// structure to store all information regarding each threat
struct worker_info
{
	pthread_t id;
	bool dead=false;
	string type;
};

// defining common variables globally for ease of access
struct worker_info * tinfo;
int * shared_buff;
int shared_buff_size = 0, max_buff_size, producers_remain=0, total_threads;
// reporter specific global variables to pass information
bool rep_last_terminated=false, thread_woken=false;
int cur_thread=-1;
pthread_mutex_t buffer_lock;

int main()
{
	// user input variables
	int N,M;

	// initialize random number generator and mutex lock
	srand(time(0));
	pthread_mutex_init(&buffer_lock, NULL);

	// signal handler assingment
	signal(SIGUSR1, signal_handler);
	signal(SIGUSR2, signal_handler);

	// get user input
	cout<<"Enter the number of threads: ";
	cin>>N;
	total_threads = N;
	cout<<"Enter the maximum buffer capacity: ";
	cin>>M;
	// create array for storing N threads and related information
	tinfo = (struct worker_info *)malloc(sizeof(struct worker_info) * N);
	// create buffer space for M integers
	shared_buff = (int *)malloc(sizeof(int) * M);
	max_buff_size = M;
	// create N worked with equal probability
	int X[N];
	int i;
	for(i=0;i<N;i++)
	{
		X[i] = i;
		if(rand()%2)
		{
			//create producer
			cout<<"Creating Producer, thread no: "<<i<<endl;
			// create a thread to run producer function with thread id as argument
			pthread_create(&tinfo[i].id, NULL, &producer, &X[i]);
			tinfo[i].type = "Producer";
			// put thread to sleep initially
			pthread_kill(tinfo[i].id, SIGUSR1);
			// increase count of producers
			producers_remain++;
		}
		else
		{
			//create consumer
			cout<<"Creating Consumer, thread no: "<<i<<endl;
			// create a thread to run consumer function with thread id as argument
			pthread_create(&tinfo[i].id, NULL, &consumer, &X[i]);
			tinfo[i].type = "Consumer";
			// put thread to sleep initially
			pthread_kill(tinfo[i].id, SIGUSR1);
		}
	}

	if(producers_remain==0)
	{
		cout<<"No Producers created! Exiting..."<<endl;
		exit(EXIT_FAILURE);
	}
	if(producers_remain==N)
	{
		cout<<"No Consumers created! Exiting..."<<endl;;
		exit(EXIT_FAILURE);
	}

	// creating scheduler and reporter
	pthread_t scheduler_tid, reported_tid;
	pthread_create(&scheduler_tid, NULL, &scheduler, &i);
	int j = i+1;
	pthread_create(&reported_tid, NULL, &reporter, &j);

	// wait till the scheduler and reporter are alive
	pthread_join(scheduler_tid, NULL);
	pthread_join(reported_tid, NULL);

	cout<<"All operations done, scheduler and reporter closed!"<<endl;
	return 0;
}


void * producer(void * x)
{
	// initialisation time required in order to avoid seg fault
	usleep(1000);

	int id =*(int*)x;
	// cout<<"This is Producer Thread Number "<<id<<endl;

	// masking for signal SIGUSR1 at times when important process is in progress
	sigset_t ign; sigemptyset(&ign); sigaddset(&ign, SIGUSR1);

	// generating 1000 pseduo-random numbers and inserting them
	for (int count=0; count<1000; count++)
	{
		// allow no SIGUSR1 when insertion in progress
		pthread_sigmask(SIG_BLOCK, &ign, NULL);
		// call access_buff to add a random integer 
		if (!access_buff(1, rand()))
		{
			// if insertion was unsuccessful
			count--;
		}
		// allow SIGUSR1 when insertion complete
		pthread_sigmask(SIG_UNBLOCK, &ign, NULL);
	}
	pthread_mutex_lock(&buffer_lock);
	// decrease producer count and mark it as dead
	producers_remain--;
	tinfo[id].dead=true;
	pthread_mutex_unlock(&buffer_lock);
	return NULL;
}

void * consumer(void * x)
{
	// initialisation time required in order to avoid seg fault
	usleep(1000);
	int id =*(int*)x;
	// cout<<"This is Consumer Thread Number "<<id<<endl;

	// masking for signal SIGUSR1 at times when important process is in progress
	sigset_t ign; sigemptyset(&ign); sigaddset(&ign, SIGUSR1);

	// trying to remove an integer
	while(true)
	{
		// allow no SIGUSR1 when removal in progress
		pthread_sigmask(SIG_BLOCK, &ign, NULL);
		// try to remove integer
		access_buff(0, 0);
		// allow SIGUSR1 when removal complete
		pthread_sigmask(SIG_UNBLOCK, &ign, NULL);
	}
	return NULL;
}

void * scheduler(void * x)
{
	int id =*(int*)x;
	int cur_thrd_id=0;
	cout<<"This is Scheduler Thread Number "<<id<<endl;
	// initialisation time required in order to avoid seg fault
	usleep(1000);

	while(true){
		pthread_mutex_lock(&buffer_lock);
		// terminate if no producers remain and buffer size if zero
		if((!producers_remain) && shared_buff_size==0) 
		{
			pthread_mutex_unlock(&buffer_lock);
			break;
		}
		pthread_mutex_unlock(&buffer_lock);
		// find a non dead thread
		while(tinfo[cur_thrd_id].dead)
		{
			cur_thrd_id = (cur_thrd_id+1) % total_threads;
		}

		// wake up that thread
		pthread_kill(tinfo[cur_thrd_id].id, SIGUSR2);
		// tell reporter
		pthread_mutex_lock(&buffer_lock);
		cur_thread = cur_thrd_id;
		thread_woken = true;
		pthread_mutex_unlock(&buffer_lock);	
		// initialize clock
		clock_t cur_t = clock();
		// let the thread run till it runs out of time or dies
		while(!(((double)(clock()-cur_t)/CLOCKS_PER_SEC) >= QUANTUM) && !(tinfo[cur_thrd_id].dead));

		// if thread not dead, send signal to sleep
		if (!tinfo[cur_thrd_id].dead) pthread_kill(tinfo[cur_thrd_id].id, SIGUSR1);
		else 
		{
			pthread_mutex_lock(&buffer_lock);
			rep_last_terminated = true;
			pthread_mutex_unlock(&buffer_lock);
		}
		// move to next thread
		cur_thrd_id = (cur_thrd_id+1) % total_threads;
	}
	cout<<"Scheduler Done! Closing..."<<endl;
	return NULL;
}

void * reporter(void * x) 
{
	int id =*(int*)x;
	cout<<"This is Reported Thread Number "<<id<<endl;
	int last_thread = -1;
	while(true)
	{
		// wait till a thread is started
		while(!thread_woken){
			if ((!producers_remain) && shared_buff_size==0){
				break;
			}
		}
		pthread_mutex_lock(&buffer_lock);
		thread_woken = false;

		if(rep_last_terminated){
			cout<<"REPORT: Last thread "<< last_thread<< " terminated! Thread "<< cur_thread <<", a "<<tinfo[cur_thread].type<<" running!"<<endl;
		} 
		else if (last_thread == -1)
		{
			cout<<"REPORT: First thread "<<cur_thread<<", a "<<tinfo[cur_thread].type<<" running!"<<endl;
		}
		else 
		{
			cout<<"REPORT: Last thread "<< last_thread << ", a "<<tinfo[last_thread].type<<" switched to Thread "<< cur_thread << ", a "<<tinfo[cur_thread].type<<endl;
		}
		last_thread = cur_thread;
		cout<<"REPORT: Size of buffer is "<<shared_buff_size<<endl;
		// terminate if no producers remain and buffer size if zero
		if((!producers_remain) && shared_buff_size==0) 
		{
			pthread_mutex_unlock(&buffer_lock);
			break;
		}
		rep_last_terminated = false;
		pthread_mutex_unlock(&buffer_lock);
	}
	cout<<"Reported done, closing..."<<endl;
	return NULL;
}

// common method to access buffer in order to avoid code duplication
int access_buff(int opr, int val)
{
	// cout<<"Here1"<<endl;
	// put lock
	pthread_mutex_lock(&buffer_lock);
	// 0 for removing integer
	if (opr==0)
	{
		// return 0 if buffer is empty
		if (shared_buff_size<=0) {
			pthread_mutex_unlock(&buffer_lock);
			return 0;
		}
		// return 1 if integer removed
		else {
			shared_buff_size--;
			pthread_mutex_unlock(&buffer_lock);
			return 1;
		}
	}
	// 1 for adding integer
	else if (opr==1)
	{
		// return 0 if buffer is full
		if (shared_buff_size>=max_buff_size) {
			pthread_mutex_unlock(&buffer_lock);
			return 0;
		}
		// return 1 if integer added
		else {
			shared_buff[shared_buff_size] = val;
			shared_buff_size++;
			// cout<<"Here1"<<endl;
			pthread_mutex_unlock(&buffer_lock);
			return 1;
		}
	}
	pthread_mutex_unlock(&buffer_lock);
}

// signal handlers for all threads
void signal_handler(int sig)
{
	// creating set for signal SIGUSR2 at times when the current thread is suspended
	sigset_t ign; sigemptyset(&ign); sigaddset(&ign, SIGUSR2);
	if (sig==SIGUSR1)
	{
		// wait till SIGUSR2 is provided
		int sign;
		sigwait(&ign, &sign);
	}
}