#include <rdix/memory.h>
#include <common/assert.h>

extern size_t mem_size;
extern bitmap_t v_bit_map;

vir_addr_t start_io_memory;

/* 设备 IO 映射专用
 * 将 n 个物理页映射到内核 IO 空间 
 * 有 bug 可能跨多个页表 */
vir_addr_t link_nppage(phy_addr_t addr, size_t size){
    //assert((u32)addr > mem_size);
    assert(start_io_memory >= IO_MEM_START && (start_io_memory + size) <= KERNEL_MEMERY_SIZE);

    vir_addr_t res = start_io_memory;
    page_entry_t *pte = get_pte(start_io_memory, false);
    page_entry_t *entry = &pte[TIDX((u32)start_io_memory)];
    page_idx_t pg_num = PAGE_IDX(size + PAGE_SIZE - 1);

    start_io_memory += pg_num * PAGE_SIZE;

    for (int i = 0; i < pg_num; ++i){
        entry_init(&entry[i], PAGE_IDX((u32)addr) + i);
    }

    return res;
}

