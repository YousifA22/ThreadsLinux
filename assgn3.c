
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>

#include "assgn3.h"


int num_processes;												 
cpu_run_queue cpu_run_queues[NUM_CPU] = {0};
pthread_mutex_t queue_mutex[NUM_CPU];  		 
static int all_processes_finished = 0;					

int main(int argc, char *argv[])
{
	int res;
	pthread_t producer_thread, consumer_threads[NUM_CPU];
	int cpu_num;
	void *thread_result;

	
	if (argc != 2) {
		fprintf(stderr, "parameters are required\n");
		exit(EXIT_FAILURE);
	}
	num_processes = atoi(argv[1]);

	
	for(cpu_num = 0; cpu_num < NUM_CPU; cpu_num++) {
		res = pthread_mutex_init(&queue_mutex[cpu_num], NULL);
		if (res != 0) {
			perror("Mutex initialization failed\n");
			exit(EXIT_FAILURE);
		}
	}


	res = pthread_create(&producer_thread, NULL, producer_thread_function, NULL);
	if (res != 0) {
		perror("Producer thread creation failed\n");
		exit(EXIT_FAILURE);
	}
	usleep(10000);				

	printf("Starting:\n\n");

	

	for(cpu_num = 0; cpu_num < NUM_CPU; cpu_num++) {
		res = pthread_create(&(consumer_threads[cpu_num]), NULL, consumer_thread_function, (void *)&cpu_num);
		if (res != 0) {
			perror("Consumer thread creation failed\n");
			exit(EXIT_FAILURE);
		}
		usleep(70000);			
	}
	
	

	

	res = pthread_join(producer_thread, &thread_result);
	if( res != 0)
		perror("pthread_join failed\n");
		
	



	for(cpu_num = 0; cpu_num < NUM_CPU; cpu_num++) {
		res = pthread_join(consumer_threads[cpu_num], &thread_result);
		if(res != 0)
			perror("pthread_join failed\n");
	}

	

	printf("All done, cleaning up\n");
	for(cpu_num = 0; cpu_num < NUM_CPU; cpu_num++) {
		pthread_mutex_destroy(&queue_mutex[cpu_num]);
	}
	
 	exit(EXIT_SUCCESS);
}

void *producer_thread_function(void *arg) 
{
	int i;
	int res;
	int curr_cpu;							
	int buf_tail;						
	int rq_num, sched_type, priority;
	process_struct *curr_process;


	srand(time(NULL));	


	for(i = 0; i < NUM_CPU; i++) {
		res = pthread_mutex_lock(&queue_mutex[i]);
		if (res != 0) {
			perror("Mutex lock failed\n");
			exit(EXIT_FAILURE);
		}
	}

	for(i = 0; i < num_processes; i++) {
		
		curr_cpu = i % 4;

	
		
		if( (i % 5) == 0){
			rq_num = 0;											
			sched_type = MY_SCHED_FIFO;
			priority = rand()%100;				
			
		}
		if( (i % 5) == 1){
			rq_num = 0;											
			sched_type = MY_SCHED_RR;			
			priority = rand()%100;	
		}
		if( (i % 5) == 3){
			priority = 100 + rand()%40;
		
			if(priority<130){
				rq_num = 1;
			}if(priority>130){
			rq_num=2;
			}												
			sched_type = MY_SCHED_NORMAL;			
		
		}
		
	
		buf_tail = cpu_run_queues[curr_cpu].rq[rq_num].tail++;
		cpu_run_queues[curr_cpu].rq[rq_num].count++;
		curr_process = &((cpu_run_queues[curr_cpu].rq[rq_num]).processes[buf_tail]);

		curr_process->pid = i;																	 
		curr_process->state = READY;														
		curr_process->sched_type = sched_type;									
		curr_process->priority = priority;						
 		curr_process->expected_exec_time = (rand()%40+1) *100;	
 		curr_process->remaining_exec_time = curr_process->expected_exec_time;
	}

	
	print_processes(cpu_run_queues, NUM_CPU);
	sleep(8);				


	for(i = 0; i < NUM_CPU; i++) {
		res = pthread_mutex_unlock(&queue_mutex[i]);
		if (res != 0) {
			perror("Mutex unlock failed\n");
			exit(EXIT_FAILURE);
		}
	}

	pthread_exit(NULL);
}


