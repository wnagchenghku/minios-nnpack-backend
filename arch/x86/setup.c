/******************************************************************************
 * common.c
 * 
 * Common stuff special to x86 goes here.
 * 
 * Copyright (c) 2002-2003, K A Fraser & R Neugebauer
 * Copyright (c) 2005, Grzegorz Milos, Intel Research Cambridge
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 *
 */

#include <mini-os/os.h>
#include <mini-os/lib.h> /* for printk, memcpy */
#include <mini-os/kernel.h>
#include <xen/xen.h>

#define CPU_FEATURE_FPU			(1 << 0)
#define CPU_FEATURE_PSE			(1 << 3)
#define CPU_FEATURE_MSR			(1 << 5)
#define CPU_FEATURE_PAE			(1 << 6)
#define CPU_FEATURE_MCE			(1 << 7)
#define CPU_FEATURE_APIC		(1 << 9)
#define CPU_FEATURE_SEP			(1 << 11)
#define CPU_FEATURE_PGE			(1 << 13)
#define CPU_FEATURE_PAT			(1 << 16)
#define CPU_FEATURE_PSE36		(1 << 17)
#define CPU_FEATURE_CLFLUSH		(1 << 19)
#define CPU_FEATURE_MMX			(1 << 23)
#define CPU_FEATURE_FXSR		(1 << 24)
#define CPU_FEATURE_SSE			(1 << 25)
#define CPU_FEATURE_SSE2		(1 << 26)

// feature list 0x00000001 (ecx)
#define CPU_FEATURE_MWAIT			(1 << 3)
#define CPU_FEATURE_VMX				(1 << 5)
#define CPU_FEATURE_EST				(1 << 7)
#define CPU_FEATURE_SSE3			(1 << 9)
#define CPU_FEATURE_FMA				(1 << 12)
#define CPU_FEATURE_DCA				(1 << 18)
#define CPU_FEATURE_SSE4_1			(1 << 19)
#define CPU_FEATURE_SSE4_2			(1 << 20)
#define CPU_FEATURE_X2APIC			(1 << 21)
#define CPU_FEATURE_MOVBE			(1 << 22)
#define CPU_FEATURE_XSAVE			(1 << 26)
#define CPU_FEATURE_OSXSAVE			(1 << 27)
#define CPU_FEATURE_AVX				(1 << 28)
#define CPU_FEATURE_RDRAND			(1 << 30)
#define CPU_FEATURE_HYPERVISOR			(1 << 31)

// CPUID.80000001H:EDX feature list
#define CPU_FEATURE_SYSCALL			(1 << 11)
#define CPU_FEATURE_NX				(1 << 20)
#define CPU_FEATURE_1GBHP			(1 << 26)
#define CPU_FEATURE_RDTSCP			(1 << 27)
#define CPU_FEATURE_LM				(1 << 29)

// feature list 0x00000007:0
#define CPU_FEATURE_FSGSBASE			(1 << 0)
#define CPU_FEATURE_TSC_ADJUST			(1 << 1)
#define CPU_FEATURE_SGX			(1 << 2)
#define CPU_FEATURE_BMI1			(1 << 3)
#define CPU_FEATURE_HLE				(1 << 4)
#define CPU_FEATURE_AVX2			(1 << 5)
#define CPU_FEATURE_SMEP			(1 << 7)
#define CPU_FEATURE_BMI2			(1 << 8)
#define CPU_FEATURE_ERMS			(1 << 9)
#define CPU_FEATURE_INVPCID			(1 << 10)
#define CPU_FEATURE_RTM				(1 << 11)
#define CPU_FEATURE_CQM				(1 << 12)
#define CPU_FEATURE_MPX				(1 << 14)
#define CPU_FEATURE_AVX512F			(1 << 16)
#define CPU_FEATURE_RDSEED			(1 << 18)
#define CPU_FEATURE_ADX				(1 << 19)
#define CPU_FEATURE_SMAP			(1 << 20)
#define CPU_FEATURE_PCOMMIT			(1 << 22)
#define CPU_FEATURE_CLFLUSHOPT			(1 << 23)
#define CPU_FEATURE_CLWB			(1 << 24)
#define CPU_FEATURE_AVX512PF			(1 << 26)
#define CPU_FEATURE_AVX512ER			(1 << 27)
#define CPU_FEATURE_AVX512CD			(1 << 28)
#define CPU_FEATURE_SHA_NI			(1 << 29)
#define CPU_FEATURE_AVX512BW		(1 << 30)
#define CPU_FEATURE_AVX512VL		(1 <<31)

