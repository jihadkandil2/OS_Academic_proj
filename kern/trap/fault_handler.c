/*
 * fault_handler.c
 *
 *  Created on: Oct 12, 2022
 *      Author: HP
 */

#include "trap.h"
#include <kern/proc/user_environment.h>
#include <kern/mem/kheap.h>
#include "../cpu/sched.h"
#include "../disk/pagefile_manager.h"
#include "../mem/memory_manager.h"


//2014 Test Free(): Set it to bypass the PAGE FAULT on an instruction with this length and continue executing the next one
// 0 means don't bypass the PAGE FAULT
uint8 bypassInstrLength = 0;

//===============================
// REPLACEMENT STRATEGIES
//===============================
//2020
void setPageReplacmentAlgorithmLRU(int LRU_TYPE)
{
	assert(LRU_TYPE == PG_REP_LRU_TIME_APPROX || LRU_TYPE == PG_REP_LRU_LISTS_APPROX);
	_PageRepAlgoType = LRU_TYPE ;
}
void setPageReplacmentAlgorithmCLOCK(){_PageRepAlgoType = PG_REP_CLOCK;}
void setPageReplacmentAlgorithmFIFO(){_PageRepAlgoType = PG_REP_FIFO;}
void setPageReplacmentAlgorithmModifiedCLOCK(){_PageRepAlgoType = PG_REP_MODIFIEDCLOCK;}
/*2018*/ void setPageReplacmentAlgorithmDynamicLocal(){_PageRepAlgoType = PG_REP_DYNAMIC_LOCAL;}
/*2021*/ void setPageReplacmentAlgorithmNchanceCLOCK(int PageWSMaxSweeps){_PageRepAlgoType = PG_REP_NchanceCLOCK;  page_WS_max_sweeps = PageWSMaxSweeps;}

//2020
uint32 isPageReplacmentAlgorithmLRU(int LRU_TYPE){return _PageRepAlgoType == LRU_TYPE ? 1 : 0;}
uint32 isPageReplacmentAlgorithmCLOCK(){if(_PageRepAlgoType == PG_REP_CLOCK) return 1; return 0;}
uint32 isPageReplacmentAlgorithmFIFO(){if(_PageRepAlgoType == PG_REP_FIFO) return 1; return 0;}
uint32 isPageReplacmentAlgorithmModifiedCLOCK(){if(_PageRepAlgoType == PG_REP_MODIFIEDCLOCK) return 1; return 0;}
/*2018*/ uint32 isPageReplacmentAlgorithmDynamicLocal(){if(_PageRepAlgoType == PG_REP_DYNAMIC_LOCAL) return 1; return 0;}
/*2021*/ uint32 isPageReplacmentAlgorithmNchanceCLOCK(){if(_PageRepAlgoType == PG_REP_NchanceCLOCK) return 1; return 0;}

//===============================
// PAGE BUFFERING
//===============================
void enableModifiedBuffer(uint32 enableIt){_EnableModifiedBuffer = enableIt;}
uint8 isModifiedBufferEnabled(){  return _EnableModifiedBuffer ; }

void enableBuffering(uint32 enableIt){_EnableBuffering = enableIt;}
uint8 isBufferingEnabled(){  return _EnableBuffering ; }

void setModifiedBufferLength(uint32 length) { _ModifiedBufferLength = length;}
uint32 getModifiedBufferLength() { return _ModifiedBufferLength;}

//===============================
// FAULT HANDLERS
//===============================




