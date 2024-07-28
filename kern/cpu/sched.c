#include "sched.h"

#include <inc/assert.h>

#include <kern/proc/user_environment.h>
#include <kern/trap/trap.h>
#include <kern/mem/kheap.h>
#include <kern/mem/memory_manager.h>
#include <kern/tests/utilities.h>
#include <kern/cmd/command_prompt.h>


uint32 isSchedMethodRR(){if(scheduler_method == SCH_RR) return 1; return 0;}
uint32 isSchedMethodMLFQ(){if(scheduler_method == SCH_MLFQ) return 1; return 0;}
uint32 isSchedMethodBSD(){if(scheduler_method == SCH_BSD) return 1; return 0;}

//===================================================================================//
//============================ SCHEDULER FUNCTIONS ==================================//
//===================================================================================//

//===================================
// [1] Default Scheduler Initializer:
//===================================
void sched_init()
{
	old_pf_counter = 0;

	sched_init_RR(INIT_QUANTUM_IN_MS);

	init_queue(&env_new_queue);
	init_queue(&env_exit_queue);
	scheduler_status = SCH_STOPPED;
}

//=========================
// [2] Main FOS Scheduler:
//=========================
void
fos_scheduler(void)
{
	//	cprintf("inside scheduler\n");

	chk1();
	scheduler_status = SCH_STARTED;

	//This variable should be set to the next environment to be run (if any)
	struct Env* next_env = NULL;

	if (scheduler_method == SCH_RR)
	{
		// Implement simple round-robin scheduling.
		// Pick next environment from the ready queue,
		// and switch to such environment if found.
		// It's OK to choose the previously running env if no other env
		// is runnable.

		//If the curenv is still exist, then insert it again in the ready queue
		if (curenv != NULL)
		{
			enqueue(&(env_ready_queues[0]), curenv);
		}

		//Pick the next environment from the ready queue
		next_env = dequeue(&(env_ready_queues[0]));

		//Reset the quantum
		//2017: Reset the value of CNT0 for the next clock interval
		kclock_set_quantum(quantums[0]);
		//uint16 cnt0 = kclock_read_cnt0_latch() ;
		//cprintf("CLOCK INTERRUPT AFTER RESET: Counter0 Value = %d\n", cnt0 );

	}
	else if (scheduler_method == SCH_MLFQ)
	{
		next_env = fos_scheduler_MLFQ();
	}
	else if (scheduler_method == SCH_BSD)
	{
		next_env = fos_scheduler_BSD();
	}
	//temporarily set the curenv by the next env JUST for checking the scheduler
	//Then: reset it again
	struct Env* old_curenv = curenv;
	curenv = next_env ;
	chk2(next_env) ;
	curenv = old_curenv;

	//sched_print_all();

	if(next_env != NULL)
	{
		//		cprintf("\nScheduler select program '%s' [%d]... counter = %d\n", next_env->prog_name, next_env->env_id, kclock_read_cnt0());
		//		cprintf("Q0 = %d, Q1 = %d, Q2 = %d, Q3 = %d\n", queue_size(&(env_ready_queues[0])), queue_size(&(env_ready_queues[1])), queue_size(&(env_ready_queues[2])), queue_size(&(env_ready_queues[3])));
		env_run(next_env);
	}
	else
	{
		/*2015*///No more envs... curenv doesn't exist any more! return back to command prompt
		curenv = NULL;
		//lcr3(K_PHYSICAL_ADDRESS(ptr_page_directory));
		lcr3(phys_page_directory);

		//cprintf("SP = %x\n", read_esp());

		scheduler_status = SCH_STOPPED;
		//cprintf("[sched] no envs - nothing more to do!\n");
		while (1)
			run_command_prompt(NULL);

	}
}

//=============================
// [3] Initialize RR Scheduler:
//=============================
void sched_init_RR(uint8 quantum)
{

	// Create 1 ready queue for the RR
	num_of_ready_queues = 1;
#if USE_KHEAP
	sched_delete_ready_queues();
	env_ready_queues = kmalloc(sizeof(struct Env_Queue));
	quantums = kmalloc(num_of_ready_queues * sizeof(uint8)) ;
#endif
	quantums[0] = quantum;
	kclock_set_quantum(quantums[0]);
	init_queue(&(env_ready_queues[0]));

	//=========================================
	//DON'T CHANGE THESE LINES=================
	scheduler_status = SCH_STOPPED;
	scheduler_method = SCH_RR;
	//=========================================
	//=========================================
}