// feature list 0x00000006
#define CPU_FEATURE_IDA				(1 << 1)
#define CPU_FEATURE_ARAT				(1 << 2)
#define CPU_FEATURE_EPB				(1 << 3)
#define CPU_FEATURE_HWP				(1 << 7)
#define CPU_FEATURE_HWP_EPP				(1 << 10)

typedef struct {
	uint32_t feature1, feature2;
	uint32_t feature3, feature4;
	uint32_t addr_width;
} cpu_info_t;

cpu_info_t cpu_info = { 0, 0, 0, 0, 0};
static char cpu_vendor[13] = {[0 ... 12] = 0};

inline static uint32_t has_fpu(void) {
	return (cpu_info.feature1 & CPU_FEATURE_FPU);
}

inline static uint32_t has_sse(void) {
	return (cpu_info.feature1 & CPU_FEATURE_SSE);
}

inline static uint32_t has_sse2(void) {
	return (cpu_info.feature1 & CPU_FEATURE_SSE2);
}

inline static uint32_t has_sse3(void) {
	return (cpu_info.feature2 & CPU_FEATURE_SSE3);
}

inline static uint32_t has_sse4_1(void) {
	return (cpu_info.feature2 & CPU_FEATURE_SSE4_1);
}

inline static uint32_t has_sse4_2(void) {
	return (cpu_info.feature2 & CPU_FEATURE_SSE4_2);
}

inline static uint32_t has_xsave(void) {
	return (cpu_info.feature2 & CPU_FEATURE_XSAVE);
}

inline static uint32_t has_osxsave(void) {
	return (cpu_info.feature2 & CPU_FEATURE_OSXSAVE);
}

inline static uint32_t has_avx(void) {
	return (cpu_info.feature2 & CPU_FEATURE_AVX);
}

inline static uint32_t has_fxsr(void) {
	return (cpu_info.feature1 & CPU_FEATURE_FXSR);
}

inline static uint32_t has_fsgsbase(void) {
	return (cpu_info.feature4 & CPU_FEATURE_FSGSBASE);
}

// x86 control registers

/// Protected Mode Enable
#define CR0_PE					(1 << 0)
/// Monitor coprocessor
#define CR0_MP					(1 << 1)
/// Enable FPU emulation
#define CR0_EM					(1 << 2)
/// Task switched
#define CR0_TS					(1 << 3)
/// Extension type of coprocessor
#define CR0_ET					(1 << 4)
/// Enable FPU error reporting
#define CR0_NE					(1 << 5)
/// Enable write protected pages
#define CR0_WP					(1 << 16)
/// Enable alignment checks
#define CR0_AM					(1 << 18)
/// Globally enables/disable write-back caching
#define CR0_NW					(1 << 29)
/// Globally disable memory caching
#define CR0_CD					(1 << 30)
/// Enable paging
#define CR0_PG					(1 << 31)

/// Virtual 8086 Mode Extensions
#define CR4_VME					(1 << 0)
/// Protected-mode Virtual Interrupts
#define CR4_PVI					(1 << 1)
/// Disable Time Stamp Counter register (rdtsc instruction)
#define CR4_TSD					(1 << 2)
/// Enable debug extensions
#define CR4_DE					(1 << 3)
///  Enable hugepage support
#define CR4_PSE					(1 << 4)
/// Enable physical address extension
#define CR4_PAE					(1 << 5)
/// Enable machine check exceptions
#define CR4_MCE					(1 << 6)
/// Enable global pages
#define CR4_PGE					(1 << 7)
/// Enable Performance-Monitoring Counter
#define CR4_PCE					(1 << 8)
/// Enable Operating system support for FXSAVE and FXRSTOR instructions
#define CR4_OSFXSR				(1 << 9)
/// Enable Operating System Support for Unmasked SIMD Floating-Point Exceptions
#define CR4_OSXMMEXCPT			(1 << 10)
/// Enable Virtual Machine Extensions, see Intel VT-x
#define CR4_VMXE				(1 << 13)
/// Enable Safer Mode Extensions, see Trusted Execution Technology (TXT)
#define CR4_SMXE				(1 << 14)
/// Enables the instructions RDFSBASE, RDGSBASE, WRFSBASE, and WRGSBASE
#define CR4_FSGSBASE				(1 << 16)
/// Enables process-context identifiers
#define CR4_PCIDE				(1 << 17)
/// Enable XSAVE and Processor Extended States
#define CR4_OSXSAVE				(1 << 18)
/// Enable Supervisor Mode Execution Protection
#define CR4_SMEP				(1 << 20)
/// Enable Supervisor Mode Access Protection
#define CR4_SMAP				(1 << 21)

