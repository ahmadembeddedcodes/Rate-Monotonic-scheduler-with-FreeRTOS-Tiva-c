// need to also change configMAX_PRIORITIES accordingly
#define N 5
#define TST 1
#define LATEST_ARRIVAL_TIME 15
#define MAXIMUM_COMPUTATION_TIME 8
#define MAXIMUM_PERIOD_MULTIPLER 17
#define MODE NO_GUARANTEE_MODE
#define RAND_SEED 444

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "tm4c123gh6pm.h"
#include "My_queue.h"
#include "inc/hw_memmap.h"
#include "inc/hw_sysctl.h"
#include "inc/hw_types.h"
#include "inc/hw_uart.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "utils/uartstdio.h"

typedef struct task_data {
	
	unsigned int arrival_time;
	unsigned int period;
	unsigned int computation_time;
	unsigned int priorty;
	unsigned int delete_time;
	TaskHandle_t handler;
	char* name;
	
}task_data;

enum Event_Type {ARRIVAL, DELETE};

// passed to generate_task_data() func to determine
// how to caluclate task period
enum Generation_Mode {
	SAFE_MODE = true, 
	NO_GUARANTEE_MODE = false
};

typedef struct Task_Event {
	
	unsigned int event_time;
	enum Event_Type type;
	task_data* task;
	
}Task_Event;

// This struct just wraps the queues to pass it to our scheduler
typedef struct Event_Queues {
	
	struct My_queue* arrival_events;
	struct My_queue* delete_events;
	task_data** data;
	
}Event_Queues;


void uart_init(void);
void ports_init(void);

static void random_task(void *pvParameters);
void vApplicationIdleHook(void);
void monotonic_scheduler(void *pvParameters);
void wait_1ms(unsigned long msec);
unsigned int rand_range(unsigned int lower, unsigned int upper);
void generate_task_data(unsigned n, task_data** data, bool safe_mode);
float cpu_utilization(unsigned int n, task_data** data);
void assign_priorty(unsigned int n, task_data** data);
int cmp_period(const void* a, const void* b);
int cmp_arrival(const void* a, const void* b);
int cmp_delete(const void* a, const void* b);
void float_split(float f, int *upper, int *lower, int decimals);
void process_event(Task_Event* event, unsigned int time_slice);
void delete_task_data(task_data* task, task_data** task_array, unsigned n);
void fix_task_priorities(task_data** task_array, unsigned int n);
void print_data(task_data* x);
void print_data_array(task_data** x, int n);
void print_task_list(void);


int main(void)
{
	ports_init();
	uart_init();	

	srand(RAND_SEED);
	
	int n = rand_range(0, N);
	UARTprintf("Number of Generated Tasks: %d \n", n);
	task_data** data = pvPortMalloc(sizeof(task_data*) * n);
	
	generate_task_data(n, data, MODE);
	float utilization = cpu_utilization(n, data);
	
	// we split float since UARTprintf gives error with %f
	int upper, lower;	
	float_split(utilization, &upper, &lower, 4);	
	
	if(utilization < 0.7 && n > 0) {
		UARTprintf("Utilization is : %d.%d scheduling ...\n", upper, lower);
	}
	else {
		UARTprintf("Utilization is : %d.%d can't schedule\n", upper, lower);
		for(;;);
		
	}

	assign_priorty(n, data);
	print_data_array(data, n);
	
	struct My_queue* arrival_events = createQueue(100);
	struct My_queue* delete_events = createQueue(100);
	
	// sort tasks ascendingly by their arrival time
	qsort(&data[0], n, sizeof(task_data*), cmp_arrival);
	
	for(int i = 0; i < n; i++) {
		xTaskCreate(random_task, data[i]->name, configMINIMAL_STACK_SIZE, data[i], data[i]->priorty, &(data[i]->handler));
		
		// if a task doesn't arrive at time 0 we suspend it 
		// and make an event so our schedular resumes it
		// when it's arrival time comes
		if(data[i]->arrival_time > 0) {
			vTaskSuspend(data[i]->handler);
			Task_Event* te = pvPortMalloc(sizeof(Task_Event));
			*te = (Task_Event){.event_time = data[i]->arrival_time, .type=ARRIVAL, .task=data[i]};
			enqueue(arrival_events, (void*)te); 
		}
	}
	
	// sort tasks ascendingly by their delete time
	qsort(&data[0], n, sizeof(task_data*), cmp_delete);
	
	// add events for task deletion
	for(int i = 0; i < n; i++) {
		Task_Event* te = pvPortMalloc(sizeof(Task_Event));
		*te = (Task_Event){.event_time = data[i]->delete_time, .type=DELETE, .task=data[i]};
		enqueue(delete_events, (void*)te); 
	}
		
	Event_Queues* queues = pvPortMalloc(sizeof(Event_Queues));
	*queues	=	(Event_Queues){.arrival_events = arrival_events, .delete_events = delete_events, .data = data};
	
	xTaskCreate(monotonic_scheduler, "Rate Monotonic Scheduler Task", configMINIMAL_STACK_SIZE, queues, configMAX_PRIORITIES, NULL);
	
	vTaskStartScheduler();
	
	for(;;);

}

