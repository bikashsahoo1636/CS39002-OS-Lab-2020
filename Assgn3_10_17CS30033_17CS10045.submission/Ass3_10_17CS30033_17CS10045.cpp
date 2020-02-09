#include <iostream>
#include <stdio.h>
#include <unistd.h> 
#include <stdlib.h> 
#include <time.h>
#include <pthread.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <sys/time.h>
#include <chrono>

using namespace std;

// can handle a maximum priority queue size of 1000
#define MAX_PRIORITY_QUEUE 1000

// defining structures and functions structures

typedef struct
{
	int proc_id, prod_num, priority, time, job_id;
} job;

void delay(float number_of_seconds);

void printjob(job j);
job createjob(int proc_id, int prod_num);

typedef struct
{
	job jobpq[MAX_PRIORITY_QUEUE];
	int max_size, size, job_created, job_completed;
	pthread_mutex_t lock;
} priorityqueue;

void printqueue(priorityqueue* pq);
void insert(priorityqueue* pq, job j);
job remove(priorityqueue* pq);

priorityqueue* createpq(int max_size, int shmid1);
void producer(priorityqueue *pq, int prod_num,int proc_id, int max_jobs);
void consumer(priorityqueue *pq,int cons_num,int proc_id, int max_jobs);

int main()
{
	// random seed for parent
	srand(time(0));

	// user input
	int NP,NC,max_size,max_jobs;
	cout<<"Producers:";
	cin>>NP;
	cout<<"Consumers:";
	cin>>NC;
	cout<<"Maxsize of priority queue:";
	cin>>max_size;
	cout<<"Max jobs:";
	cin>>max_jobs;

	// random key for shared memory allocation using /dev/random file
	key_t key1 = ftok("/dev/random", 'b');
	// create shared memory
	int shmid1 = shmget(key1,sizeof(priorityqueue),0660|IPC_CREAT);
	if (shmid1<0) {
		cout<<"Failed to allocate shared memory!\n";
		exit(1);
	}
	// form priority queue from the shared memory
	priorityqueue *pq = createpq(max_size, shmid1);


	// note begin time
	auto begin = std::chrono::high_resolution_clock::now();
	// double begin =  (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;

	pid_t pid;
	cout<<endl;
	cout<<"Action\tProdProcID\tProdNum\tConsProcID\tConsNum\tJobID\tJobPrio\tCompTime"<<endl;
	// creating all producers
	for(int i=1;i<=NP;++i)
	{
		pid = fork();
		if(pid<0) cout<<"ERROR PRODUCER CREATION FAILED.";
		else if(pid==0)
		{
			//differed seed for each child process
			srand(time(0) ^ i*7);
			// process id of the child
			int proc_id=getpid();
			producer(pq, i,proc_id, max_jobs);
			return 0;
		}
	}

	// creating all consumers
	for(int i=1;i<=NC;++i)
	{
		pid = fork();
		if(pid<0) cout<<"ERROR CONSUMER CREATION FAILED.";
		else if(pid==0)
		{
			//different seed for each child process
			srand(time(0) ^ i);
			// get pid of the child in itself
			int proc_id=getpid();
			consumer(pq, i, proc_id, max_jobs);
			return 0;
		}		
	}

	// while loop that waits till all the jobs are created and consumed
	while(1)
	{
		// put lock before querying so state change is not possible
		pthread_mutex_lock(&pq->lock);
		if((pq->job_completed)==max_jobs and (pq->job_created)==max_jobs)
		{
			// calculate end time and time consumed thereafter
			auto end = std::chrono::high_resolution_clock::now();
			auto consumed = std::chrono::duration_cast<std::chrono::microseconds>(end-begin);
			cout<<"We have created and consumed "<<max_jobs<<" jobs, within "<< (float)consumed.count()/1000000 <<" seconds."<<endl;
			pthread_mutex_unlock(&pq->lock);
			break;
		}
		// unlock memory
		pthread_mutex_unlock(&pq->lock);
	}
	return 0;
}

// producer function
void producer(priorityqueue* pq, int prod_num, int proc_id, int max_jobs)
{
	while(1)
	{
		// if all jobs created, exit
		if(pq->job_created>=max_jobs) break;
		// random delay
		delay((float) (rand()%4));
		while(1)
		{
			// locking shared memory for job insertion
			pthread_mutex_lock(&pq->lock);
			if(pq->job_created>=max_jobs)
			{
				pthread_mutex_unlock(&pq->lock);
				break;
			}
			// create job
			job j = createjob(proc_id,prod_num);
			if((pq->size)<(pq->max_size))
			{
				// increment job counter
				pq->job_created++;
				// insert job
				insert(pq,j);
				// printqueue(pq);
				cout<<"Create\t"<<j.proc_id<<"\t\t"<<j.prod_num<<"\t"<<"-\t\t-\t"<<j.job_id<<"\t"<<j.priority<<"\t"<<j.time<<endl;
				// unlocking shared memory
				pthread_mutex_unlock(&pq->lock);
				break;
			}
			// unlocking shared memory
			pthread_mutex_unlock(&pq->lock);
		}
	}
	return;
}