void *consumer_thread_function(void *arg) 
{
	int cpu_num = *(int *)arg;
	int res;
	int time_slice, sched_type;
	int s_time, w_time;
	int rq_num, buf_index;
	process_struct *process_to_run;

	int temp_sleep_avg;
	
	

	while(!all_processes_finished) {
	
		


		res = pthread_mutex_lock(&queue_mutex[cpu_num]);
		if (res != 0) {
			perror("Mutex lock failed\n");
			exit(EXIT_FAILURE);
		}


		process_to_run = get_process_to_run(&(cpu_run_queues[cpu_num]), &rq_num, &buf_index);

		if (process_to_run != NULL) {				
			time_slice = process_to_run->time_slice = get_quantum_size(process_to_run);		
			sched_type = process_to_run->sched_type;																		
			process_to_run->state = RUNNING;																				
			
			temp_sleep_avg = process_to_run->sleep_avg;
		}


		res = pthread_mutex_unlock(&queue_mutex[cpu_num]);
		if (res != 0) {
			perror("Mutex unlock failed\n");
			exit(EXIT_FAILURE);
		}

	
		if(process_to_run == NULL) {
		
			
			all_processes_finished=1;
			break;
		}

		
		
		print_action(cpu_num, rq_num, process_to_run, time_slice);
		sleep( time_slice/1000 );					
		usleep( (time_slice%1000)*1000 );			
	
		


		res = pthread_mutex_lock(&queue_mutex[cpu_num]);
		if (res != 0) {
			perror("Mutex lock failed\n");
			exit(EXIT_FAILURE);
		}

		
	
		process_to_run->accu_time_slice += time_slice;
		process_to_run->remaining_exec_time = process_to_run->expected_exec_time - process_to_run->accu_time_slice;
		process_to_run->last_cpu = cpu_num;
	
		if(sched_type == MY_SCHED_NORMAL)
			process_to_run->priority = max(100, min(process_to_run->priority - temp_sleep_avg+5, 139));
		
	
		if(process_to_run->remaining_exec_time <= 0) {
			process_to_run->state = FINISHED;
			print_action(cpu_num, rq_num, process_to_run, 0);
			delete_process(&(cpu_run_queues[cpu_num]), rq_num, buf_index);
			process_to_run = NULL;
		}
		else {												
			process_to_run->state = READY;

		
			if( (process_to_run->priority >= 130) && (rq_num == 1) ) {
				process_to_run = move_process(2, 1, &(cpu_run_queues[cpu_num]), buf_index);		
				rq_num = 2;
			}
			else if ( (process_to_run->priority < 130) && (rq_num == 2) ) {
				process_to_run = move_process(1, 2, &(cpu_run_queues[cpu_num]), buf_index);			
				rq_num = 1;
			}
			print_action(cpu_num, rq_num, process_to_run, 0);
		}

	
		res = pthread_mutex_unlock(&queue_mutex[cpu_num]);
		if (res != 0) {
			perror("Mutex unlock failed\n");
			exit(EXIT_FAILURE);
		}
	}

  pthread_exit(NULL);
}



 


void print_processes(cpu_run_queue *cpu_run_queues, int num_cpu)
{
	int cpu_num, rq_num, process_num;
	circular_buffer *curr_buffer;
	process_struct *curr_process;

	for(cpu_num = 0; cpu_num < num_cpu; cpu_num++) {
		printf("for the processors below, CPU# = %d \n", cpu_num);

		for(rq_num = 0; rq_num < NUM_RQ; rq_num++) {
			curr_buffer = &(cpu_run_queues[cpu_num].rq[rq_num]);

			for(process_num = curr_buffer->head; process_num < curr_buffer->tail; process_num++) {
				curr_process = &(curr_buffer->processes[process_num]);
				
				print_detail(rq_num, curr_process);
			}
		}
	}

}



void print_detail(int rq_num, process_struct *process)
{
	printf("RQ# = %d, ", rq_num);
	printf("PID = %2d, ", process->pid);
	printf("SCHED = ");
	if(process->sched_type==MY_SCHED_FIFO){
		printf("FIFO, ");
		}
	if(process->sched_type==MY_SCHED_RR){
		printf("RR, ");
		}
	if(process->sched_type==MY_SCHED_NORMAL){
		printf("NORMAL, ");
		}
	
	printf("Priority = %3d, Initial expected execution time = %02d\n\n", process->priority, process->expected_exec_time);
}



