
#ifndef ASSGN3_H
#define ASSGN3_H



#define NUM_RQ 3						
#define MAX_QUEUE_SIZE 10		


#define MY_SCHED_FIFO 0
#define MY_SCHED_RR 1
#define MY_SCHED_NORMAL 2


#define READY 0
#define RUNNING 1
#define FINISHED 2


#define MAX_SLEEP_AVG 10


#define NUM_CPU 4
#define Q_SIZE_MULTIPLIER 3 	


typedef struct {
	int pid;								
	int state;								
	
	int sched_type;					
	int priority;							
	int expected_exec_time;		
	
	int sleep_avg;						
	int time_slice;						
	int accu_time_slice;			
	int remaining_exec_time;	
	int last_cpu;						
			
} process_struct;


typedef struct {
	int head;
	int tail;
	int count;
	process_struct processes[MAX_QUEUE_SIZE];
} circular_buffer;


typedef struct {
	circular_buffer rq[NUM_RQ];
} cpu_run_queue;

	

extern int num_processes;
											
extern cpu_run_queue cpu_run_queues[NUM_CPU];

extern pthread_mutex_t queue_mutex[NUM_CPU];  




void *producer_thread_function(void *arg);

void *consumer_thread_function(void *arg);




void print_processes(cpu_run_queue *cpu_run_queues, int num_cpu);


void print_detail(int rq_num, process_struct *process);

void print_action(int cpu_num, int rq_num, process_struct *process, int s_time);



process_struct * get_process_to_run(cpu_run_queue *current_run_queue, int *rq_num, int *buf_index);



void delete_process(cpu_run_queue *current_run_queue, int rq_num, int buf_index);



process_struct * move_process(int rq_num_dest, int rq_num_src, cpu_run_queue *current_run_queue, int buf_index);



int get_quantum_size(process_struct *process);

int max(int a, int b);

int min(int a, int b);




#endif /* ASSGN3_H */