// consumer function
void consumer(priorityqueue *pq, int cons_num, int proc_id, int max_jobs)
{
	while(1)
	{
		// exit if all jobs consumed
		if(pq->job_completed==max_jobs) break;
		// random delay
		delay((float) (rand()%4));
		while(1)
		{
			// locking shared memory for job removal
			pthread_mutex_lock(&pq->lock);
			if((pq->job_completed)==max_jobs)
			{
				pthread_mutex_unlock(&pq->lock);
				break;
			}
			if((pq->size)>0)
			{
				// printqueue(pq);
				// remove job
				job j = remove(pq);
				// delay caused by the job compute time
				delay((float)j.time);
				cout<<"Consume\t"<<j.proc_id<<"\t\t"<<j.prod_num<<"\t"<<proc_id<<"\t\t"<<cons_num<<"\t"<<j.job_id<<"\t"<<j.priority<<"\t"<<j.time<<endl;
				// increment counter
				(pq->job_completed) = (pq->job_completed) + 1;
				// releasing the lock on shared memory
				pthread_mutex_unlock(&pq->lock);
				break;
			}
			// releasing the lock on shared memory
			pthread_mutex_unlock(&pq->lock);
		}	
	}
	return;
}

// print job
void printjob(job j)
{
	cout<<"\nProcess ID:"<<j.proc_id;
	cout<<"\nProducer Number:"<<j.prod_num;
	cout<<"\nPriority:"<<j.priority;
	cout<<"\nTime:"<<j.time;
	cout<<"\nJob ID:"<<j.job_id<<"\n\n";
}

// create job with random integers for id, priority and computer time
job createjob(int proc_id, int prod_num)
{
	job j;
	j.proc_id = proc_id;
	j.prod_num = prod_num;
	j.priority = rand()%10 + 1;
	j.time = rand()%4 + 1;
	j.job_id = rand()%100000+1;
	return j;
}

// debugging function, prints the priority queue
void printqueue(priorityqueue* pq)
{
	for(int i=1;i<=pq->size;++i) 
	{
		cout<<"Job "<<i<<endl;
		printjob((pq->jobpq)[i]);
	}
}

// insert a new item into the priority queue
void insert(priorityqueue* pq, job j)
{

	pq->size++;
	(pq->jobpq)[pq->size] = j;
	int i=pq->size;
	while(i>1)
	{
		//cout<<"INLOOP"<<endl;
		if((pq->jobpq)[i].priority>(pq->jobpq)[i/2].priority)
		{
			job temp = (pq->jobpq)[i];
			(pq->jobpq)[i] = (pq->jobpq)[i/2];
			(pq->jobpq)[i/2] = temp;
			i/=2;
		}
		else break;
	}
	//cout<<"OUTLOOP"<<endl;
	return;
}

// creates priority queue and initialises it
priorityqueue* createpq(int max_size, int shmid1)
{
	priorityqueue* pq = (priorityqueue*)shmat(shmid1,(void*)0,0);
	pq->size=0;
	pq->job_created=0;
	pq->job_completed=0;
	pq->max_size=max_size;

	// initialising mutex locking
	pthread_mutexattr_t lock_attr;
	pthread_mutexattr_init(&lock_attr);
	pthread_mutexattr_setpshared(&lock_attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&pq->lock, &lock_attr);

	return pq;
}

// removes an item and arranges the priority queue to put item with highest priority at top
job remove(priorityqueue* pq)
{
	if(pq->size==0) 
	{
		job j;
		cout<<"ERROR\n";
		return j;
	}
	else if(pq->size==1)
	{
		pq->size--;
		return (pq->jobpq)[1];
	}
	else
	{
		// cout << pq->size << endl;
		job temp=(pq->jobpq)[1];
		(pq->jobpq)[1] = (pq->jobpq)[(pq->size)];
		pq->size--;
		int i=1;
		while(i<pq->size)
		{
			if(2*i>pq->size) break;
			if((pq->jobpq)[i].priority >(pq->jobpq)[2*i].priority)
			{
				if(2*i+1>pq->size) break;
				if((pq->jobpq)[i].priority > (pq->jobpq)[2*i+1].priority) break;
				else
				{
					job t = (pq->jobpq)[2*i+1];
					(pq->jobpq)[2*i+1] = (pq->jobpq)[i];
					(pq->jobpq)[i] = t;
					i=2*i+1;
				}
			}
			else
			{
				if((pq->jobpq)[i].priority > (pq->jobpq)[2*i+1].priority)
				{
					job t = (pq->jobpq)[2*i];
					(pq->jobpq)[2*i] = (pq->jobpq)[i];
					(pq->jobpq)[i] = t;
					i=2*i;
				}
				else
				{
					if((pq->jobpq)[2*i].priority > (pq->jobpq)[2*i+1].priority)
					{
						job t = (pq->jobpq)[2*i];
						(pq->jobpq)[2*i] = (pq->jobpq)[i];
						(pq->jobpq)[i] = t;
						i=2*i;
					}
					else
					{
						job t = (pq->jobpq)[2*i+1];
						(pq->jobpq)[2*i+1] = (pq->jobpq)[i];
						(pq->jobpq)[i] = t;
						i=2*i+1;
					}
				}
			}
			
		}

		return temp;
	}
}

// custom implementation of delay in order to avoid pausing of clock
void delay(float number_of_seconds) 
{
    clock_t start_time = clock(); 
    while (clock() < start_time + number_of_seconds*CLOCKS_PER_SEC); 
}
