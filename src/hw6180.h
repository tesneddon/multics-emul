#ifndef _HW6180_H
#define _HW6180_H   // HACK

#ifdef __cplusplus
extern "C" {
#endif


#include "sim_defs.h"


/* These are from SIMH, but not listed in sim_defs.h */
extern t_addr (*sim_vm_parse_addr)(DEVICE *, char *, char **);
extern void (*sim_vm_fprint_addr)(FILE *, DEVICE *, t_addr);
extern uint32 sim_brk_types, sim_brk_dflt, sim_brk_summ; /* breakpoint info */

/* Additions to SIMH -- may conflict with future versions of SIMH */
#define REG_USER1 040000000
#define REG_USER2 020000000

// ============================================================================

/*
        For efficiency, we mostly use full ints instead of bit fields for
    the flags and other fields of most of the typedefs here.  When necessary,
    such as when an instruction saves a control register to memory, the more
    efficient variables are copied into appropriate bit fields.
        Also note that we only represent selected fields of some of the
    registers.  The simulator doesn't need some of the various scratches
    and the OS doesn't need them either.
        Descriptions of registers may be found in AN87 and AL39.
*/

typedef unsigned int uint;  // efficient unsigned int, at least 32 bits
typedef unsigned flag_t;    // efficient unsigned flag

// ============================================================================
// === Misc enum typdefs

typedef enum { ABSOLUTE_mode, APPEND_mode, BAR_mode } addr_modes_t;
typedef enum { NORMAL_mode, PRIV_mode } instr_modes_t;
typedef enum {
    ABORT_cycle, FAULT_cycle, EXEC_cycle, FAULT_EXEC_cycle, INTERRUPT_cycle,
    FETCH_cycle
// CA FETCH OPSTORE, DIVIDE_EXEC
} cycles_t;

enum faults {
    // Note that these numbers are decimal, not octal.
    // Faults not listed are not generated by the simulator.
    shutdown_fault = 0, store_fault = 1, mme1_fault = 2, f1_fault = 3,
    timer_fault = 4, cmd_fault = 5, connect_fault = 8,
    illproc_fault = 10, startup_fault = 12, overflow_fault = 13,
    div_fault = 14, dir_flt0_fault = 16, acc_viol_fault = 20,
    mme2_fault = 21, mme3_fault = 22, mme4_fault = 23, fault_tag_2_fault = 24,
    trouble_fault = 31,
    //
    oob_fault=32    // out-of-band, simulator only, the physical HW never generated this
};

// Simulator stop codes (as returned by sim_instr)
//      Zero and values above 63 reserved by SIMH
enum sim_stops {
    STOP_MEMCLEAR = 1,  // executing empty memory; zero reserved by SIMH
    STOP_BUG,           // impossible conditions, coding error;
    STOP_WARN,          // something odd or interesting; further exec might possible
    STOP_ODD_FETCH,
    STOP_IBKPT          // breakpoint
};
enum dev_type { DEV_NONE, DEV_TAPE, DEV_CON, DEV_DISK };    // devices connected to an IOM
enum log_level { DEBUG_MSG, NOTIFY_MSG, WARN_MSG, ERR_MSG };


// ============================================================================
// === Misc constants and macros

// Clocks
#define CLK_TR_HZ (512*1) /* should be 512 kHz, but we'll use 512 Hz for now */
#define TR_CLK 1 /* SIMH allows clock ids 0..7 */

// Memory
#define IOM_MBX_LOW 01200
#define IOM_MBX_LEN 02200
#define DN355_MBX_LOW 03400
#define DN355_MBX_LEN 03000

#define ARRAY_SIZE(a) ( sizeof(a) / sizeof((a)[0]) )

enum { seg_bits = 15};  // number of bits in a segment number
enum { n_segments = 1 << seg_bits} ;    // why does c89 treat enums as more constant than consts?

// ============================================================================
// === Struct typdefs

/* MF fields of EIS multi-word instructions -- 7 bits */
typedef struct {
    flag_t ar;
    flag_t rl;
    flag_t id;
    uint reg;   // 4 bits
} eis_mf_t;

/* Format of a 36 bit instruction word */
typedef struct {
    uint addr;  // 18 bits at 0..17; 18 bit offset or seg/offset pair
    uint opcode;    /* 10 bits at 18..27 */
    uint inhibit;   /* 1 bit at 28 */
    union {
        struct {
            uint pr_bit;    // 1 bit at 29; use offset[0..2] as pointer reg
            uint tag;       /* 6 bits at 30..35 */
        } single;
        eis_mf_t mf1;       // from bits 29..35 of EIS instructions
    } mods;
    flag_t is_eis_multiword;    // set true for relevent opcodes
} instr_t;

//typedef struct {
//  // 18 bits at 0..17 of an instruction or indir word
//  // uint raw18;      // all 18 bits
//  uint pr;        // first 3 bits of above if (pr_bit==1)
//  uint offset;    // unsigned offset, 15 or 18 bits
//  int soffset;    // signed offset, 15 or 18 bits
//} offset_t;


/* Indicator register (14 bits [only positions 18..32 have meaning]) */
typedef struct {
    uint zero;              // bit 18
    uint neg;               // bit 19
    uint carry;             // bit 20; see AL39, 3-6
    uint overflow;          // bit 21
    uint exp_overflow;      // bit 22   (used only by ldi/stct1)
    uint exp_underflow;     // bit 23   (used only by ldi/stct1)
    uint overflow_mask;     // bit 24
    uint tally_runout;      // bit 25
    uint parity_error;      // bit 26   (used only by ldi/stct1)
    uint parity_mask;       // bit 27
    uint not_bar_mode;      // bit 28
    uint truncation;        // bit 29
    uint mid_instr_intr_fault;  // bit 30
    uint abs_mode;          // bit 31
    uint hex_mode;          // bit 32
} IR_t;


// more simulator state variables for the cpu
// these probably belong elsewhere, perhaps control unit data or the cu-history regs...
typedef struct {
    cycles_t cycle;
    uint IC_abs;    // translation of odd IC to an absolute address; see ADDRESS of cu history
    flag_t irodd_invalid;   // cached odd instr invalid due to memory write by even instr
    uint read_addr; // last absolute read; might be same as CA for our purposes...; see APU RMA
    // flag_t instr_fetch;  // true during an instruction fetch
    // from the control unit history register:
        flag_t trgo;    // most recent instruction caused a transfer?
        flag_t ic_odd;  // executing odd pair?
        flag_t poa;     // prepare operand address
        uint opcode;    // currently executing opcode
    addr_modes_t orig_mode_BUG;
    struct {
        flag_t  fhld;   // An access violation or directed fault is waiting.   AL39 mentions that the APU has this flag, but not where scpr stores it
    } apu_state;
} cpu_state_t;


/* Fault register (72 bits) */
#if 0
typedef struct {
    // Multics never examines this (just the CPU) -- multicians.org glossary
    uint ill_op:1;      /* 1 bit at 0 */
    uint ill_mod:1;     /* 1 bit at 1 */
    uint ill_slv:1;     /* 1 bit at 2 */
    uint ill_proc:1;    /* 1 bit at 3 */
    /* ... */
} fault_reg_t;
#endif

// Simulator-only interrupt and fault info
// tentative
typedef struct {
    flag_t xed;             // executed xed for a fault handler
    flag_t any;                 // true if any of the below are true
    flag_t int_pending;
    int low_group;          // Lowest group-number fault preset
    uint32 group7;          // bitmask for multiple group 7 faults
    int fault[6];           // only one fault in groups 1..6 can be pending
    flag_t interrupts[32];
} events_t;

// Base Address Register (BAR) -- 18 bits
typedef struct {
    uint base;      // 9 bits, upper 9 bits of an 18bit base
    uint bound;     // 9 bits, upper 9 bits of an 18bit bounds
} BAR_reg_t;

// Combination: Pointer Registers and Address Registers
// Note that the eight registers are also known by the
// names: ap, ab, bp, bb, lp, lb, sp, sb
typedef struct {
    int wordno; // offset from segment base;
    struct {
        int snr;    // segment #, 15 bits
        uint rnr;   // effective ring number
        uint bitno; // index into wordno
    } PR;   // located in APU in physical hardware
    struct {
        uint charno;    // index into wordno
        uint bitno; // index into charno
    } AR;   // located in Decimal Unit in physical hardware
} AR_PR_t;

// PPR Procedure Pointer Register (pseudo register)
//      Use: Holds info relative to the location in main memory of the
//      procedure segment in execution and the location of the current
//      instruction within that segment
typedef struct {
    uint PRR;       /* Procedure ring register; 3 bits @ 0[0..2] */
    uint PSR;       /* Procedure segment register; 15 bits @ 0[3..17] */
    uint P;         /* Privileged bit; 1 bit @ 0[18] */
    int IC;         /* Instruction counter, 18 bits */
} PPR_t;

// TPR Temporary Pointer Register (pseudo register)
//      Use: current virt addr used by the processor in performing addr
//      prep for operands, indirect words, and instructions.   At the
//      completion of addr prep, the contents of the TPR is presented
//      to the appending unit associative memories for translation
//      into the 24-bit absolute main memory address.
typedef struct {
    uint TRR;   // Current effective ring number, 3 bits
    uint TSR;   // Current effective segment number, 15 bits
    uint TBR;   // Current bit offset as calculated from ITS and ITP
    uint CA;    // Current computed addr relative to the segment in TPR.TSR, 18 bits
    //t_uint64 CA;  // Current computed addr relative to the segment in TPR.TSR, 18 bits
    //int bitno;    // see TBR
    // BUG: CA value should probably be placed in ctl_unit_data_t
    uint is_value;  // is offset a value or an address? (du or dl modifiers)
    t_uint64 value; // 36bit value from opcode constant via du/dl
} TPR_t;


/* Control unit data (288 bits) */
typedef struct {
    /*
            NB: Some of the data normally stored here is represented
        elsewhere -- e.g.,the PPR is a variable outside of this
        struct.   Other data is live and only stored here.
    */
    /*      This is a collection of flags and registers from the
        appending unit and the control unit.  The scu and rcu
        instructions store and load these values to an 8 word
        memory block.
            The CU data may only be valid for use with the scu and
        rcu instructions.
            Comments indicate format as stored in 8 words by the scu
        instruction.
    */

    /* NOTE: PPR (procedure pointer register) is a combination of registers:
        From the Appending Unit
            PRR bits [0..2] of word 0
            PSR bits [3..17] of word 0
            P   bit 18 of word 0
        From the Control Unit
            IC  bits [0..17] of word 4
    */

#if 0

    /* First we list some registers we either don't use or that we have represented elsewhere */

    /* word 0 */
    // PPR portions copied from Appending Unit
    uint PPR_PRR;       /* Procedure ring register; 3 bits @ 0[0..2] */
    uint PPR_PSR;       /* Procedure segment register; 15 bits @ 0[3..17] */
    uint PPR_P;         /* Privileged bit; 1 bit @ 0[18] */
    // uint64 word0bits; /* Word 0, bits 18..32 (all for the APU) */
    uint FCT;           /* Fault counter; 3 bits at 0[33..35]; */

    /* word 1 */
    //uint64 word1bits; /* Word1, bits [0..19] and [35] */

    uint IA;        /* 4 bits @ 1[20..23] */
    uint IACHN;     /* 3 bits @ 1[24..26] */
    uint CNCHN;     /* 3 bits @ 1[27..29] */
    uint FIADDR     /* 5 bits @ 1[30..34] */

    /* word 2 */
    uint TPR_TRR;   // 3 bits @ 2[0..2];  temporary ring register
    uint TPR_TSR;   // 15 bits @ 2[3..17]; temporary segment register
    // unused: 10 bits at 2[18..27]
    // uint cpu_no; // 2 bits at 2[28..29]; from maint panel switches
    
    /* word 3 */

    /* word 4 */
    // IC belongs to CU
    int IC;         // 18 bits at 4[0..17]; instruction counter aka ilc
    // copy of IR bits 14 bits at 4[18..31]
    // unused: 4 bits at 4[32..36];

    /* word 5 */
    uint CA;        // 18 bits at 5[0..17]; computed address value (offset) used in the last address preparation cycle
    // cu bits for repeats, execute double, restarts, etc
#endif



    /* word 0, continued */
    flag_t SD_ON;   // SDWAM enabled
    flag_t PT_ON;   // PTWAM enabled

    /* word 1, continued  */
    struct {
        unsigned oosb:1;    // out of segment bounds
        unsigned ocall:1;   // outward call
        // unsigned boc:1;      // bad outward call
        // unsigned ocb:1;      // out of call brackets
    } word1flags;
    flag_t instr_fetch;     // our usage of this may match PI-AP

    /* word 2, continued */
    uint delta;     // 6 bits at 2[30..35]; addr increment for repeats

    /* word 5, continued */
    flag_t rpts;        // just executed a repeat instr;  bit 12 in word one of the CU history register
    flag_t repeat_first;        // "RF" flag -- first cycle of a repeat instruction; We also use with xed
    flag_t rpt;     // execute an rpt instruction
    uint CT_HOLD;   // 6 bits at 5[30..35]; contents of the "remember modifier" register
    flag_t xde;     // execute even instr from xed pair
    flag_t xdo;     // execute even instr from xed pair

    /* word 6 */
    instr_t IR;     /* Working instr register; addr & tag are modified */
    uint tag;       // td portion of instr tag (we only update this for rpt instructions which is the only time we need it)

    /* word 7 */
    // instr_t IRODD;   // Instr holding register; odd word of last pair fetched
    t_uint64 IRODD; /* Instr holding register; odd word of last pair fetched */
    
} ctl_unit_data_t;

// PTW --36 bits -- AN87, page 1-17 or AL39
typedef struct {
    uint addr;      // 18 bits; mod 64 abs main memory addr of page aka upper 18 bits; bits 0..17
    // uint did;    // 4 bits; bit 18..21
    // flag_t d;
    // flag_t p;
    flag_t u;       // used; bit 26
    // flag_t o;
    // flag_t y;
    flag_t m;       // modified; bit 29
    // flag_t q;
    // flag_t w;
    // flag_t s;
    flag_t f;       // directed fault (0=>page non in memory, so fault); bit 33
    uint fc;        // which directed fault; bits 34..35
} PTW_t;


// PTWAM registers, 51 bits stored by sptr & sptp.  Some bits of PTW are
// ignored by those instructions.
typedef struct {
    PTW_t ptw;
    struct {
        uint ptr;       // 15 bits; effective segment #
        uint pageno;    // 12 bits; 12 high order bits of CA used to fetch this PTW from mem
        uint is_full;   // flag; PTW is valid
        uint use;       // counter, 4 bits
        uint enabled;   // internal flag, not part of the register
    } assoc;
} PTWAM_t;

// SDW (72 bits)
typedef struct {
    // even word:
    uint addr;      // 24bit main memory addr -- page table or page segment
    uint r1;        // 3 bits
    uint r2;        // 3 bits
    uint r3;        // 3 bits
    flag_t f;       // In memory if one, fault if zero;  In SDW bit 33, stored by ssdp but not ssdr
    uint fc;        // directed fault; Bits 34..35 of even word; in SDW, but not SDWAM?
    // odd word:
    uint bound;     // 14 bits; 14 high order bits of furtherest Y-block16
    uint r;         // flag; read perm
    uint e;         // flag; exec perm
    uint w;         // flag; write perm
    uint priv;      // flag; priv
    uint u;             // flag; unpaged; bit 19 odd word of SDW
    uint g;         // flag; gate control
    uint c;         // flag; cache control
    uint cl;            // 14 bits; (inbound) call limiter; aka eb
} SDW_t;

// SDWAM registers (88 bits each)
typedef struct {
    SDW_t sdw;
    struct {
        //uint modified;    // flag
        uint ptr;           // 15 bits; effective segment #
        uint is_full;   // flag; this SDW is valid
        uint use;       // counter, 4 bits
        // uint enabled;    // internal flag, not part of the register
    } assoc;
} SDWAM_t;

// Descriptor Segment Base Register (51 bits)
typedef struct {
    uint32 addr;    // Addr of DS or addr of page tbl; 24 bits at 0..23
    uint32 bound;   // Upper bits of 16bit addr; 14 bits at 37..50
    flag_t u;       // Is paged?  1 bit at 55
    uint32 stack;   // Used by call6; 12 bits at 60..71
} DSBR_t;

// ============================================================================

// Beginnings of movement of all cpu info into a single struct.   This
// will be needed for supporting multiple CPUs.   At the moment, it mostly
// holds semi-exposed registers used during saved/restored memory debugging.
typedef struct {
    PTWAM_t PTWAM[16];  // Page Table Word Associative Memory, 51 bits
    SDWAM_t SDWAM[16];  // Segment Descriptor Word Associative Memory, 88 bits
    DSBR_t DSBR;            // Descriptor Segment Base Register (51 bits)
} cpu_t;

// Physical Switches
typedef struct {
    // Switches on the Processor's maintenance and configuration panels
    int FLT_BASE;   // normally 7 MSB of 12bit fault base addr
    int cpu_num;
} switches_t;

typedef struct {
    uint ports[8];  // SCU connectivity; designated a..h
    int scu_port;   // What port num are we connected to (same for all SCUs)
} cpu_ports_t;

typedef struct {
    // int interrupts[32];
    // uint mem_base;   // zero on boot scu
    // mode reg: not stored here; returned by scu_get_mode_register()
#if 0
    struct {
        unsigned mask_a_assign:9;
        unsigned a_online:1;    // bank a online?
        unsigned a1_online:1;
        unsigned b_online:1;
        unsigned b1_online:1;
        unsigned port_no:4;
        unsigned mode:1;    // program or manual
        unsigned nea_enabled:1;
        unsigned nea:7;
        unsigned interlace:1;
        unsigned lwr:1;     // controls whether A or B is low order memory
        unsigned port_mask_0_3:4;
        unsigned cyclic_prio:7;
        unsigned port_mask_4_7:4;
    } config_switches;
#endif
    uint ports[8];  // CPU/IOM connectivity; designated 0..7; negative to disable
    //t_uint64 masks[4];    // 32bit masks
    //uint mask_assign[4];  //  Bit masks.  Which port(s) is each PIMA reg assigned to?
    struct {
        unsigned raw;       // 9 bits; raw mask; decoded below
        flag_t avail;       // does mask exist?
        flag_t assigned;    // is it assigned to a port?
        uint port;          // 3 bits; port to which mask is assigned
    } eima_data[4];
    
} scu_t;

typedef struct {
    uint ports[8];  // CPU/IOM connectivity; designated a..h; negative to disable
    int scu_port;   // which port on the SCU(s) are we connected to?
    enum dev_type channels[64];
    DEVICE* devices[64];
} iom_t;


typedef struct {
    uint address;   // 18 bits at  0..17
    uint cn;    //  3 bits at 18..20; character position
    uint bitno; // only for bitstring operand descriptors, not alpha-numeric op descriptors
    uint ta;    //  2 bits at 21..22; data type
    uint n_orig;    // unmodified for debugging; 12 bits at 24..35; length (0..4096 or -2048..2047 ?)
    uint n;         // converted to value; 12 bits at 24..35; length (0..4096 or -2048..2047 ?)
    // convenience members, not part of stored word
    int nbits;
    // tracking info for get_eis_an() and put_eis_an()
    struct {
        // Absolute memory locations -- first address pointed
        // to by the descriptor and bounds of the relevent segment
        // or page, if any.
        // The addr member is initially the address of the first word
        // pointed to by the descriptor.  On subsequent pages, the addr
        // member matches min_addr.
        uint addr;
        int bitpos;
        uint min_addr;  // segment or page base
        uint max_addr;  // segment or page last word
    } area;
    struct {
        // absolute memory locations
        uint addr;
        int bitpos;
    } curr;
    flag_t first;
    t_uint64 word;      // buffers full words to/from memory
} eis_alpha_desc_t;
// eis_num_desc


#if 0
typedef struct {
    // A hack
    uint n;
    eis_alpha_desc_t desc;
    int bitno;
} eis_bit_desc_t;
#else
typedef eis_alpha_desc_t eis_bit_desc_t;
#endif


// ============================================================================
// === Operations on 36-bit pseudo words
/*
    Operations on 36-bit pseudo words

    Multics 36bit words are simulated with 64bit integers.  Multics
    documentation refers to the right-most or least significant bit
    as position 35.   Position zero is the leftmost bit.   This is
    unusual.  Usually, we refer to the LSB as position zero and the
    MSB (most significant bit) as the highest position number.  The latter
    convention matches up the value of a bit position with its
    twos-complement value, e.g. turning only bit #4 results is a value
    of 2 raised to the 4th power.  With the Multics notational convention,
    turning on only bit 35 results in the twos-complement value of one.

    The following macros support operating on 64bit words as though the
    right-most bit were bit 35.  This means that bit positon zero is
    in the 36th bit from the right -- in the middle of the 64 bit word.
*/

static const t_uint64 MASK36 = ~(~((t_uint64)0)<<36);   // lower 36 bits all ones
static const t_uint64 MASK18 = ~(~((t_uint64)0)<<18);   // lower 18 bits all ones
#define MASKBITS(x) ( ~(~((t_uint64)0)<<x) )    // lower (x) bits all ones

extern void log_msg(enum log_level, const char* who, const char* format, ...);
extern int log_ignore_ic_change(void);
extern int log_notice_ic_change(void);
extern void log_forget_ic(void);
extern int log_any_io(int val);
extern int words2its(t_uint64 word1, t_uint64 word2, AR_PR_t *prp);

extern int scan_seg(uint segno, int msgs);  // scan definitions section for procedure entry points
t_stat cmd_seginfo(int32 arg, char *buf);   // display segment info
extern int cmd_symtab_parse(int32 arg, char *buf);

/*  Extract (i)th bit of a 36 bit word (held in a uint64). */
#define bitval36(word,i) ( ((word)>>(35-i)) & (uint64_t) 1 )
/*  Value of a 36bit word (held in a uint64) after setting or clearing the
    the (i)th bit. */
#define bitset36(word,i) ( (word) | ( (uint64_t) 1 << (35 - i)) )
#define bitclear36(word,i) ( (word) & ~ ( (uint64_t) 1 << (35 - i)) )

static inline t_uint64 getbits36(t_uint64 x, int i, unsigned n) {
    // bit 35 is right end, bit zero is 36th from the right
    int shift = 35-i-n+1;
    if (shift < 0 || shift > 35) {
        log_msg(ERR_MSG, "getbits36", "bad args (%Lo,i=%d,n=%d)\n", x, i, n);
        return 0;
    } else
        return (x >> (unsigned) shift) & ~ (~0 << n);
}

static inline t_uint64 setbits36(t_uint64 x, int p, unsigned n, t_uint64 val)
{
    // return x with n bits starting at p set to n lowest bits of val 
    // return (x & ((~0 << (p + 1)) | (~(~0 << (p + 1 - n))))) | ((val & ~(~0 << n)) << (p + 1 - n));

    t_uint64 mask = ~ (~0<<n);  // n low bits on
        // 1 => ...1
        // 2 => ..11
    int shift = 36 - p - n;
    if (shift < 0 || shift > 35) {
        log_msg(ERR_MSG, "setbit36", "bad args\n");
        return 0;
    }
    mask <<= (unsigned) shift;  // shift 1s to proper position; result 0*1{n}0*
            // 0,1 => 35 => 1000... (35 zeros)
            // 1,1 => 34 => 0100... (34 z)
            // 0,2 = 34 => 11000 (34 z)
            // 1,2 = 33 => 011000 (33 z)
            // 35,1 => 0 => 0001 (0 z)
    // caller may provide val that is too big, e.g., a word with all bits set to one
    t_uint64 result = (x & ~ mask) | ((val&MASKBITS(n)) << (36 - p - n));
    return result;
}


// #define bit36_is_neg(x) ( ((x)>>35) == 1 )
#define bit36_is_neg(x) (((x) & (((t_uint64)1)<<35)) != 0)
#define bit27_is_neg(x) (((x) & (((t_uint64)1)<<26)) != 0)
#define bit18_is_neg(x) (((x) & (((t_uint64)1)<<17)) != 0)
#define bit_is_neg(x,n) (((x) & (((t_uint64)1)<<(n-1))) != 0)

// obsolete typedef -- hold named register sub-fields in their inefficient
// native format.
/* #define CU_PPR_P(CU) (bitval36(CU.word0bits, 18)) */

// ============================================================================
// === Variables

extern int opt_debug;
extern t_uint64 reg_A;      // Accumulator, 36 bits
extern t_uint64 reg_Q;      // Quotient, 36 bits
extern int8 reg_E;          // Floating Point exponent, 8 bits
extern uint32 reg_X[8];     // Index Registers, 18 bits; SIMH expects data type to be no larger than needed
extern IR_t IR;             // Indicator Register
extern BAR_reg_t BAR;       // Base Address Register (BAR); 18 bits
extern uint32 reg_TR;       // Timer Reg, 27 bits -- only valid after calls to SIMH clock routines
extern AR_PR_t AR_PR[8];    // Combined Pointer Registers and Address Registers
extern PPR_t PPR;           // Procedure Pointer Reg, 37 bits, internal only
extern TPR_t TPR;           // Temporary Pointer Reg, 42 bits, internal only
extern uint8 reg_RALR;      // Ring Alarm Reg, 3 bits
extern cpu_t *cpup;     // Everything you ever wanted to know about a CPU

extern ctl_unit_data_t cu;
extern cpu_state_t cpu;
extern flag_t fault_gen_no_fault;

extern t_uint64 calendar_a;
extern t_uint64 calendar_q;

// ============================================================================
// === Functions

extern void log_msg(enum log_level, const char* who, const char* format, ...);
//extern void debug_msg(const char* who, const char* format, ...);
//extern void warn_msg(const char* who, const char* format, ...);
//extern void complain_msg(const char* who, const char* format, ...);
extern void out_msg(const char* format, ...);

extern void restore_from_simh(void);    // SIMH has a different form of some internal variables
extern int cmd_dump_history(int32 arg, char *buf);
extern int cmd_xdebug(int32 arg, char *buf);
extern int cmd_find(int32 arg, char *buf);
extern int cmd_load_listing(int32 arg, char *buf);
extern int cmd_stack_trace(int32 arg, char *buf);
extern void ic2text(char *icbuf, addr_modes_t addr_mode, uint seg, uint ic);
extern char *ir2text(const IR_t *irp);

extern void state_save(void);
extern void state_dump_changes(void);
extern void check_seg_debug(void);
extern void ic_history_init(void);
extern void ic_history_add(void);
extern void show_location(int show_source_lines);

extern void cancel_run(enum sim_stops reason);
extern void fault_gen(enum faults);
extern char *bin2text(t_uint64 word, int n);

extern void decode_instr(instr_t *ip, t_uint64 word);
extern void encode_instr(const instr_t *ip, t_uint64 *wordp);
extern int fetch_instr(uint IC, instr_t *ip);
extern void execute_instr(void);
extern char* instr2text(const instr_t* ip);
extern char* print_instr(t_uint64 word);
extern void cu_safe_store(void);
extern int fault_check_group(int group);    // Do faults exist a given or higher priority?

extern int decode_addr(instr_t* ip, t_uint64* addrp);
extern int decode_ypair_addr(instr_t* ip, t_uint64* addrp);
extern void iom_interrupt(void);
extern char* print_ptw(t_uint64 word);
extern char* print_sdw(t_uint64 word0, t_uint64 word1);
extern char* sdw2text(const SDW_t *sdwp);
extern int fetch_appended(uint addr, t_uint64 *wordp);
extern int store_appended(uint offset, t_uint64 word);

extern int fetch_word(uint addr, t_uint64 *wordp);
extern int fetch_abs_word(uint addr, t_uint64 *wordp);
extern int fetch_pair(uint addr, t_uint64* word0p, t_uint64* word1p);
extern int fetch_abs_pair(uint addr, t_uint64* word0p, t_uint64* word1p);
extern int store_word(uint addr, t_uint64 word);
extern int store_abs_word(uint addr, t_uint64 word);
extern int store_abs_pair(uint addr, t_uint64 word0, t_uint64 word1);
extern int store_pair(uint addr, t_uint64 word0, t_uint64 word1);
extern void save_IR(t_uint64* wordp);
extern int store_yblock8(uint addr, const t_uint64 *wordsp);
extern int fetch_yblock(uint addr, int aligned, uint n, t_uint64 *wordsp);
extern int fetch_yblock8(uint addr, t_uint64 *wordsp);
extern int store_yblock16(uint addr, const t_uint64 *wordsp);

extern int add72(t_uint64 ahi, t_uint64 alow, t_uint64* dest1, t_uint64* dest2, int is_unsigned);

extern void mpy(t_uint64 a, t_uint64 b, t_uint64* hip, t_uint64 *lowp);
extern void div72(t_uint64 hi, t_uint64 low, t_uint64 divisor, t_uint64* quotp, t_uint64* remp);
extern void mpy72fract(t_uint64 ahi, t_uint64 alow, t_uint64 b, t_uint64* hip, t_uint64 *lowp);

extern int instr_dvf(t_uint64 word);
extern int instr_ufa(t_uint64 word);
extern int instr_ufm(t_uint64 word);
extern int instr_fno(void);

extern int cmd_dump_vm(int32 arg, char *buf);
extern int get_seg_addr(uint offset, uint perm_mode, uint *addrp);
extern int convert_address(uint* addrp, int seg, int offset, int fault);
extern int addr_mod(const instr_t *ip);
extern SDW_t* get_sdw();
extern int get_address(uint y, flag_t pr, flag_t ar, uint reg, int nbits, uint *addrp, uint* bitnop, uint *minaddrp, uint* maxaddrp);
extern void reg_mod(uint td, int off);          // BUG: might be performance boost if inlined
extern void mod2text(char *buf, uint tm, uint td);

extern eis_mf_t* parse_mf(uint mf, eis_mf_t* mfp);
extern int fetch_mf_ops(const eis_mf_t* mf1p, t_uint64* word1p, const eis_mf_t* mf2p, t_uint64* word2p, const eis_mf_t* mf3p, t_uint64* word3p);
//void fix_mf_len(uint *np, const eis_mf_t* mfp, int nbits);

// EIS multi-word instructions
extern int addr_mod_eis_addr_reg(instr_t *ip);
extern const char* mf2text(const eis_mf_t* mfp);
extern int get_eis_indir_addr(t_uint64 word, uint* addrp);
// EIS Alphanumeric operands
extern const char* eis_alpha_desc_to_text(const eis_mf_t* mfp, const eis_alpha_desc_t* descp);
extern void parse_eis_alpha_desc(t_uint64 word, const eis_mf_t* mfp, eis_alpha_desc_t* descp);
extern int get_eis_an(const eis_mf_t* mfp, eis_alpha_desc_t *descp, uint *nib);
extern int put_eis_an(const eis_mf_t* mfp, eis_alpha_desc_t *descp, uint nib);
extern int save_eis_an(const eis_mf_t* mfp, eis_alpha_desc_t *descp);
extern int get_eis_an_rev(const eis_mf_t* mfp, eis_alpha_desc_t *descp, uint *nib);
// EIS Bit String Operands
extern const char* eis_bit_desc_to_text(const eis_bit_desc_t* descp);
extern void parse_eis_bit_desc(t_uint64 word, const eis_mf_t* mfp, eis_bit_desc_t* descp);
extern int get_eis_bit(const eis_mf_t* mfp, eis_bit_desc_t *descp, flag_t *bitp);
extern int retr_eis_bit(const eis_mf_t* mfp, eis_bit_desc_t *descp, flag_t *bitp);
extern int put_eis_bit(const eis_mf_t* mfp, eis_bit_desc_t *descp, flag_t bitval);
extern int save_eis_bit(const eis_mf_t* mfp, eis_bit_desc_t *descp);

extern void load_IR(IR_t *irp, t_uint64 word);

extern void set_addr_mode(addr_modes_t mode);
extern addr_modes_t get_addr_mode(void);
extern int is_priv_mode(void);

extern int mt_iom_cmd(int chan, int dev_cmd, int dev_code, int* majorp, int* subp);
extern int mt_iom_io(int chan, t_uint64 *wordp, int* majorp, int* subp);
extern void mt_init(void);

extern void console_init(void);
extern int con_iom_cmd(int chan, int dev_cmd, int dev_code, int* majorp, int* subp);
extern int con_iom_io(int chan, t_uint64 *wordp, int* majorp, int* subp);


// ============================================================================

#ifdef __cplusplus
}
#endif

#include "opcodes.h"
// #include "symtab.h"
#include "seginfo.h"

#endif  // _HW6180_H