//===============================
// [4] Initialize MLFQ Scheduler:
//===============================
void sched_init_MLFQ(uint8 numOfLevels, uint8 *quantumOfEachLevel)
{
#if USE_KHEAP
	//=========================================
	//DON'T CHANGE THESE LINES=================
	sched_delete_ready_queues();
	//=========================================
	//=========================================

	//=========================================
	//DON'T CHANGE THESE LINES=================
	scheduler_status = SCH_STOPPED;
	scheduler_method = SCH_MLFQ;
	//=========================================
	//=========================================
#endif
}

//===============================
// [5] Initialize BSD Scheduler:
//===============================
void sched_init_BSD(uint8 numOfLevels, uint8 quantum)
{
	//TODO: [PROJECT'23.MS3 - #4] [2] BSD SCHEDULER - sched_init_BSD
		//Your code is here
		//Comment the following line
		//panic("Not implemented yet");
		num_of_ready_queues = numOfLevels;
	#if USE_KHEAP
		sched_delete_ready_queues();
		env_ready_queues = kmalloc(num_of_ready_queues * sizeof(struct Env_Queue));
		quantums = kmalloc(num_of_ready_queues * sizeof(uint8)) ;


	#endif
		ticks=1;
		for(int i=0; i<num_of_ready_queues; i++)
		{
			quantums[i] = quantum;
		    kclock_set_quantum(quantums[i]);
		    init_queue(&(env_ready_queues[i]));
		}


		//=========================================
		//DON'T CHANGE THESE LINES=================
		scheduler_status = SCH_STOPPED;
		scheduler_method = SCH_BSD;
		//=========================================
		//=========================================
}


//=========================
// [6] MLFQ Scheduler:
//=========================
struct Env* fos_scheduler_MLFQ()
{
	panic("not implemented");
	return NULL;
}

//=========================
// [7] BSD Scheduler:
//=========================
struct Env *fos_scheduler_BSD()
{
	// TODO: [PROJECT'23.MS3 - #5] [2] BSD SCHEDULER - fos_scheduler_BSD
	// Your code is here
	// Comment the following line
	// panic("Not implemented yet");
	if (curenv != NULL)
	{
		curenv->env_status = ENV_READY;
		enqueue(&(env_ready_queues[curenv->priority]), curenv);
	}

	struct Env *next_env = NULL;
	for (int i = num_of_ready_queues - 1; i >= 0; i--)
	{
		if (env_ready_queues[i].size > 0)
		{
			next_env = dequeue(&(env_ready_queues[i]));
			kclock_set_quantum(quantums[i]);
			return next_env;
		}
	}

	load_avg = fix_int(0);

	// panic("Not implemented yet");
	return NULL;
}

