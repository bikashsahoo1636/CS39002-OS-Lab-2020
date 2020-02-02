#include <iostream>
#include <stdio.h>
#include <unistd.h> 
#include <stdlib.h> 
#include <time.h>

#include <sys/ipc.h> 
#include <sys/shm.h> 

using namespace std;

// can handle a maximum priority queue size of 1000
#define MAX_PRIORITY_QUEUE 1000

typedef struct
{
	int proc_id, prod_num, priority, time, job_id;
} job;

void delay(int number_of_seconds);

void printjob(job j);
job createjob(int proc_id, int prod_num);

typedef struct
{
	job jobpq[MAX_PRIORITY_QUEUE];
	int max_size, size, job_created, job_completed;
} priorityqueue;

void printqueue(priorityqueue* pq);
void insert(priorityqueue* pq, job j);
job remove(priorityqueue* pq);

priorityqueue* createpq(int max_size, int shmid1);
void producer(priorityqueue *pq, int prod_num,int proc_id, int max_jobs);
void consumer(priorityqueue *pq,int cons_num,int proc_id, int max_jobs);

int main()
{
	srand(time(0));
	int NP,NC,max_size,max_jobs;
	cout<<"Producers:";
	cin>>NP;
	cout<<"Consumers:";
	cin>>NC;
	cout<<"Maxsize of priority queue:";
	cin>>max_size;
	cout<<"Max jobs:";
	cin>>max_jobs;

	key_t key1 = ftok("/dev/urandom", 500);
	int shmid1 = shmget(key1,sizeof(priorityqueue),0660|IPC_CREAT);
	if (shmid1<0) {
		cout<<"Failed to allocate shared memory!\n";
		exit(1);
	}
	priorityqueue *pq = createpq(max_size, shmid1);


	clock_t start,end;

	pid_t pid;
	/*
	cout<<"\t\t\Job Hierarchy"<<endl;
	cout<<"Insert/Cons\tConsID\tConsNum\tProdID\tProdNum"<<endl;
	*/
	start = clock();
	for(int i=1;i<=NP;++i)
	{
		pid = fork();
		if(pid<0) cout<<"ERROR PRODUCER CREATION FAILED.";
		else if(pid==0)
		{
			//child process
			int proc_id=rand()%100000+1;
			pq = (priorityqueue*)shmat(shmid1,(void*)0,0);
			producer(pq, i,proc_id, max_jobs);
			return 0;
		}
	}

	for(int i=1;i<=NC;++i)
	{
		pid = fork();
		if(pid<0) cout<<"ERROR CONSUMER CREATION FAILED.";
		else if(pid==0)
		{
			//child process
			int proc_id=rand()%100000+1;
			pq = (priorityqueue*)shmat(shmid1,(void*)0,0);
			consumer(pq, i, proc_id, max_jobs);
			return 0;
		}		
	}

	while(1)
	{
		if(pq->job_completed==max_jobs)
		{
			end = clock();
			cout<<"We have created and consumed "<<max_jobs<<" jobs, within "<<((double)(end-start))/CLOCKS_PER_SEC<<" seconds."<<endl;
			break;
		}
	}

	return 0;
}


void producer(priorityqueue* pq, int prod_num, int proc_id, int max_jobs)
{
	while(1)
	{
		if(pq->job_created>=max_jobs) break;
		pq->job_created++;
		job j = createjob(proc_id,prod_num);
		delay(rand()%4);
		while(1)
		{
			if((pq->size)<(pq->max_size))
			{
				cout<<"END\n";
				insert(pq,j);
				cout<<"END2\n";
				cout<<"Job Inserted-->Proc_id:"<<j.proc_id<<", ProdNum:"<<j.prod_num<<", JobID"<<j.job_id<<", Priority"<<j.priority<<", Time:"<<j.time<<endl;
				break;
			}
		}
	}
	cout<<"ProducerDone"<<endl;
	return;
}

void consumer(priorityqueue *pq, int cons_num, int proc_id, int max_jobs)
{
	while(1)
	{
		delay(rand()%4);
		if(pq->job_completed==max_jobs) break;
		while(1)
		{
			if(pq->job_completed==max_jobs) break;
			if(pq->size>0)
			{
				if(pq->job_completed==max_jobs) break;
				job j = remove(pq);
				cout<<"Job Consumed-->Con_id:"<<proc_id<<", ConsNum"<<cons_num<<", Proc_id:"<<j.proc_id<<", ProdNum:"<<j.prod_num<<", JobID"<<j.job_id<<", Priority"<<j.priority<<", Time:"<<j.time<<endl;
				delay(j.time);
				pq->job_completed++;
				break;
			}
		}
		if(pq->job_completed==max_jobs) break;		
	}

	return;
}

void printjob(job j)
{
	cout<<"\nProcess ID:"<<j.proc_id;
	cout<<"\nProducer Number:"<<j.prod_num;
	cout<<"\nPriority:"<<j.priority;
	cout<<"\nTime:"<<j.time;
	cout<<"\nJob ID:"<<j.job_id<<"\n\n";
}

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

void printqueue(priorityqueue* pq)
{
	for(int i=1;i<=pq->size;++i) 
	{
		cout<<"Job "<<i<<endl;
		printjob((pq->jobpq)[i]);
	}
}

void insert(priorityqueue* pq, job j)
{

	(pq->size)++;
	// (pq->jobpq)[pq->size] = j;
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

priorityqueue* createpq(int max_size, int shmid1)
{
	priorityqueue* pq = (priorityqueue*)shmat(shmid1,(void*)0,0);
	pq->size=0;
	pq->job_created=0;
	pq->job_completed=0;
	pq->max_size=max_size;
	return pq;
}

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
		job temp=(pq->jobpq)[1];
		(pq->jobpq)[1] = (pq->jobpq)[pq->size];
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

void delay(int number_of_seconds) 
{
    clock_t start_time = clock(); 
    while (clock() < start_time + number_of_seconds*CLOCKS_PER_SEC); 
}
