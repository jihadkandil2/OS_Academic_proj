#include "kheap.h"

#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include "memory_manager.h"


int initialize_kheap_dynamic_allocator(uint32 daStart, uint32 initSizeToAllocate, uint32 daLimit)
{
	// 1- initialize 3 variables
	// 2- all pages in given range should be allocated and mapped
	// 3- remember call intialize MS1

	kernel_Heap_start = ROUNDDOWN(daStart,PAGE_SIZE) ;
	uint32 size = initSizeToAllocate;
	segmentBreak = ROUNDDOWN(kernel_Heap_start + size , PAGE_SIZE) ;
	hardLimit = ROUNDDOWN(daLimit ,PAGE_SIZE);

	struct FrameInfo *ptr_frame_info ;

	if ( segmentBreak <= hardLimit)
	{
		for (uint32 i = kernel_Heap_start ; i < segmentBreak ; i = i + PAGE_SIZE)
		{
			allocate_frame(&ptr_frame_info);
			map_frame(ptr_page_directory , ptr_frame_info , (uint32)i ,PERM_PRESENT|PERM_WRITEABLE );
			ptr_frame_info->va = (uint32)i ;
		}
		initialize_dynamic_allocator( kernel_Heap_start , size);
		return 0 ;
	}
	else // u exceded hard limit or you want to access size > 2KB which is not block allocator
	{
		return E_NO_MEM ;
	}
}