//========================================
// [8] Clock Interrupt Handler
//	  (Automatically Called Every Quantum)
//========================================
void clock_interrupt_handler()
{
	//TODO: [PROJECT'23.MS3 - #5] [2] BSD SCHEDULER - Your code is here
	{
			fixed_point_t Recent_CPU = curenv->recent_cpu;
			int Nice_Value = curenv->nice;
			fixed_point_t Load_Average = load_avg;
			int Priority;
			int64 number_of_ticks = timer_ticks();
			int time_ms = quantums[0] * number_of_ticks; // Current tick
			int time_pre_ms = (quantums[0] * (number_of_ticks-1)); // Previous tick
			fixed_point_t fixed_1 = fix_int((int)1); // convert 1 to fixed point

			if ((time_ms / 1000) > (time_pre_ms / 1000))
			{
				int Ready_Processes = (curenv != NULL); // Running process
				for (int i = PRI_MIN; i < num_of_ready_queues; i++)
				{
					Ready_Processes += LIST_SIZE(&env_ready_queues[i]); // Ready processes
				}
				// Load Average
				fixed_point_t fixed_59 = fix_int((int)59);													// convert 59 to fixed point
				fixed_point_t fixed_60 = fix_int((int)60);													// convert 60 to fixed point
				fixed_point_t div59over60 = fix_div(fixed_59, fixed_60);									// convert 59/60 to fixed point
				fixed_point_t load_multiple_59div60 = fix_mul(Load_Average, div59over60);					//(59/60)*load_avg
				fixed_point_t fixed_ready_processes = fix_int((int)Ready_Processes);						// convert ready processes to fixed point
				fixed_point_t div1over60 = fix_div(fixed_1, fixed_60);										// convert 1/60 to fixed point
				fixed_point_t ready_processes_multiple_1div60 = fix_mul(fixed_ready_processes, div1over60); //(1/60)*ready processes
				fixed_point_t result = fix_add(load_multiple_59div60, ready_processes_multiple_1div60);		//(59/60)*load_avg + (1/60)*ready processes

				Load_Average = result; // Recalculated once per second

				load_avg = Load_Average; // Updated

			}

			if (curenv != NULL)
			{
				// Each time a timer interrupt occurs: it is incremented by 1 for the running process only
				fixed_point_t result_recent = fix_add(Recent_CPU, fixed_1); // Recent cpu + 1
				Recent_CPU = result_recent;

				curenv->recent_cpu = Recent_CPU; // Updated
			}

			// To check if count one second
			// Quantum = ms ; 1s = 1000ms
			if ((time_ms / 1000) > (time_pre_ms / 1000))
			{

				if (curenv != NULL)
				{
					// Recent CPU
					fixed_point_t load_mul2 = fix_scale(Load_Average, (int)2); // Load_avg*2
					fixed_point_t load_mul2_plus1 = fix_add(load_mul2, fixed_1); // 2*load_avg+1
					fixed_point_t division = fix_div(load_mul2, load_mul2_plus1); // 2*load_avg/2*load_avg+1
					fixed_point_t multiply = fix_mul(division, curenv->recent_cpu);	// 2*load_avg/2*load_avg+1*recent
					fixed_point_t fixed_recent = fix_add(multiply, fix_int((int)curenv->nice)); // 2*load_avg/2*load_avg+1*recent+nice

					curenv->recent_cpu = fixed_recent; // Updated
				}
				// Once per second: it is recalculated for every process (running, ready, or blocked)

				for (int i = 0; i < num_of_ready_queues; i++)
				{
					struct Env *for_every_processes;
					LIST_FOREACH(for_every_processes, &env_ready_queues[i])
					{

						// Recent CPU
						fixed_point_t load_mul2 = fix_scale(Load_Average, (int)2); // 2*load_avg												 // convert 1 to fixed point
						fixed_point_t load_mul2_plus1 = fix_add(load_mul2, fixed_1); // 2*load_avg+1
						fixed_point_t division = fix_div(load_mul2, load_mul2_plus1); // 2*load_avg/2*load_avg+1
						fixed_point_t multiply = fix_mul(division, for_every_processes->recent_cpu); // 2*load_avg/2*load_avg+1*recent
						fixed_point_t fixed_recent = fix_add(multiply, fix_int((int)for_every_processes->nice)); // 2*load_avg/2*load_avg+1*recent+nice

						for_every_processes->recent_cpu = fixed_recent; // Updated
					}
				}
			}

			// To check if count 4th tick
			if (number_of_ticks % 4 == 0)
			{
				if (curenv != NULL)
				{
					// Priority
					fixed_point_t recent_over4 = fix_div(Recent_CPU, fix_int((int)4)); // Recent/4
					fixed_point_t nice_mul2 = fix_mul(fix_int((int)Nice_Value), fix_int((int)2)); // Nice*2
					fixed_point_t fix_num_of_ready = fix_int(num_of_ready_queues); // Convert number of ready queues to fixed point
					fixed_point_t num_of_ready_sub_one = fix_sub(fix_num_of_ready,fixed_1);// Number of ready queues - 1
					fixed_point_t pri_sub_recent = fix_sub(num_of_ready_sub_one, recent_over4);// PRI_MAX-[recent/4]
					fixed_point_t priority = fix_sub(pri_sub_recent, nice_mul2); // PRI_MAX-[recent/4]-[nice*2]

					// Result should be rounded down to the nearest integer (truncated)
					Priority = fix_trunc(priority); // every 4th tick.

					// Adjusted to lie in the valid range PRI_MIN to PRI_MAX
					if (Priority < PRI_MIN)
					{
						Priority = PRI_MIN;
					}
					else if (Priority > num_of_ready_queues - 1)
					{
						Priority = num_of_ready_queues - 1;
					}

					curenv->priority = Priority; // Updated
				}
				// For every processes
				for (int i = 0; i < num_of_ready_queues; i++)
				{
					struct Env *for_every_processes;
					LIST_FOREACH(for_every_processes, &env_ready_queues[i])
					{
						// Priority
						fixed_point_t recent_over4 = fix_div(for_every_processes->recent_cpu, fix_int((int)4));	// Recent/4
						fixed_point_t nice_mul2 = fix_mul(fix_int((int)for_every_processes->nice), fix_int((int)2)); // Nice*2
						fixed_point_t fix_num_of_ready = fix_int(num_of_ready_queues);// Convert number of ready queues to fixed point
						fixed_point_t num_of_ready_sub_one = fix_sub(fix_num_of_ready,fixed_1);// Number of ready queues - 1
						fixed_point_t pri_sub_recent = fix_sub(num_of_ready_sub_one, recent_over4);// PRI_MAX-[recent/4]
						fixed_point_t priority = fix_sub(pri_sub_recent, nice_mul2); // PRI_MAX-[recent/4]-[nice*2]

						// Result should be rounded down to the nearest integer (truncated)
						int Current_Priority = fix_trunc(priority); // Every 4th tick.

						// Adjusted to lie in the valid range PRI_MIN to PRI_MAX
						if (Current_Priority < PRI_MIN)
						{
							Current_Priority = PRI_MIN;
						}
						else if (Current_Priority > num_of_ready_queues - 1)
						{
							Current_Priority = num_of_ready_queues - 1;
						}

						int Previous_Priority = for_every_processes->priority; // Previous Priority
						for_every_processes->priority = Current_Priority;	   // updated

						if (Previous_Priority != Current_Priority)
						{
							remove_from_queue(&env_ready_queues[Previous_Priority], for_every_processes);
							enqueue(&env_ready_queues[Current_Priority], for_every_processes);
						}
					}
				}
			}
		}

	/********DON'T CHANGE THIS LINE***********/
	ticks++ ;
	if(isPageReplacmentAlgorithmLRU(PG_REP_LRU_TIME_APPROX))
	{
		update_WS_time_stamps();
	}
	//cprintf("Clock Handler\n") ;
	fos_scheduler();
	/*****************************************/
}