/*-----------------------------------------------------------*/
void random_task(void *pvParameters) {
	
	TickType_t xLastWakeTime;
	TickType_t xSecondWakeTime;
	task_data* td = (task_data*)pvParameters;
	int computation = (*td).computation_time;
	int period  = (*td).period * 1000 * TST;
	
	for(;;){
		xLastWakeTime = xTaskGetTickCount();
		UARTprintf("%s is running \n", (*td).name);
		//xLastWakeTime = xTaskGetTickCount();
		wait_1ms(computation * 1000 * TST);
		//xSecondWakeTime = xTaskGetTickCount();
		//UARTprintf("%s time taken %d\n", (*td).name, (xSecondWakeTime - xLastWakeTime));
		UARTprintf("%s is blocking for reamining period\n", (*td).name);
		//xSecondWakeTime = xTaskGetTickCount();
		//UARTprintf("%s time taken %d\n", (*td).name, (xSecondWakeTime - xLastWakeTime));
		vTaskDelayUntil( &xLastWakeTime, period / portTICK_RATE_MS);
	}
	
	return;
}

void monotonic_scheduler(void *pvParameters) {
	
	struct My_queue* arrival_events = ((Event_Queues*)pvParameters)->arrival_events;
	struct My_queue* delete_events = ((Event_Queues*)pvParameters)->delete_events;
	task_data** data = ((Event_Queues*)pvParameters)->data;
			
	// -3 for our scheduler, idle and tmr svc tasks
	unsigned int num_of_tasks = uxTaskGetNumberOfTasks() - 3;
	unsigned int time_slice = 0;
	
	vTaskDelay((1000*TST) / portTICK_RATE_MS);
	for(;;){
		//UARTprintf("entering scheduler seconds: %d\n", time_slice);				
		time_slice++;
		
		Task_Event* arrival_event = (Task_Event*)front(arrival_events);
		while(arrival_event != NULL && time_slice >= arrival_event->event_time) {
			dequeue(arrival_events);
			process_event(arrival_event, time_slice);
			vPortFree(arrival_event);
			arrival_event = (Task_Event*)front(arrival_events);		
		}
		
		Task_Event* delete_event = (Task_Event*)front(delete_events);
		while(delete_event != NULL && time_slice >= delete_event->event_time) {
			
			dequeue(delete_events);
			process_event(delete_event, time_slice);
			delete_task_data(delete_event->task, data, num_of_tasks);
			vPortFree(delete_event);
			
			UARTprintf("Fixing priorities and restarting scheduler\n");
			
			vTaskSuspendAll();
			assign_priorty(num_of_tasks, data);
			num_of_tasks--;
			//print_data_array(data, num_of_tasks);
			fix_task_priorities(data, num_of_tasks);
			print_task_list();
			xTaskResumeAll();
			
			delete_event = (Task_Event*)front(delete_events);		
		}
			
		vTaskDelay((1000*TST) / portTICK_RATE_MS);
	}
}

void process_event(Task_Event* event, unsigned int time_slice) {
	
	TaskHandle_t handler = event->task->handler;
	if(event->type == ARRIVAL){
		UARTprintf("%s arrived at Time Slice %d\n", event->task->name, time_slice);
		vTaskResume(handler);
	}
	else if(event->type == DELETE) {
		UARTprintf("%s was deleted at Time Slice %d\n", event->task->name, time_slice);
		vTaskDelete(handler);
	}
}

void delete_task_data(task_data* task, task_data** task_array, unsigned n){ 
	int i;
	for(i = 0; i < n; i++) {
		if(task_array[i] == task)
			break;
	}
	vPortFree(task_array[i]->name);
	vPortFree(task_array[i]);
	task_array[i] = NULL;
}

void fix_task_priorities(task_data** task_array, unsigned int n) {
	for(int i = 0; i < n; i++) {
		TaskHandle_t handler = task_array[i]->handler;
		vTaskPrioritySet(handler, task_array[i]->priorty);
	}
}

void vApplicationIdleHook()
{

}

