#include <defs.h>
#include <x86.h>
#include <elf.h>

/* *********************************************************************
 * This a dirt simple boot loader, whose sole job is to boot
 * an ELF kernel image from the first IDE hard disk.
 *
 * DISK LAYOUT
 *  * This program(bootasm.S and bootmain.c) is the bootloader.
 *    It should be stored in the first sector of the disk.
 *	     �������(bootasm.S and bootmain.c)����������������Ӧ�ñ������ڴ��̵ĵ�һ������
 *
 *  * The 2nd sector onward holds the kernel image.
 *	     �ڶ����������󱣴����ں�ӳ��
 *
 *  * The kernel image must be in ELF format.
 *	     �ں�ӳ����������ELF��ʽ��
 *
 * BOOT UP STEPS
 *  * when the CPU boots it loads the BIOS into memory and executes it
 *
 *  * the BIOS intializes devices, sets of the interrupt routines, and
 *    reads the first sector of the boot device(e.g., hard-drive)
 *    into memory and jumps to it.
 *
 *  * Assuming this boot loader is stored in the first sector of the
 *    hard-drive, this code takes over...
 *
 *  * control starts in bootasm.S -- which sets up protected mode,
 *    and a stack so C code then run, then calls bootmain()
 *
 *  * bootmain() in this file takes over, reads in the kernel and jumps to it.
 * */

#define SECTSIZE        512
#define ELFHDR          ((struct elfhdr *)0x10000)      // scratch space

/* waitdisk - wait for disk ready */
static void
waitdisk(void) {
	// �����ݣ���0x1f7��Ϊæ״̬ʱ�����Զ�
    while ((inb(0x1F7) & 0xC0) != 0x40)
        /* do nothing */;
}

/* readsect - read a single sector at @secno into @dst */
// ��ȡһ������������(��@secnoָ��)��@dstָ��ָ����ڴ���
static void
readsect(void *dst, uint32_t secno) {
	// https://chyyuu.gitbooks.io/ucore_os_docs/content/lab1/lab1_3_2_3_dist_accessing.html
	// ʵ��ָ����lab1�еĶ�ideӲ�̵ķ���������ϸ����

    // wait for disk to be ready
    waitdisk();

    // ���̶�ȡ��������
    outb(0x1F2, 1);                         // count = 1
    outb(0x1F3, secno & 0xFF);
    outb(0x1F4, (secno >> 8) & 0xFF);
    outb(0x1F5, (secno >> 16) & 0xFF);
    outb(0x1F6, ((secno >> 24) & 0xF) | 0xE0);
    outb(0x1F7, 0x20);                      // cmd 0x20 - read sectors

    // wait for disk to be ready
    waitdisk();

    // read a sector
    insl(0x1F0, dst, SECTSIZE / 4);
}

/* *
 * readseg - read @count bytes at @offset from kernel into virtual address @va,
 * might copy more than asked.
 * */
static void
readseg(uintptr_t va, uint32_t count, uint32_t offset) {
    uintptr_t end_va = va + count;

    // round down to sector boundary
    va -= offset % SECTSIZE;

    // translate from bytes to sectors; kernel starts at sector 1
    // �������Ҫ��ȡ�Ĵ��������ţ����ڵ�1��������bootloaderռ�ݣ�kernel�ں˴ӵڶ���������ʼ(�±�Ϊ1)��������������Ҫ����1
    uint32_t secno = (offset / SECTSIZE) + 1;

    // If this is too slow, we could read lots of sectors at a time.
    // We'd write more to memory than asked, but it doesn't matter --
    // we load in increasing order.
    // ѭ��������ͨ��vaָ���������һ��һ��������ѭ����ȡ����д��vaָ����ڴ�����
    for (; va < end_va; va += SECTSIZE, secno ++) {
        readsect((void *)va, secno);
    }
}

/* bootmain - the entry of bootloader */
void
bootmain(void) {
    // read the 1st page off disk
	// ��Ӳ���ж�ȡ���ں��ļ�ELF�ļ�ͷ���ݣ�����ELFHDRָ��ָ����ڴ����� (��СΪ8������)
    readseg((uintptr_t)ELFHDR, SECTSIZE * 8, 0);

    // is this a valid ELF? У���ȡ������ELF�ļ�ͷ��ħ��ֵ�Ƿ���ȷ
    if (ELFHDR->e_magic != ELF_MAGIC) {
        goto bad;
    }

    struct proghdr *ph, *eph;

    // load each program segment (ignores ph flags)
    ph = (struct proghdr *)((uintptr_t)ELFHDR + ELFHDR->e_phoff);
    eph = ph + ELFHDR->e_phnum;
    for (; ph < eph; ph ++) {
    	// ѭ������������������ε����ݶ�ȡ��ָ�����ڴ�λ��(ph->p_va)
        readseg(ph->p_va & 0xFFFFFF, ph->p_memsz, ph->p_offset);
    }

    // call the entry point from the ELF header
    // note: does not return
    // ͨ������ָ��ķ�ʽ����ת��ELFHDR->e_entryָ���ĳ����ʼִ�����(���ں����)
    // ��makefile�������У�ELFHDR->e_entryָ�����kern/init/init.c�е�kern_init���� (kernel.ld�е�ENTRY(kern_init))
    ((void (*)(void))(ELFHDR->e_entry & 0xFFFFFF))();

bad:
	// ��ת���ں�֮�󣬲�Ӧ�÷���
    outw(0x8A00, 0x8A00);
    outw(0x8A00, 0x8E00);

    /* do nothing */
    while (1);
}