void print_action(int cpu_num, int rq_num, process_struct *process, int s_time)

{
	int print_service_t = 0;
	
	if(process->priority <139){
	sleep(1);
	
		printf("CPU# = %d, RQ# = %d, PID= %02d, ", cpu_num, rq_num, process->pid);

	// Print sched_type
	
	if(process->sched_type==MY_SCHED_FIFO){
		printf("SCHED =FIFO, ");
		}
	if(process->sched_type==MY_SCHED_RR){
		printf("SCHED =RR, ");
		}
	if(process->sched_type==MY_SCHED_NORMAL){
		printf("SCHED =NORM, ");
		}
	
	

	if(process->state == READY){
		printf("STATE =READY, ");
	}
	if(process->state == RUNNING){
		printf("STATE =RUNNING, ");
		print_service_t = 1;
	}
	if(process->state == FINISHED){
		printf("STATE= FINISH, ");
	
	}



	printf("priority =%3d,Expected ex Time =%4d, ", process->priority, process->expected_exec_time);						


	if (print_service_t)
		printf("Service Time =%4d\n\n", s_time);
	else
		printf(" Service Time =NONE\n\n");
		
	
	
	}
	
	

	
}


process_struct * get_process_to_run(cpu_run_queue *current_run_queue, int *rq_num, int *buf_index)
{
	int i, j;
	circular_buffer *curr_buffer;

	int temp_priority = 140;
	process_struct *process_to_run = NULL;


	for(i = 0; (i < NUM_RQ) && (process_to_run == NULL); i++) {
		curr_buffer = &(current_run_queue->rq[i]);


		if( curr_buffer->count != 0 ) {


			for(j = curr_buffer->head; j < curr_buffer->tail; j++) {

				if( (curr_buffer->processes[j]).priority < temp_priority ) {
					temp_priority = (curr_buffer->processes[j]).priority;
					process_to_run = &(curr_buffer->processes[j]);
					*buf_index = j;
					*rq_num = i;
				}				
			}
		}
	}

	return process_to_run;
}



void delete_process(cpu_run_queue *current_run_queue, int rq_num, int buf_index)
{
	int i;
	circular_buffer *curr_buffer;
	process_struct *process_hole;

	curr_buffer = &(current_run_queue->rq[rq_num]);


	for(i = buf_index+1; i < curr_buffer->tail; i++) {
		process_hole = &(curr_buffer->processes[i-1]);
		*process_hole = curr_buffer->processes[i];
	}
	curr_buffer->tail--;
	curr_buffer->count--;
}

process_struct * move_process(int rq_num_dest, int rq_num_src, cpu_run_queue *current_run_queue, int buf_index)
{
	circular_buffer *dest_circ_buffer, *src_circ_buffer;

	src_circ_buffer = &(current_run_queue->rq[rq_num_src]);
	dest_circ_buffer = &(current_run_queue->rq[rq_num_dest]);

	dest_circ_buffer->processes[dest_circ_buffer->tail] = src_circ_buffer->processes[buf_index];	
	dest_circ_buffer->tail++;
	dest_circ_buffer->count++;

	delete_process(current_run_queue, rq_num_src, buf_index);

	return &( dest_circ_buffer->processes[dest_circ_buffer->tail-1] );
}


int get_quantum_size(process_struct *process)
{
	int q_size;

	
	if(process->sched_type == MY_SCHED_FIFO) {
		return process->remaining_exec_time;
	}
	else {
		if(process->priority < 120) {
			q_size = (140 - process->priority)*20;
		}
		else {
			q_size = (140 - process->priority)*5;
		}
	}

	q_size *= Q_SIZE_MULTIPLIER;		

	if ( q_size > process->remaining_exec_time ) {
		q_size = process->remaining_exec_time;
	}
	return q_size;			
}

int max(int a, int b)
{
	if (a > b)
		return a;
	else
		return b;
}

int min(int a, int b)
{
	if (a < b)
		return a;
	else
		return b;
}