#define true 1
#define false 0
void *sbrk(int increment)
{
	// TODO: [PROJECT'23.MS2 - #02] [1] KERNEL HEAP - sbrk()

	uint32 old_break = segmentBreak;
	uint32 new_break;
	if (increment == 0)
	{
		segmentBreak = old_break;
		return (void *)old_break;
	}
	if (increment > 0)
	{
		// cprintf("i am in sbrk inc \n");
		uint32 size_new_break = (uint32)increment;
		new_break = ROUNDUP(old_break + size_new_break, PAGE_SIZE);
		uint32 virtual_address = old_break + PAGE_SIZE;
		bool alloc = 0;

		if (new_break <= hardLimit)
		{
			if (((segmentBreak + increment) & 0x00FFF000) != ((segmentBreak - 1) & 0x00FFF000))
			{
				uint32 *ptr;
				struct FrameInfo *ptr_frame_info = get_frame_info(ptr_page_directory, old_break, &ptr);
				if (ptr_frame_info == 0 && old_break % PAGE_SIZE == 0)
				{
					struct FrameInfo *pointer;
					if (allocate_frame(&pointer) == 0)
					{
						map_frame(ptr_page_directory, pointer, old_break, PERM_USED | PERM_WRITEABLE);
						pointer->va = old_break;
						alloc = 1;
					}
				}
				if (alloc == 1)
				{
					new_break = new_break - PAGE_SIZE;
				}
				// new_break += PAGE_SIZE;
				for (uint32 i = old_break + PAGE_SIZE; i < new_break; i += PAGE_SIZE)
				{
					struct FrameInfo *allcoate_pages;
					int ret = allocate_frame(&allcoate_pages);
					if (ret != E_NO_MEM)
					{
						allcoate_pages->va = virtual_address;
						map_frame(ptr_page_directory, allcoate_pages, virtual_address, PERM_USED | PERM_WRITEABLE);
						virtual_address = virtual_address + PAGE_SIZE;
						// cprintf("LLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLL\n\n");
					}
					else
					{
						panic("No Memory");
					}
				}
			}
			segmentBreak = ROUNDUP(old_break + size_new_break, PAGE_SIZE);
			//			cprintf("updated\n\n");
			return (void *)old_break;
		}
		else
		{
			panic("Kernel Crashed hard limit exceeded");
		}
	}
	else // increment < 0
	{

		uint32 size_new_break = (uint32)increment;
		uint32 old_break = segmentBreak - 1;
		 new_break = old_break + size_new_break; // check on new break !!!!!

		uint32 round_new_break = new_break;
		uint32 virtual_address = old_break-1;
		if (segmentBreak + increment >= KERNEL_HEAP_START)
		{
			if (((segmentBreak + increment) & 0x00FFF000) != ((segmentBreak - 1) & 0x00FFF000))
			{
				for (uint32 i = 0; i < ROUNDUP(-increment, PAGE_SIZE) / PAGE_SIZE; i++)
				{
					unmap_frame(ptr_page_directory, virtual_address);
					virtual_address = virtual_address - PAGE_SIZE;
					if (virtual_address - (segmentBreak + increment) < PAGE_SIZE)
					{
						break;
					}
				}
			}
		}
		segmentBreak += increment;
		// cprintf("updated\n\n");
		return (void *)segmentBreak;
	}
}
// old sbrk tests 100 %
/*
void* sbrk(int increment)
{
	//TODO: [PROJECT'23.MS2 - #02] [1] KERNEL HEAP - sbrk()
	struct FrameInfo *ptr_frame_info ;
	uint32 old_break = segmentBreak;
	uint32 new_break ;
	if (increment > 0)
	{
	//	cprintf("i am in sbrk inc \n");
		uint32 size_new_break = (uint32)increment;
		new_break = ROUNDUP(old_break + size_new_break,PAGE_SIZE);
       // cprintf("INCREMENT > 000000000000\n\n");
		if(new_break <= hardLimit)
		{
			uint32 virtual_address = old_break;

				while(virtual_address < new_break)
				{
					int ret  = allocate_frame(&ptr_frame_info);

						if (ret != E_NO_MEM)
						{
							ptr_frame_info->va = virtual_address;
							map_frame(ptr_page_directory,ptr_frame_info,virtual_address,PERM_PRESENT | PERM_WRITEABLE);
							virtual_address = virtual_address + PAGE_SIZE;

						}
						else //failed to allocate
						{
							panic("Memory is full");
						}
				}
			segmentBreak = new_break;
			return (void*)old_break;
		}
		else
		{
			panic("Kernel Crashed hard limit exceeded");
		}
	}
	else if (increment < 0)
	{

		uint32 size_new_break = (uint32)increment;
		new_break = old_break + size_new_break;    //check on new break !!!!!

		uint32 round_new_break = new_break;
		uint32 virtual_address = new_break;
		 // cprintf("INCREMENT < 000000000000\n\n");
		for(uint32 i = ROUNDUP(round_new_break ,PAGE_SIZE) ; i < old_break ; i+= PAGE_SIZE)
		{
			free_frame(ptr_frame_info);
			unmap_frame(ptr_page_directory,virtual_address);
			pt_set_page_permissions(ptr_page_directory, virtual_address, 0 , PERM_PRESENT);
			virtual_address = virtual_address + PAGE_SIZE;
			//cprintf("sssssssssssssssssssssssssss\n\n");

		}
		segmentBreak = new_break;
		return (void*)new_break;

	}
	else // increment = 0
	{
	  // cprintf("INCREMENT == 000000000000\n\n");
	   segmentBreak = old_break;
	   return (void*)old_break;
	}
}

*/
struct pageAllocate myArrayOfPages[9024];
uint32 arrIndx =0 ;
void* kmalloc(unsigned int size)
{
	//TODO: [PROJECT'23.MS2 - #03] [1] KERNEL HEAP - kmalloc()
	uint32 RoundedUpSize = ROUNDUP(size, PAGE_SIZE);   // عشان الوكيت بيدجات كاملة
	struct FrameInfo *ptr_frame_info;
	startPageAllocator = (uint32 )(hardLimit + PAGE_SIZE) ;
	EndPageAllocator = (uint32) KERNEL_HEAP_MAX ;
	NumberOfFreePages= (uint32)  ((uint32) (EndPageAllocator -startPageAllocator) /(uint32) PAGE_SIZE) ;




	if (isKHeapPlacementStrategyFIRSTFIT()) // لو  اشتغل فيرست فيت
	{
		if (size <= DYN_ALLOC_MAX_BLOCK_SIZE) // هل هالوكيت بلوكاية
		{
			return alloc_block_FF(size);
		}
		else     // هنالوكيت بيدجاية
		{
			uint32 reqPagesNo = (uint32)(RoundedUpSize / (uint32)PAGE_SIZE) ;
            uint8 fixVa = 0;

			if (reqPagesNo <= NumberOfFreePages) //    عندي بيدجات فاضية مكفية اقدر الوكيت عادي
			{
				uint32 count_contigous_pages = 0 ;
				uint8 found = 0;
				uint32 targetVA =(uint32)NULL ;

				for (uint32 i = startPageAllocator ;  i < (uint32)KERNEL_HEAP_MAX ; i = i + (uint32)PAGE_SIZE)
				{
					uint32 pagePermission = pt_get_page_permissions(ptr_page_directory ,i);

					if (pagePermission & PERM_PRESENT)
					{
						count_contigous_pages= 0;
						targetVA = (uint32)NULL ;
						fixVa =0 ;
					}
					else  // سعتها هحجز مجموعه من البيدجز
					{
						count_contigous_pages++ ;
						if (fixVa == 0)
						{
							targetVA = i ;
							fixVa =1 ;   // هثبت البداية دي بقي طول ما انا بلاقي فري بيدجز
						}
					}
					if (count_contigous_pages == reqPagesNo)
					{
						found =1 ;
						break;
					}
				}
				if (found == 1 )  //  هنالوكيت بقي في هذه المساحه ونماب وكمان هنعدل معلومات هذه البدجات
				{
					myArrayOfPages[arrIndx].noOfPages = reqPagesNo ;
					myArrayOfPages[arrIndx].startVaddress = targetVA ;
					arrIndx++;

					for (uint32 indx = targetVA ; indx < (targetVA + PAGE_SIZE * reqPagesNo) ; indx = indx + PAGE_SIZE )
					{

						allocate_frame(&ptr_frame_info);
						uint32 va = indx ;
						map_frame(ptr_page_directory , ptr_frame_info , (uint32)va ,PERM_PRESENT |PERM_WRITEABLE);
						ptr_frame_info->va = indx ;
					}

					NumberOfFreePages =NumberOfFreePages - reqPagesNo ;
					return (void *)targetVA;
				}
				else //اذن هوا ملقاش مساحه بدجات ورا بعض
				{
					return NULL ;
				}
			}
			else // مفيش فري بيدجز مكفية اصلا
			{
				return NULL ;
			}
		}
	}
	else  // using best fit
	{
      cprintf(" BEST FIT NOT Implemented YET") ;
      return NULL ;
	}
	//return NULL ;
}