//Handle the table fault
void table_fault_handler(struct Env * curenv, uint32 fault_va)
{
	//panic("table_fault_handler() is not implemented yet...!!");
	//Check if it's a stack page
	uint32* ptr_table;
#if USE_KHEAP
	{
		ptr_table = create_page_table(curenv->env_page_directory, (uint32)fault_va);
	}
#else
	{
		__static_cpt(curenv->env_page_directory, (uint32)fault_va, &ptr_table);
	}
#endif
}
// our helper function
void placePage (struct Env * curenv, uint32 fault_va)
{
	struct FrameInfo* ptr_New_frame;
	allocate_frame(&ptr_New_frame); // if did not found free frame it will panic code will not continue
	map_frame(curenv->env_page_directory , ptr_New_frame , fault_va , PERM_USER|PERM_WRITEABLE|PERM_PRESENT);
	ptr_New_frame->va =fault_va ;
	struct WorkingSetElement *returnedObject = env_page_ws_list_create_element(curenv,fault_va);
	LIST_INSERT_TAIL(&(curenv->page_WS_list), returnedObject);

	if (LIST_SIZE(&(curenv->page_WS_list)) == curenv->page_WS_max_size)
	{
	   curenv->page_last_WS_element = LIST_FIRST(&(curenv->page_WS_list));
	}
	else
	{
		curenv->page_last_WS_element = NULL;
	}
}
//Handle the page fault
void page_fault_handler(struct Env * curenv, uint32 fault_va)
{
#if USE_KHEAP
		struct WorkingSetElement *victimWSElement = NULL;
		uint32 wsSize = LIST_SIZE(&(curenv->page_WS_list));
#else
		int iWS =curenv->page_last_WS_index;
		uint32 wsSize = env_page_ws_get_size(curenv);
#endif

if(isPageReplacmentAlgorithmFIFO())
{
	if(wsSize < (curenv->page_WS_max_size))
	{
		//TODO: [PROJECT'23.MS3 - #1] [1] PAGE FAULT HANDLER - FIFO Placement
		struct FrameInfo* ptr_New_frame;
		allocate_frame(&ptr_New_frame);
		map_frame(curenv->env_page_directory , ptr_New_frame , fault_va , PERM_USER|PERM_WRITEABLE|PERM_PRESENT);
		ptr_New_frame->va =fault_va ;
		int ret = pf_read_env_page(curenv, (void*)fault_va);
		if (ret == E_PAGE_NOT_EXIST_IN_PF)
		{
			if( (fault_va>=USER_HEAP_START && fault_va<USER_HEAP_MAX)||(fault_va>=USTACKBOTTOM && fault_va<USTACKTOP) )
			{
				struct WorkingSetElement *returnedObject = env_page_ws_list_create_element(curenv,fault_va);
				LIST_INSERT_TAIL(&(curenv->page_WS_list), returnedObject);

				if (LIST_SIZE(&(curenv->page_WS_list)) == curenv->page_WS_max_size)
				{
				   curenv->page_last_WS_element = LIST_FIRST(&(curenv->page_WS_list));
				}
				else
				{
					curenv->page_last_WS_element = NULL;
				}
			}
			else
			{
				sched_kill_env(curenv->env_id);
			}
		}
		else// it is found in page file
		{
			struct WorkingSetElement *returnedObject = env_page_ws_list_create_element(curenv,fault_va);
			LIST_INSERT_TAIL(&(curenv->page_WS_list), returnedObject);

			if (LIST_SIZE(&(curenv->page_WS_list)) == curenv->page_WS_max_size)
			{
			   curenv->page_last_WS_element = LIST_FIRST(&(curenv->page_WS_list));
			}
			else
			{
				curenv->page_last_WS_element = NULL;
			}
		}
	}
	else
	{
		//TODO: [PROJECT'23.MS3 - #1] [1] PAGE FAULT HANDLER - FIFO Replacement
		//env_page_ws_print(curenv );
		struct FrameInfo* ptr_frame_info;
		int ret;
		allocate_frame(&ptr_frame_info);
		map_frame(curenv->env_page_directory , ptr_frame_info , fault_va , PERM_USER|PERM_WRITEABLE|PERM_PRESENT);
		ptr_frame_info->va =fault_va;
	    ret=pf_read_env_page(curenv, (void*)fault_va);

		if (ret == E_PAGE_NOT_EXIST_IN_PF)
		{
			if( (fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX) || (fault_va < USTACKTOP && fault_va >= USTACKBOTTOM ) ) // جايز تكون بدجاية ستاك او هيب اتعملت وقت الرن تايم
			{
				struct WorkingSetElement *ptr_removed_element =curenv->page_last_WS_element;    // victim
				uint32 victimVa  = ptr_removed_element->virtual_address ;
				uint32 *env_page_dir  = (uint32 *)curenv->env_page_directory ;
				uint32 page_permission = pt_get_page_permissions(env_page_dir , victimVa);
				if (page_permission & PERM_MODIFIED)
				{
					uint32 *ptr_page_table ;
					get_page_table(env_page_dir , victimVa, &ptr_page_table);
					struct FrameInfo *ptr_newframe_info = get_frame_info(env_page_dir, victimVa , &ptr_page_table);
					int retrn = pf_update_env_page(curenv , victimVa ,ptr_newframe_info) ;
					if (retrn != 0 )
					{
						panic("Error no memory to write on disk ........");
					}
				}
				unmap_frame(env_page_dir,victimVa);
				env_page_ws_invalidate(curenv, victimVa);
				struct WorkingSetElement *newElement = env_page_ws_list_create_element(curenv,fault_va);
				LIST_INSERT_TAIL(&(curenv->page_WS_list), newElement);
			}
			else
			{
				sched_kill_env(curenv->env_id);
			}
		}
		else
		{
			struct WorkingSetElement *ptr_removed_element =curenv->page_last_WS_element;    // victim
			uint32 victimVa  = ptr_removed_element->virtual_address ;
			uint32 *env_page_dir  = (uint32 *)curenv->env_page_directory ;
			uint32 page_permission = pt_get_page_permissions(env_page_dir , victimVa);
			if (page_permission & PERM_MODIFIED)
			{
				uint32 *ptr_page_table ;
				get_page_table(env_page_dir , victimVa, &ptr_page_table);
				struct FrameInfo *ptr_newframe_info = get_frame_info(env_page_dir, victimVa , &ptr_page_table);
				int retrn = pf_update_env_page(curenv , victimVa ,ptr_newframe_info) ;
				if (retrn != 0 )
				{
					panic("Error no memory to write on disk ........");
				}
			}
			unmap_frame(env_page_dir,victimVa);

			env_page_ws_invalidate(curenv, victimVa);
			struct WorkingSetElement *newElement = env_page_ws_list_create_element(curenv,fault_va);
			LIST_INSERT_TAIL(&(curenv->page_WS_list), newElement);
        }
    }
  }
if (isPageReplacmentAlgorithmLRU(PG_REP_LRU_LISTS_APPROX))
{
	    fault_va=ROUNDDOWN(fault_va,PAGE_SIZE);
		uint32 activeListSize=LIST_SIZE(&curenv->ActiveList);
		uint32 secondListSize=LIST_SIZE(&curenv->SecondList);
		uint32 maxSize=curenv->ActiveListSize+curenv->SecondListSize;
		//TODO: [PROJECT'23.MS3 - #2] [1] PAGE FAULT HANDLER - LRU placement

	   if(activeListSize+secondListSize<maxSize) //Active not full
	   {
		   if(curenv->ActiveListSize!=activeListSize)//CHECK IF THE ACTIVELIST NOT FULL
			 {
				  struct FrameInfo* ptr_New_frame;
				  allocate_frame(&ptr_New_frame);
				  map_frame(curenv->env_page_directory,ptr_New_frame,fault_va,PERM_USER|PERM_WRITEABLE|PERM_PRESENT);
				  ptr_New_frame->va=fault_va;
				  int ret = pf_read_env_page(curenv, (void*)fault_va);//read page file
					   if (ret == E_PAGE_NOT_EXIST_IN_PF) //check if is not exist
					   {
						   if(!(fault_va>=USER_HEAP_START)&& !(fault_va<=USTACKTOP) )  //check is on stack or heap
							{
								// illegal access
								sched_kill_env(curenv->env_id);
								return ;
							}
					   }
				  struct WorkingSetElement *returnedObject = env_page_ws_list_create_element(curenv,fault_va);
				  pt_set_page_permissions(curenv->env_page_directory,returnedObject->virtual_address,PERM_PRESENT,0);
				  LIST_INSERT_HEAD(&(curenv->ActiveList), returnedObject);
			 }
		   else  //Active list if full
		   {
				uint8 foundflag=0;
				struct WorkingSetElement *foundinsecond=curenv->SecondList.lh_first;
				 LIST_FOREACH(foundinsecond,&(curenv->SecondList))//to check if the fault va in secondlist or not
				 {
					 if((unsigned int) fault_va==foundinsecond->virtual_address )
					   {
						   foundinsecond->virtual_address=fault_va;
						   foundflag=1;
						   break;
					   }
				 }
				 if(foundflag==1)//that means that the fault va in secondlist
				 {		 //take from second put in active (switching)
					LIST_REMOVE(&(curenv->SecondList),foundinsecond);
					 struct WorkingSetElement *switchedelement=LIST_LAST(&(curenv->ActiveList));
					 LIST_REMOVE(&(curenv->ActiveList),switchedelement);
					 pt_set_page_permissions(curenv->env_page_directory,foundinsecond->virtual_address,PERM_PRESENT,0);
					 LIST_INSERT_HEAD(&(curenv->ActiveList),foundinsecond);
					 pt_set_page_permissions(curenv->env_page_directory,switchedelement->virtual_address,0,PERM_PRESENT);
					 LIST_INSERT_HEAD(&(curenv->SecondList),switchedelement);
				 }
				 else //not exist in second take the last element in active and put it in second and insert in active
				 {
				  struct WorkingSetElement *switchedElement=LIST_LAST(&(curenv->ActiveList));
				  struct FrameInfo* ptr_New_frame;
				  allocate_frame(&ptr_New_frame);
				  map_frame(curenv->env_page_directory,ptr_New_frame,fault_va,PERM_USER|PERM_WRITEABLE|PERM_PRESENT);
				  ptr_New_frame->va=fault_va;
				  int ret = pf_read_env_page(curenv, (void*)fault_va);//read page file
					   if (ret == E_PAGE_NOT_EXIST_IN_PF) //check if is not exist
					   {
						   if(!(fault_va>=USER_HEAP_START)&&!(fault_va<=USTACKTOP) )  //check is not on stack or heap
							{
								// illegal access
							   cprintf("killing in lru 2 placementttt\n\n");
								sched_kill_env(curenv->env_id);
								return;
							}
					   }
					   pt_set_page_permissions(curenv->env_page_directory,fault_va,PERM_PRESENT,0);
					  struct WorkingSetElement *sended=env_page_ws_list_create_element(curenv,fault_va);
					  LIST_REMOVE(&(curenv->ActiveList),switchedElement);
					  LIST_INSERT_HEAD(&(curenv->ActiveList),sended);
					  pt_set_page_permissions(curenv->env_page_directory,switchedElement->virtual_address,0,PERM_PRESENT);
					  LIST_INSERT_HEAD(&(curenv->SecondList), switchedElement);
				 }
		   }
	   }
		else
		{
			uint8 flagInSecond=0;
			struct WorkingSetElement *it_in_second=curenv->SecondList.lh_first;
			LIST_FOREACH(it_in_second,&(curenv->SecondList))
			{
			  if (fault_va==it_in_second->virtual_address)
			  {
				  it_in_second->virtual_address=fault_va;
				  flagInSecond=1;
				  break;
			  }
			}
			if (flagInSecond== 1)//in second list.
			{
				 LIST_REMOVE(&(curenv->SecondList),it_in_second);
				 struct WorkingSetElement *switchedelement=LIST_LAST(&(curenv->ActiveList));
				 LIST_REMOVE(&(curenv->ActiveList),switchedelement);
				 pt_set_page_permissions(curenv->env_page_directory,it_in_second->virtual_address,PERM_PRESENT,0);
				 LIST_INSERT_HEAD(&(curenv->ActiveList),it_in_second);
				 pt_set_page_permissions(curenv->env_page_directory,switchedelement->virtual_address,0,PERM_PRESENT);
				 LIST_INSERT_HEAD(&(curenv->SecondList),switchedelement);
			}
			else //not in both lists
			{
			  struct FrameInfo* ptr_New_frame;
			  allocate_frame(&ptr_New_frame);
			  map_frame(curenv->env_page_directory,ptr_New_frame,fault_va,PERM_USER|PERM_WRITEABLE|PERM_PRESENT);
			  ptr_New_frame->va=fault_va;
			  int ret = pf_read_env_page(curenv, (void*)fault_va);//read page file
			  if (ret == E_PAGE_NOT_EXIST_IN_PF) //check if is not exist
			   {
				   if(!(fault_va>=USER_HEAP_START)&&!(fault_va<=USTACKTOP) )  //check is on stack or heap
					{
						//illegal access
						sched_kill_env(curenv->env_id);
						return;
					}
			   }
			   struct WorkingSetElement *victim=LIST_LAST(&(curenv->SecondList));
			   uint32 page_permission_for_victim = pt_get_page_permissions(curenv->env_page_directory , victim->virtual_address);
			   if (page_permission_for_victim & PERM_MODIFIED)
				{
						uint32 *ptr_page_table ;
						get_page_table(curenv->env_page_directory , victim->virtual_address, &ptr_page_table);
						struct FrameInfo *ptr_frame_info = get_frame_info(curenv->env_page_directory , victim->virtual_address , &ptr_page_table);
						int retrn = pf_update_env_page(curenv , victim->virtual_address ,ptr_frame_info);
						if (retrn != 0 )
						{
								panic("Error no memory to write on disk ........");
						}

				}

			    uint32 *ptr_page_table ;
				get_page_table(curenv->env_page_directory , victim->virtual_address, &ptr_page_table);
				struct FrameInfo *ptr_frame_info = get_frame_info(curenv->env_page_directory , victim->virtual_address , &ptr_page_table);
				unmap_frame(curenv->env_page_directory,victim->virtual_address);
			   LIST_REMOVE(&(curenv->SecondList) ,victim);
			   struct WorkingSetElement *switchedElement=LIST_LAST(&(curenv->ActiveList));
			   LIST_REMOVE(&(curenv->ActiveList),switchedElement);
			   pt_set_page_permissions(curenv->env_page_directory,fault_va,PERM_PRESENT, 0);
			   struct WorkingSetElement *faulted_page=env_page_ws_list_create_element(curenv,ROUNDDOWN(fault_va, PAGE_SIZE));
			  LIST_INSERT_HEAD(&(curenv->ActiveList),faulted_page);
			  pt_set_page_permissions(curenv->env_page_directory,switchedElement->virtual_address,0,PERM_PRESENT);
			  LIST_INSERT_HEAD(&(curenv->SecondList), switchedElement);
			}
		}
	}
	else if (isPageReplacmentAlgorithmLRU(PG_REP_LRU_LISTS_APPROX))
	{
		//TODO: [PROJECT'23.MS3 - BONUS] [1] PAGE FAULT HANDLER - O(1) implementation of
		fault_va=ROUNDDOWN(fault_va,PAGE_SIZE);
			uint32 activeListSize=LIST_SIZE(&curenv->ActiveList);
			uint32 secondListSize=LIST_SIZE(&curenv->SecondList);
			uint32 maxSize=curenv->ActiveListSize+curenv->SecondListSize;
			//TODO: [PROJECT'23.MS3 - #2] [1] PAGE FAULT HANDLER - LRU placement
			if( activeListSize+secondListSize <maxSize) //Active not full
			{
			   if(curenv->ActiveListSize!=activeListSize && secondListSize == 0)//CHECK IF THE ACTIVELIST NOT FULL
			   {
				  struct FrameInfo* ptr_New_frame;
				  allocate_frame(&ptr_New_frame);
				  map_frame(curenv->env_page_directory,ptr_New_frame,fault_va,PERM_USER|PERM_WRITEABLE|PERM_PRESENT);
				  ptr_New_frame->va=fault_va;
				  int ret = pf_read_env_page(curenv, (void*)fault_va);//read page file
				   if (ret == E_PAGE_NOT_EXIST_IN_PF) //check if is not exist
				   {
					   if((fault_va >= USER_HEAP_START && fault_va<USER_HEAP_MAX)||(fault_va>=USTACKBOTTOM && fault_va<USTACKTOP) )  //check is on stack or heap
						{
						   struct WorkingSetElement *returnedObject = env_page_ws_list_create_element(curenv,fault_va);
							  pt_set_page_permissions(curenv->env_page_directory,returnedObject->virtual_address,PERM_PRESENT,PERM_USED);
							  LIST_INSERT_HEAD(&(curenv->ActiveList), returnedObject);
						}
					   else
					   {
						   return sched_kill_env(curenv->env_id);
							//return ;
					   }
				   }
				   else
				   {
					  struct WorkingSetElement *returnedObject = env_page_ws_list_create_element(curenv,fault_va);
					  pt_set_page_permissions(curenv->env_page_directory,returnedObject->virtual_address,PERM_PRESENT,PERM_USED);
					  LIST_INSERT_HEAD(&(curenv->ActiveList), returnedObject);
				   }
			   } // اكتف مليانة ساكند عندها مكان
			   else if (curenv->ActiveListSize==activeListSize && curenv->SecondListSize != secondListSize) //Active list if full -> there is space in second list  -> there is no space will victim a page
			   {
					uint8 foundflag=0;
					struct WorkingSetElement *foundinsecond=curenv->SecondList.lh_first;
					 LIST_FOREACH(foundinsecond,&(curenv->SecondList))//to check if the fault va in secondlist or not
					 {
						 if((unsigned int) fault_va==foundinsecond->virtual_address )
						   {
							   foundinsecond->virtual_address=fault_va;
							   foundflag=1;
							   break;
						   }
					 }
					 if(foundflag==1)//that means that the fault va in secondlist
					 {
						 //take from second put in active (switching)
						LIST_REMOVE(&(curenv->SecondList),foundinsecond);                 //شيل من الساكند
						 struct WorkingSetElement *switchedelement=LIST_LAST(&(curenv->ActiveList));
						 LIST_REMOVE(&(curenv->ActiveList),switchedelement);              // شيل من الاكتف اخر واحد عشان نحطه في الساكند

						 pt_set_page_permissions(curenv->env_page_directory,switchedelement->virtual_address,PERM_USED,PERM_PRESENT);
						 LIST_INSERT_HEAD(&(curenv->SecondList),switchedelement);    // حط في الساكند الي شيلته من الاكتف

						 pt_set_page_permissions(curenv->env_page_directory,foundinsecond->virtual_address,PERM_PRESENT,PERM_USED);
						 LIST_INSERT_HEAD(&(curenv->ActiveList),foundinsecond);      // حط الفولت في الاكتف خلاص كدا
					 }
					 else //not exist in second take the last element in active and put it in second and insert in active
					 {
						  struct WorkingSetElement *switchedElement=LIST_LAST(&(curenv->ActiveList));
						  struct FrameInfo* ptr_New_frame;
						  allocate_frame(&ptr_New_frame);
						  map_frame(curenv->env_page_directory,ptr_New_frame,fault_va,PERM_USER|PERM_WRITEABLE|PERM_PRESENT);
						  ptr_New_frame->va=fault_va;
						  int ret = pf_read_env_page(curenv, (void*)fault_va);//read page file
						   if (ret == E_PAGE_NOT_EXIST_IN_PF) //check if is not exist
						   {
							   if((fault_va>=USER_HEAP_START && fault_va<USER_HEAP_MAX)||(fault_va>=USTACKBOTTOM && fault_va<USTACKTOP) ) //check is not on stack or heap
								{
								   LIST_REMOVE(&(curenv->ActiveList),switchedElement);
								  pt_set_page_permissions(curenv->env_page_directory,switchedElement->virtual_address,PERM_USED,PERM_PRESENT);
								  LIST_INSERT_HEAD(&(curenv->SecondList), switchedElement);
								  pt_set_page_permissions(curenv->env_page_directory,fault_va,PERM_PRESENT,PERM_USED);
								 struct WorkingSetElement *sended=env_page_ws_list_create_element(curenv,fault_va);
								  LIST_INSERT_HEAD(&(curenv->ActiveList),sended);
								}
							   else
							   {
								  return sched_kill_env(curenv->env_id);
							   }
						   }
						   else
						   {
							   LIST_REMOVE(&(curenv->ActiveList),switchedElement);
							  pt_set_page_permissions(curenv->env_page_directory,switchedElement->virtual_address,PERM_USED,PERM_PRESENT);
							  LIST_INSERT_HEAD(&(curenv->SecondList), switchedElement);
							  pt_set_page_permissions(curenv->env_page_directory,fault_va,PERM_PRESENT,PERM_USED);
							 struct WorkingSetElement *sended=env_page_ws_list_create_element(curenv,fault_va);
							  LIST_INSERT_HEAD(&(curenv->ActiveList),sended);
						   }
					 }
			   }
			}
		else
		{
			uint32 permissions = pt_get_page_permissions(curenv->env_page_directory , fault_va);
		   if (permissions & PERM_USED )//in second
		   {
			   struct WorkingSetElement *ptr= env_page_ws_list_create_element(curenv,fault_va);
				 LIST_REMOVE(&(curenv->SecondList),ptr);

				 struct WorkingSetElement *switchedelement=LIST_LAST(&(curenv->ActiveList));
				 LIST_REMOVE(&(curenv->ActiveList),switchedelement);
				 pt_set_page_permissions(curenv->env_page_directory,ptr->virtual_address,PERM_PRESENT,PERM_USED);
				 LIST_INSERT_HEAD(&(curenv->ActiveList),ptr);
				 pt_set_page_permissions(curenv->env_page_directory,switchedelement->virtual_address,PERM_USED,PERM_PRESENT);
				 LIST_INSERT_HEAD(&(curenv->SecondList),switchedelement);

		   }
		   else //in page file
		   {
			   struct FrameInfo* ptr_New_frame;
				  allocate_frame(&ptr_New_frame);
				  map_frame(curenv->env_page_directory,ptr_New_frame,fault_va,PERM_USER|PERM_WRITEABLE|PERM_PRESENT);
				  ptr_New_frame->va=fault_va;
				  int ret = pf_read_env_page(curenv, (void*)fault_va);//read page file
				  if (ret == E_PAGE_NOT_EXIST_IN_PF) //check if is not exist
				   {
					   if(!(fault_va>=USER_HEAP_START)&&!(fault_va<=USTACKTOP) )  //check is on stack or heap
						{
							//illegal access
							sched_kill_env(curenv->env_id);
							return;
						}
				   }
				   struct WorkingSetElement *victim=LIST_LAST(&(curenv->SecondList));
				   uint32 page_permission_for_victim = pt_get_page_permissions(curenv->env_page_directory , victim->virtual_address);
				   if (page_permission_for_victim & PERM_MODIFIED)
					{
							uint32 *ptr_page_table ;
							get_page_table(curenv->env_page_directory , victim->virtual_address, &ptr_page_table);
							struct FrameInfo *ptr_frame_info = get_frame_info(curenv->env_page_directory , victim->virtual_address , &ptr_page_table);
							int retrn = pf_update_env_page(curenv , victim->virtual_address ,ptr_frame_info);
							if (retrn != 0 )
							{
									panic("Error no memory to write on disk ........");
							}

					}

					uint32 *ptr_page_table ;
					get_page_table(curenv->env_page_directory , victim->virtual_address, &ptr_page_table);
					struct FrameInfo *ptr_frame_info = get_frame_info(curenv->env_page_directory , victim->virtual_address , &ptr_page_table);
					unmap_frame(curenv->env_page_directory,victim->virtual_address);
				   LIST_REMOVE(&(curenv->SecondList) ,victim);
				   struct WorkingSetElement *switchedElement=LIST_LAST(&(curenv->ActiveList));
				   LIST_REMOVE(&(curenv->ActiveList),switchedElement);
				   pt_set_page_permissions(curenv->env_page_directory,fault_va,PERM_PRESENT, 0);
				   struct WorkingSetElement *faulted_page=env_page_ws_list_create_element(curenv,ROUNDDOWN(fault_va, PAGE_SIZE));
				  LIST_INSERT_HEAD(&(curenv->ActiveList),faulted_page);
				  pt_set_page_permissions(curenv->env_page_directory,switchedElement->virtual_address,0,PERM_PRESENT);
				  LIST_INSERT_HEAD(&(curenv->SecondList), switchedElement);
		   }					   }

	}

}


void __page_fault_handler_with_buffering(struct Env * curenv, uint32 fault_va)
{
	panic("this function is not required...!!");
}