inline static uint32_t has_pge(void)
{
	return (cpu_info.feature1 & CPU_FEATURE_PGE);
}

/** @brief Read cr0 register
 * @return cr0's value
 */
static inline size_t read_cr0(void) {
	size_t val;
	asm volatile("mov %%cr0, %0" : "=r"(val) :: "memory");
	return val;
}

/** @brief Write a value into cr0 register
 * @param val The value you want to write into cr0
 */
static inline void write_cr0(size_t val) {
	asm volatile("mov %0, %%cr0" :: "r"(val) : "memory");
}


/** @brief Read cr4 register
 * @return cr4's value
 */
static inline size_t read_cr4(void) {
	size_t val;
	asm volatile("mov %%cr4, %0" : "=r"(val) :: "memory");
	return val;
}

/** @brief Write a value into cr4 register
 * @param val The value you want to write into cr4
 */
static inline void write_cr4(size_t val) {
	asm volatile("mov %0, %%cr4" :: "r"(val) : "memory");
}

/** @brief Get Extended Control Register
 *
 * Reads the contents of the extended control register (XCR) specified
 * in the ECX register.
 */
static inline uint64_t xgetbv(uint32_t index)
{
	uint32_t edx, eax;

	asm volatile ("xgetbv" : "=a"(eax), "=d"(edx) : "c"(index));

	return (uint64_t) eax | ((uint64_t) edx << 32ULL);
}

/** @brief Set Extended Control Register
 *
 * Writes a 64-bit value into the extended control register (XCR) specified
 * in the ECX register.
 */
static inline void xsetbv(uint32_t index, uint64_t value)
{
	uint32_t edx, eax;

	edx = (uint32_t) (value >> 32ULL);
	eax = (uint32_t) value;

	asm volatile ("xsetbv" :: "a"(eax), "c"(index), "d"(edx));
}

inline static void cpuid(uint32_t code, uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d) {
	asm volatile ("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "0"(code), "2"(*c));
}

inline static uint32_t has_mce(void) {
	return (cpu_info.feature1 & CPU_FEATURE_MCE);
}

int cpu_detection(void) {
	uint64_t xcr0;
	uint32_t a=0, b=0, c=0, d=0, level = 0, extended = 0;
	uint32_t family, model, stepping;
	size_t cr0, cr4;

	if (!cpu_info.feature1) {
		cpuid(0, &level, (uint32_t*) cpu_vendor, (uint32_t*)(cpu_vendor+8), (uint32_t*)(cpu_vendor+4));

		cpuid(1, &a, &b, &cpu_info.feature2, &cpu_info.feature1);

		family   = (a & 0x00000F00) >> 8;
		model    = (a & 0x000000F0) >> 4;
		stepping =  a & 0x0000000F;
		if ((family == 6) && (model < 3) && (stepping < 3))
			cpu_info.feature1 &= ~CPU_FEATURE_SEP;

		cpuid(0x80000000, &extended, &b, &c, &d);
		if (extended >= 0x80000001)
			cpuid(0x80000001, &a, &b, &c, &cpu_info.feature3);

		/* Additional Intel-defined flags: level 0x00000007 */
		if (level >= 0x00000007) {
			a = b = c = d = 0;
			cpuid(7, &a, &cpu_info.feature4, &c, &d);
		}
	}

	cr0 = read_cr0();
	cr0 |= CR0_AM;
	cr0 |= CR0_NE;
	cr0 |= CR0_MP;
	cr0 &= ~(CR0_CD|CR0_NW);
	write_cr0(cr0);

	cr4 = read_cr4();
	if (has_fxsr())
		cr4 |= CR4_OSFXSR;	// set the OSFXSR bit
	if (has_sse())
		cr4 |= CR4_OSXMMEXCPT;	// set the OSXMMEXCPT bit
	if (has_xsave())
		cr4 |= CR4_OSXSAVE;
	if (has_pge())
		cr4 |= CR4_PGE;
	if (has_fsgsbase())
		cr4 |= CR4_FSGSBASE;
	if (has_mce())
		cr4 |= CR4_MCE;		// enable machine check exceptions
	//if (has_vmx())
	//	cr4 |= CR4_VMXE;
	cr4 &= ~(CR4_PCE|CR4_TSD);	// disable performance monitoring counter
								// clear TSD => every privilege level is able
								// to use rdtsc
	write_cr4(cr4);

	if (has_xsave())
	{
		xcr0 = xgetbv(0);
		if (has_fpu())
			xcr0 |= 0x1;
		if (has_sse())
			xcr0 |= 0x2;
		if (has_avx())
			xcr0 |= 0x4;
		xsetbv(0, xcr0);
	}

	if (has_fpu()) {
		asm volatile ("fninit");
	}

	return 0;
}

/*
 * Shared page for communicating with the hypervisor.
 * Events flags go here, for example.
 */
shared_info_t *HYPERVISOR_shared_info;

/*
 * This structure contains start-of-day info, such as pagetable base pointer,
 * address of the shared_info structure, and things like that.
 */
union start_info_union start_info_union;

/*
 * Just allocate the kernel stack here. SS:ESP is set up to point here
 * in head.S.
 */
char stack[2*STACK_SIZE];

extern char shared_info[PAGE_SIZE];

/* Assembler interface fns in entry.S. */
void hypervisor_callback(void);
void failsafe_callback(void);

#if defined(__x86_64__)
#define __pte(x) ((pte_t) { (x) } )
#else
#define __pte(x) ({ unsigned long long _x = (x);        \
    ((pte_t) {(unsigned long)(_x), (unsigned long)(_x>>32)}); })