void kfree(void* virtual_address)
{
	//TODO: [PROJECT'23.MS2 - #04] [1] KERNEL HEAP - kfree()

	uint32 *pagetable_ptrr=NULL;
	uint8 is_found=0;
	uint32 va= (uint32)virtual_address ;

	if((va >= kernel_Heap_start) && (va < hardLimit )) //block allocator
	{
		 free_block((void *)va);
	}
	else if((va >= startPageAllocator)&& (va < EndPageAllocator)) //page allocator
	{
		uint32 tarNumOfPages ;
		for (uint32 i = 0 ; i < 5024 ; i++)
		{
			if ( myArrayOfPages[i].startVaddress == va)
			{
				is_found =1 ;
				tarNumOfPages = myArrayOfPages[i].noOfPages;
				myArrayOfPages[i].noOfPages =0 ;
				myArrayOfPages[i].startVaddress = (uint32) NULL ;
				break;
			}
		}
		if (is_found == 1) //لقيت ال عنوان بيعبر عن بيدج فعلا
		{
          // هندخل علي الفيرشوال دا نشوف لو البيدج مش فري نعملها فري ونخش علي الي بعدها
			for (uint32 i = va ; i < (va + PAGE_SIZE * tarNumOfPages); i+=PAGE_SIZE)
			{
					get_page_table(ptr_page_directory , i , &pagetable_ptrr);
					struct FrameInfo* frame_info_ptrr=get_frame_info(ptr_page_directory,i,&pagetable_ptrr);
					if(frame_info_ptrr != 0)
					{
						free_frame(frame_info_ptrr);
						unmap_frame(ptr_page_directory,va);
						pt_set_page_permissions(ptr_page_directory, i, 0 , PERM_PRESENT);
					}
			}

			NumberOfFreePages = NumberOfFreePages + tarNumOfPages ;
		}
	    else // لم اجد هذا العنوان
	    {
		    panic("kfree() didnot find the Address...!!");
	    }
	 }
	 else   // عنوان غير صالح يا اخ
	 {
		 panic("kfree() didnot find the Address is invalid...!!");
	 }
}

unsigned int kheap_virtual_address(unsigned int physical_address)
{
	//TODO: [PROJECT'23.MS2 - #05] [1] KERNEL HEAP - kheap_virtual_address()
	//refer to the project presentation and documentation for details
	// Write your code here, remove the panic and write your code

	uint32 offset=physical_address & 0xFFF;
	struct FrameInfo *vir_frame_info=to_frame_info(physical_address);
	if(vir_frame_info->va != 0)
	{
	    return vir_frame_info->va + offset;
	}
	else
	{
		return 0;
	}
}

unsigned int kheap_physical_address(unsigned int virtual_address)
{
	//TODO: [PROJECT'23.MS2 - #06] [1] KERNEL HEAP - kheap_physical_address()
	//refer to the project presentation and documentation for details
	// Write your code here, remove the panic and write your code
	uint32 offset=virtual_address & 0xFFF;
		uint32 *page_table_ptr=NULL;
		struct FrameInfo* phy_frame_info= get_frame_info(ptr_page_directory,virtual_address,&page_table_ptr);
		if(phy_frame_info!= NULL){
		uint32 phy_address = to_physical_address(phy_frame_info);
		return phy_address+offset;
		}
		else
			return 0;
		//panic("kheap_physical_address() is not implemented yet...!!");
		//change this "return" according to your answer
}

void kfreeall()
{
	panic("Not implemented!");

}

void kshrink(uint32 newSize)
{
	panic("Not implemented!");
}

void kexpand(uint32 newSize)
{
	panic("Not implemented!");
}




//=================================================================================//
//============================== BONUS FUNCTION ===================================//
//=================================================================================//
// krealloc():

//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to kmalloc().
//	A call with new_size = zero is equivalent to kfree().

void *krealloc(void *virtual_address, uint32 new_size)
{
	//TODO: [PROJECT'23.MS2 - BONUS#1] [1] KERNEL HEAP - krealloc()
	// Write your code here, remove the panic and write your code
	return NULL;
	panic("krealloc() is not implemented yet...!!");
}