//===================================================================
// [9] Update LRU Timestamp of WS Elements
//	  (Automatically Called Every Quantum in case of LRU Time Approx)
//===================================================================
void update_WS_time_stamps()
{
	struct Env *curr_env_ptr = curenv;

	if(curr_env_ptr != NULL)
	{
		struct WorkingSetElement* wse ;
		{
			int i ;
#if USE_KHEAP
			LIST_FOREACH(wse, &(curr_env_ptr->page_WS_list))
			{
#else
			for (i = 0 ; i < (curr_env_ptr->page_WS_max_size); i++)
			{
				wse = &(curr_env_ptr->ptr_pageWorkingSet[i]);
				if( wse->empty == 1)
					continue;
#endif
				//update the time if the page was referenced
				uint32 page_va = wse->virtual_address ;
				uint32 perm = pt_get_page_permissions(curr_env_ptr->env_page_directory, page_va) ;
				uint32 oldTimeStamp = wse->time_stamp;

				if (perm & PERM_USED)
				{
					wse->time_stamp = (oldTimeStamp>>2) | 0x80000000;
					pt_set_page_permissions(curr_env_ptr->env_page_directory, page_va, 0 , PERM_USED) ;
				}
				else
				{
					wse->time_stamp = (oldTimeStamp>>2);
				}
			}
		}

		{
			int t ;
			for (t = 0 ; t < __TWS_MAX_SIZE; t++)
			{
				if( curr_env_ptr->__ptr_tws[t].empty != 1)
				{
					//update the time if the page was referenced
					uint32 table_va = curr_env_ptr->__ptr_tws[t].virtual_address;
					uint32 oldTimeStamp = curr_env_ptr->__ptr_tws[t].time_stamp;

					if (pd_is_table_used(curr_env_ptr->env_page_directory, table_va))
					{
						curr_env_ptr->__ptr_tws[t].time_stamp = (oldTimeStamp>>2) | 0x80000000;
						pd_set_table_unused(curr_env_ptr->env_page_directory, table_va);
					}
					else
					{
						curr_env_ptr->__ptr_tws[t].time_stamp = (oldTimeStamp>>2);
					}
				}
			}
		}
	}
}

