#include <linux/swap_stats.h>
#include <linux/delay.h>
#include "rswap_dram.h"
#include "constants.h"

struct timer_list canvas_info_printer;

atomic64_t num_swapout_pages = ATOMIC64_INIT(0);
atomic64_t num_swapin_pages = ATOMIC64_INIT(0);


uint64_t prev_num_swapout_pages = 0;
uint64_t prev_num_swapin_pages = 0;

static void *local_dram; // a buffer created via vzalloc
static uint64_t local_mem_size; // local DRAM size in GB

int rswap_dram_write(struct page *page, size_t roffset)
{
	void *page_vaddr;

	page_vaddr = kmap_atomic(page);
	copy_page((void *)(local_dram + roffset), page_vaddr);
	kunmap_atomic(page_vaddr);
	udelay(2);
	atomic64_inc(&num_swapout_pages);

	return 0;
}

int rswap_dram_read(struct page *page, size_t roffset)
{
	void *page_vaddr;

	VM_BUG_ON_PAGE(!PageSwapCache(page), page);
	VM_BUG_ON_PAGE(!PageLocked(page), page);
	VM_BUG_ON_PAGE(PageUptodate(page), page);

	page_vaddr = kmap_atomic(page);
	copy_page(page_vaddr, (void *)(local_dram + roffset));
	kunmap_atomic(page_vaddr);
	udelay(2);
	atomic64_inc(&num_swapin_pages);

	SetPageUptodate(page);
	unlock_page(page);
	return 0;
}

void canvas_info_print_callback(struct timer_list *timer) {
	uint64_t num_reserved_entries_tmp;
	uint64_t num_swapout_pages_tmp = atomic64_read(&num_swapout_pages);
  	uint64_t num_swapin_pages_tmp = atomic64_read(&num_swapin_pages);
  	int swapout_bw = (num_swapout_pages_tmp - prev_num_swapout_pages) / 1000;
  	int swapin_bw = (num_swapin_pages_tmp - prev_num_swapin_pages) / 1000;
  	prev_num_swapout_pages = num_swapout_pages_tmp;
  	prev_num_swapin_pages = num_swapin_pages_tmp;

	num_reserved_entries_tmp = atomic64_read(&num_reserved_entries);
	pr_info("Num of current reserved entries is: %lld\n", num_reserved_entries_tmp);
  	pr_info("swapout bw = %d Kops, swapin bw = %d Kops", swapout_bw, swapin_bw);
  	mod_timer(timer, jiffies + msecs_to_jiffies(INFO_PRINT_TINTERVAL));
}

int rswap_init_local_dram(int _mem_size)
{
	local_mem_size = (uint64_t)_mem_size * ONE_GB;
	local_dram = vzalloc(local_mem_size);
	pr_info("Allocate local dram 0x%llx bytes for debug\n", local_mem_size);
	
	timer_setup(&canvas_info_printer, canvas_info_print_callback, 0);
  	mod_timer(&canvas_info_printer, jiffies + msecs_to_jiffies(INFO_PRINT_TINTERVAL));

	return 0;
}

int rswap_remove_local_dram(void)
{
	vfree(local_dram);
	pr_info("Free the allocated local_dram 0x%llx bytes \n",
		local_mem_size);
	return 0;
}