void ports_init(void)
{
	SYSCTL_RCGCGPIO_R |= 0x01; 						// activate port A
	GPIO_PORTA_AFSEL_R |= 0x03;           // enable alt funct on PA1-0
	GPIO_PORTA_DEN_R |= 0x03;             // enable digital I/O on PA1-0									
	GPIO_PORTA_PCTL_R |= 0x11;			  // configure PA1-0 as UART
	GPIO_PORTA_AMSEL_R &= ~0x03;          // disable analog functionality on PA
	
}

void uart_init(void){
	
	SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_UART0)) {}
	
	UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 9600,
		(UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
		UART_CONFIG_PAR_NONE));
		
	UARTStdioConfig(0,9600, SysCtlClockGet());
			
}

void wait_1ms(unsigned long msec)
{
	unsigned long count;
	while (msec > 0)
	{
		count = 8310;
		while (count > 0)
		{
			count--;
		}
		msec--;
	}
}

unsigned int rand_range(unsigned int lower, unsigned int upper) {
	
	return (rand() % (upper - lower + 1)) + lower;
}

void generate_task_data(unsigned n, task_data** data, bool safe_mode) {
	
	for(int i = 0; i < n; i++) {
		
		data[i] =  pvPortMalloc(sizeof(task_data));
		
		data[i]->arrival_time = rand_range(0, LATEST_ARRIVAL_TIME);
		
		unsigned int tci = rand_range(1, MAXIMUM_COMPUTATION_TIME);
		data[i]->computation_time = tci;
		
		if(safe_mode == true) {
			data[i]->period = rand_range(3 * tci, MAXIMUM_PERIOD_MULTIPLER * tci);					
		}
		else {
			data[i]->period = rand_range(3 * tci, 10 * tci);
		}	
		
		data[i]->delete_time = rand_range(data[i]->arrival_time + 2 * data[i]->period, 
																			data[i]->arrival_time + 6 * data[i]->period);
		
		char* buffer = pvPortMalloc(sizeof(char) * 10);
		sprintf(buffer, "Task %d", i);
		data[i]->name = buffer;
		
	}	
	
	return;
}

float cpu_utilization(unsigned int n, task_data** data) {
	
	float util = 0;
	for(int i = 0; i < n; i++) {
		util += ( (float)(data[i]->computation_time) / (data[i]->period) );
	}		
	return util;
}

void assign_priorty(unsigned int n, task_data** data) {
	// sorting by period descendingly
	qsort(&data[0], n, sizeof(task_data*), cmp_period);
	int lastperiod = 0;
	int priorty = 1;
	
	for(int i = 0; i < n; i++) {
		if(data[i] == NULL)
			break;
		
		if(data[i]->period == lastperiod) {
			data[i]->priorty = priorty;
		}
		else {
			data[i]->priorty = priorty++;
		}		
		lastperiod = data[i]->period;
	}
}

int cmp_period(const void* a, const void* b) {
	task_data* x = *((task_data**)a);
	task_data* y = *((task_data**)b);
	
	// keeps nulls at the end of array
	if (x == NULL)
		return (y != NULL);
	if (y == NULL)
		return -1;
	
	// sorts period descendingly
	return (y->period - x->period);
	
}

int cmp_arrival(const void* a, const void* b) {
	task_data* x = *((task_data**)a);
	task_data* y = *((task_data**)b);

	return (x->arrival_time - y->arrival_time);
}

int cmp_delete(const void* a, const void* b){
	task_data* x = *((task_data**)a);
	task_data* y = *((task_data**)b);

	return (x->delete_time - y->delete_time);
}

void float_split(float f, int *upper, int *lower, int decimals) {
	*upper = f;
	f -= *upper;
	f *= (int) pow((double)10, decimals);
	*lower = f;
}

void print_data(task_data* x) {
	UARTprintf("Task name: %s\n", x->name);
	UARTprintf("Task arrival: %u\n", x->arrival_time);
	UARTprintf("Task deletion: %u\n", x->delete_time);
	UARTprintf("Task period: %u\n", x->period);
	UARTprintf("Task computation_time: %u\n", x->computation_time);
	UARTprintf("Task priorty: %u\n", x->priorty);
	//UARTprintf("Task handler: %i\n", x->handler);
}

void print_data_array(task_data** x, int n) {
	UARTprintf("---------------------------\n");
	for(int i = 0; i < n; i++) {
		print_data(x[i]);
		UARTprintf("---------------------------\n");
	}
}

void print_task_list(void){
	UARTprintf("\n");
	UARTprintf("******************vTaskList******************\n");
	UARTprintf("Task            State   Prio    Stack   State\n");
	UARTprintf("*********************************************\n");
	char* buffer = pvPortMalloc(sizeof(char) * 512);
	vTaskList(buffer);
	UARTprintf(buffer);
	vPortFree(buffer);
	UARTprintf("*********************************************\n");
	UARTprintf("\n");
}