#endif

static
shared_info_t *map_shared_info(unsigned long pa)
{
    int rc;

	if ( (rc = HYPERVISOR_update_va_mapping(
              (unsigned long)shared_info, __pte(pa | 7), UVMF_INVLPG)) )
	{
		printk("Failed to map shared_info!! rc=%d\n", rc);
		do_exit();
	}
	return (shared_info_t *)shared_info;
}

static inline void fpu_init(void) {
	asm volatile("fninit");
}

#ifdef __SSE__
static inline void sse_init(void) {
	unsigned long status = 0x1f80;
	asm volatile("ldmxcsr %0" : : "m" (status));
}
#else
#define sse_init()
#endif


/*
 * INITIAL C ENTRY POINT.
 */
void
arch_init(start_info_t *si)
{
	static char hello[] = "Bootstrapping...\n";

	(void)HYPERVISOR_console_io(CONSOLEIO_write, strlen(hello), hello);

	trap_init();

	/*Initialize floating point unit */
	fpu_init();

	/* Initialize SSE */
	sse_init();

	cpu_detection();

	/* Copy the start_info struct to a globally-accessible area. */
	/* WARN: don't do printk before here, it uses information from
	   shared_info. Use xprintk instead. */
	memcpy(&start_info, si, sizeof(*si));

	/* print out some useful information  */
	printk("Xen Minimal OS!\n");
	printk("  start_info: %p(VA)\n", si);
	printk("    nr_pages: 0x%lx\n", si->nr_pages);
	printk("  shared_inf: 0x%08lx(MA)\n", si->shared_info);
	printk("     pt_base: %p(VA)\n", (void *)si->pt_base);
	printk("nr_pt_frames: 0x%lx\n", si->nr_pt_frames);
	printk("    mfn_list: %p(VA)\n", (void *)si->mfn_list);
	printk("   mod_start: 0x%lx(VA)\n", si->mod_start);
	printk("     mod_len: %lu\n", si->mod_len);
	printk("       flags: 0x%x\n", (unsigned int)si->flags);
	printk("    cmd_line: %s\n",
			si->cmd_line ? (const char *)si->cmd_line : "NULL");
	printk("       stack: %p-%p\n", stack, stack + sizeof(stack));

	/* set up minimal memory infos */
	phys_to_machine_mapping = (unsigned long *)start_info.mfn_list;

	/* Grab the shared_info pointer and put it in a safe place. */
	HYPERVISOR_shared_info = map_shared_info(start_info.shared_info);

	    /* Set up event and failsafe callback addresses. */
#ifdef __i386__
	HYPERVISOR_set_callbacks(
		__KERNEL_CS, (unsigned long)hypervisor_callback,
		__KERNEL_CS, (unsigned long)failsafe_callback);
#else
	HYPERVISOR_set_callbacks(
		(unsigned long)hypervisor_callback,
		(unsigned long)failsafe_callback, 0);
#endif

	start_kernel();
}

void
arch_fini(void)
{
	/* Reset traps */
	trap_fini();

#ifdef __i386__
	HYPERVISOR_set_callbacks(0, 0, 0, 0);
#else
	HYPERVISOR_set_callbacks(0, 0, 0);
#endif
}

void
arch_do_exit(void)
{
	stack_walk();
}
