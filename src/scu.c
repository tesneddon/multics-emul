/*
    scu.c -- System Controller
    
    See AN70, section 8 and GB61.

    There were a few variations of SCs and SCUs:
        SCU -- Series 60 Level66 Controller
        SC -- Level 68 System Controller
        4MW SCU -- A later version of the L68 SC

    SCUs control access to memory.
        Each SCU owns a certain range of absolute memory.
        This emulator allows the CPU to access memory directly however.
    SCUs contain clocks.
    SCUS also contain facilites which allow CPUS and IOMs to communicate.
        CPUs or IOMS request access to memory via the SCU.
        CPUs use the cioc instr to talk to IOMs and other CPUs via a SCU.
        IOMs use interrupts to ask a SCU to signal a CPU.
    Other Interesting instructions:
        read system controller reg and set system controller reg (rscr & sscr)

*/

/*
    Physical Details & Interconnection -- AN70, section 8.

    SCUs have 8 ports.  Active modules (CPUs and IOMs) have up to four
    of their ports connected to SCUs.

    SCUs have stores (memory banks) and two sets of registers ("A" and "B")
    that control interrupt assignment.

    Two sets of interrupt registers -- "A" and "B".  Each set has:
        * execute interrupt mask register -- 32 bits; enables/disables
          the corresponding execute interrupt cell
        * interrupt mask assignment register -- 9 bits total
          two parts: assigned bit, set of assigned ports (8 bits)
          In Multics, only one CPU will be assigned in either mask
          and no CPU appears in both.

    Config panel -- level 68 6000 SCU
        -- from AM81
        store A and store B
            3 position rotary switch: on line, maint, off line
            size: 32k, 64k, 128k, 256k
        exec interrupt mask assignment
            four 10-position rotary switches (a through d): off, 0, .. 7, M
            One switch for each program interrupt register
            Assign mask registers to system ports
            Normally assign one mask reg to each CPU
        
    Config panel -- Level 68 System Controller UNIT (4MW SCU)
        -- from AM81
        Store A, A1, B, B1 (online/offline)
        LWR Store Size
        PORT ENABLE
            Eight on/off switches
            Should be on for each port connected to a configured CPU
        mask/port assignment
            Two rotary switchs (A & B); set to (off, 0..7)
            See EXEC INTERRUPT on the 6000 SCU
            When booting, one should be set to the port connected to
            the bootload CPU.   The other should be off.

    If memory port B of CPU C goes to SCU D, then memory port B of all
    other CPUs *and* IOMs must go to SCU D. -- AN70, 8-4.

    The base address of the SCU is the actual memory size * the port
    assignment. -- AN70, 8-6.
*/


/*
    The following comment is probably wrong:    
        The term SCU is used throughout this code to match AL39, but the
        device emulated is closer to a Level 68 System Controller (SC) than
        to a Series 60 Level 66 Controller (SC).  The emulated device may
        be closer to a Level 68 4MW SCU than to an Level 68 6000 SCU.

    BUG/TODO: The above is probably wrong; we explicitly report an
    ID code for SCU via rscr 000001x.  It wouldn't hurt to review
    all the code to make sure we never act like a SC instead of an
    SCU.
*/
    

/*
=== Initialization and Booting -- Part 1 -- Operator's view

Booting Instructions (GB61)
    First boot the BCE OS (Bootload command Environment).  See below.
    A config deck is used
    Bootload SCU is the one with a base addr of zero.
    BCE is on a BCE/Multics System tape
    Booted from tape into the system via bootload console

*/

/*
58009906 (DPS8)
    When CPU needs to address the SCU (for a write/read data cycle,
    for example), the ETMCM board int the CU of the CPU issues a $INT
    to the SCU.  This signal is sent ... to the SCAMX active port
    control board in the SCU
*/

/*
    // How?  If one of the 32 interrupt cells is set in one of the SCs,
    // our processor will have the interrupt present (XIP) line active.
    // Perhaps faults are flagged in the same way via the SXC system
    // controller command.
*/

// =============================================================================
#if 0
TEMPORARY
        Each SCU owns a certain range of absolute memory.
        CPUs use the cioc instr to talk to IOMs and other CPUs via a SCU.
        IOMs use interrupts to ask a SCU to signal a CPU.
        read system controller reg and set system controller reg (rscr & sscr)
    Bootload SCU is the one with a base addr of zero.
58009906
    When CPU needs to address the SCU (for a write/read data cycle,
    for example), the ETMCM board int the CU of the CPU issues a $INT
    to the SCU.  This signal is sent ... to the SCAMX active port
    control board in the 
-----------------------
    // How?  If one of the 32 interrupt cells is set in one of the SCs,
    // our processor will have the interrupt present (XIP) line active.
    // Perhaps faults are flagged in the same way via the SXC system
    // controller command.
#endif

/*
    *** More (new) notes ***

instr rmcm -- read mem controller mask register
    ... for the selected controller, if the processor has a mask register
    assigned ..
instr smcm -- set  mem controller mask register
    ... for the selected controller, if the processor has a mask register
    assigned, set it to C(AQ)
instr smic
    turn on interrupt cells (any of 0..31)
instr cioc -- connect i/o channel, pg 173
    SC addressed by Y sends a connect signal to the port specified
    by C(Y)33,35
instr rscr & sscr -- Read/Store System Controller Register, pg 170

32 interrupt cells ... XIP
mask info
    8 mask registers
58009906
=============

AM81
Every active device (CPU, IOM) must be able to access all SCUs
Every SCU must have the same active device on the same SCU, so
all SCUs must have the same PORT ENABLE settings
Every active device must have the same SCU on the same port,
so all active devices will have the same config panel settings.
Ports must correspond -- port A on every CPU and IOM must either
be connected tothe same SCU or not connected to any SCU.
IOMs should be on lower-numbered SCU ports than CPUs.
Multics can have 16MW words of memory.
CPUs have 8 ports, a..h.
SCUs have 8 ports, 0..7.


*/

// ============================================================================

#include "hw6180.h"
#include <sys/time.h>

extern cpu_ports_t cpu_ports;
extern scu_t scu;   // BUG: we'll need more than one for max memory.  Unless we can abstract past the physical HW's capabilities
static int pima_parse_raw(int pima, const char *moi);

static int scu_hw_arg_check(const char *tag, t_uint64 addr, int port)
{
    // Sanity check args
    // Verify that HW could have received signal

    if (port < 0 || port > 7) {
        log_msg(ERR_MSG, "SCU", "%s: Port %d from sscr is out of range 0..7\n", tag, port);
        cancel_run(STOP_BUG);
        return 2;
    }

#if 0
    return 0;
#else
    // Verify that HW could have received signal
    int cpu_no = cpu_ports.scu_port;    // port-no that rscr instr came in on
    int cpu_port = scu.ports[cpu_no];   // which port on the CPU connects to SCU

    // Verify that HW could have received signal
    if (cpu_port < 0) {
        log_msg(ERR_MSG, "SCU", "%s: SCU port %d is disabled; it is not connected to anything\n", tag, cpu_no);
        cancel_run(STOP_WARN);
        return 1;
    }
    if (cpu_port > 7) {
        log_msg(ERR_MSG, "SCU", "%s: Port %d is not enabled (%d).\n", tag, cpu_no, cpu_port);
        cancel_run(STOP_WARN);
        return 1;
    }
    if (cpu_ports.ports[cpu_port] != cpu_no) {
        log_msg(ERR_MSG, "SCU", "%s: Port %d on CPU is not connected to port %d of SCU.\n", tag, cpu_port, cpu_no);
        cancel_run(STOP_WARN);
        return 1;
    }
    return 0;
#endif
}


int scu_set_mask(t_uint64 addr, int port)
{
    // BUG: addr should determine which SCU is selected
    // Implements part of the sscr instruction -- functions y00[0-7]2x

    const char* moi = "SCU::setmask";
    if (scu_hw_arg_check(moi, addr, port) > 1)
        return 1;
    else
        log_msg(WARN_MSG, moi, "continuing...\n");
    int cpu_no = cpu_ports.scu_port;    // port-no that instr came in on
    int cpu_port = scu.ports[cpu_no];   // which port on the CPU connects to SCU

    // Find mask reg assigned to specified port
    int port_pima = 0;
    int cpu_pima = 0;
    int cpu_found = 0;
    int port_found = 0;
    for (int p = 0; p < ARRAY_SIZE(scu.interrupts); ++p) {
        if (! scu.interrupts[p].avail)
            continue;
        if (scu.interrupts[p].mask_assign.unassigned)
            continue;
        if (scu.interrupts[p].mask_assign.port == port) {
            port_pima = p;
            if (cpu_no != port)
                log_msg(NOTIFY_MSG, moi, "Found MASK %d assigned to port %d\n", p, port);
            ++ port_found;
        }
        if (scu.interrupts[p].mask_assign.port == cpu_no) {
            cpu_pima = p;
            log_msg(NOTIFY_MSG, moi, "Found MASK %d assigned to CPU/port %d\n", p, cpu_no);
            ++ cpu_found;
        }
    }

    if (! cpu_found) {
        log_msg(WARN_MSG, moi, "No masks assigned to cpu on port %d\n", cpu_no);
        fault_gen(store_fault);
        return 1;
    }
    if (cpu_found > 1) {
        // Not legal for Multics
        log_msg(WARN_MSG, moi, "Multiple masks assigned to cpu on port %d\n", cpu_no);
        cancel_run(STOP_WARN);
    }
    if (cpu_no != port) {
        if (! port_found) {
            log_msg(WARN_MSG, moi, "No masks assigned to port %d\n", cpu_no);
            return 0;
        }
        if (port_found > 1)
            log_msg(WARN_MSG, moi, "Multiple masks assigned to port %d\n", cpu_no);
    }

    if (port_pima > 1) {
        log_msg(ERR_MSG, moi, "Cannot write to masks other than zero and one: %d\n", port_pima);
        cancel_run(STOP_BUG);
        return 1;
    }

    // See AN87
    scu.interrupts[port_pima].exec_intr_mask = 0;
    scu.interrupts[port_pima].exec_intr_mask |= (getbits36(reg_A, 0, 16) << 16);
    scu.interrupts[port_pima].exec_intr_mask |= getbits36(reg_Q, 0, 16);
    return 0;
}


int scu_set_cpu_mask(t_uint64 addr)
{
    // BUG: addr should determine which SCU is selected

    if (scu_hw_arg_check("smcm", addr, 0) > 1)
        return 1;
    else
        log_msg(WARN_MSG, "smcm", "continuing...\n");
    int cpu_no = cpu_ports.scu_port;    // port-no that instr came in on

    return scu_set_mask(addr, cpu_no);
}


int scu_get_cpu_mask(t_uint64 addr)
{
    // BUG: addr should determine which SCU is selected

    const char *moi = "SCU::rmcm";

    if (scu_hw_arg_check(moi, addr, 0) > 1)
        return 1;
    int cpu_no = cpu_ports.scu_port;    // port-no that instr came in on

    reg_A = 0;
    reg_Q = 0;
    return scu_get_mask(addr, cpu_no);
}

int scu_get_mode_register(t_uint64 addr)
{
    // Implements part of the rscr instruction -- function  y0000x
    // BUG: addr should determine which SCU is selected

#if 1
    // BUG: is it really OK for all ports to be disabled?
    if (scu_hw_arg_check("get-mode-register", addr, 0) != 0)
        log_msg(ERR_MSG, "get-mode-register", "But proceeding anyway");
#endif

    int cpu_no = cpu_ports.scu_port;    // port-no that instr came in on
    int cpu_port = scu.ports[cpu_no];   // which port on the CPU connects to SCU


    // See scr.incl.pl1 and AN87 page 2-2

    // Note that rscr 0001X can only report an SC with a memory sizes of up
    // to 256 K-words, but rscr 0001X can report an SCU with up to 4MW.  So,
    // we identify ourselves as an 4MW SCU.

    reg_A = 0;  // first 50 bits are padding
    reg_Q = 0;
    reg_Q |= setbits36(reg_Q, 50-36, 4, 2); // id for a 4MW SCU (level 66 SCU)
    /*
        remaining bits are only for T&D test and diagnostics
    */
        // reg_Q |= setbits36(reg_Q, 54-36, 2, 0);  // TS strobe normal timing
        // reg_Q |= setbits36(reg_Q, 64-36, 2, 0);  // both 00b and 10b mean normal voltage
        // reg_Q |= setbits36(reg_Q, 70-36, 1, 0);  // SGR accepted

    return 0;
}


int scu_get_config_switches(t_uint64 addr)
{
    // Implements part of the rscr instruction -- function y0001x
    // Returns info appropriate to a 4MW SCU
    // BUG: addr should determine which SCU is selected

    const char *tag = "get-config-switches";
    const char *moi = "SCU::get-config-switches";
#if 1
    if (scu_hw_arg_check(tag, addr, 0) != 0)
        log_msg(ERR_MSG, moi, "But proceeding anyway");
#endif
    int cpu_no = cpu_ports.scu_port;    // port-no that instr came in on
    int cpu_port = scu.ports[cpu_no];   // which port on the CPU connects to SCU


    // See scr.incl.pl1 
    reg_A = 0;
    // interrupt mask A port assignment
    reg_A = setbits36(reg_A, 0, 9, scu.interrupts[0].mask_assign.raw);
    // We have 4 banks and can have 4M-words, so report banks size 1024K-words
    reg_A = setbits36(reg_A, 9, 3, 5);  // size of lower store -- 2^(5+5) == 1024 K-words
    reg_A = setbits36(reg_A, 12, 4, 017);   // all four stores online
    reg_A = setbits36(reg_A, 16, 4, cpu_no);    // requester's port #
    reg_A = setbits36(reg_A, 21, 1, scu.mode);  // programmable
    reg_A = setbits36(reg_A, 22, 1, 0); // non-existent address logic enabled
    reg_A = setbits36(reg_A, 23, 7, 0); // nea size
    reg_A = setbits36(reg_A, 30, 1, 1); // internally interlaced
    reg_A = setbits36(reg_A, 31, 1, 0); // store B is lower?
    for (int i = 0; i < 4; ++ i) {
        int pima = 0;
        int port = i + pima * 4;
        int enabled = scu.ports[port] >= 0;
        reg_A = setbits36(reg_A, 32+i, 1, enabled); // enable masks for ports 0-3
        if (enabled)
            log_msg(INFO_MSG, moi, "Port %d is enabled, it points to port %d.\n", port, scu.ports[port]);
        else
            log_msg(INFO_MSG, moi, "Port %d is disabled.\n", port);
    }

    reg_Q = 0;
    reg_Q = setbits36(reg_Q, 0, 9, scu.interrupts[1].mask_assign.raw);
    reg_Q = setbits36(reg_Q, 57-36, 7, 0);  // cyclic port priority switches; BUG
    for (int i = 0; i < 4; ++ i) {
        int pima = 1;
        int port = i + pima * 4;
        int enabled = scu.ports[port] >= 0;
        reg_Q = setbits36(reg_Q, 68-36+i, 1, enabled);  // enable masks for ports 4-7
        if (enabled)
            log_msg(INFO_MSG, moi, "Port %d is enabled, it points to port %d.\n", port, scu.ports[port]);
        else
            log_msg(INFO_MSG, moi, "Port %d is disabled.\n", port);
    }

    return 0;
}


int scu_set_config_switches(t_uint64 addr)
{
    // Implements part of the sscr instruction, function y0001x
    // Only valid for a 4MW SCU
    // BUG: addr should determine which SCU is selected
    const char* moi = "SCU::set-config-switches";

    if (scu_hw_arg_check(moi, addr, 0) > 1) {
        log_msg(WARN_MSG, moi, "failed...\n");
        return 1;
    } else
        log_msg(WARN_MSG, moi, "continuing...\n");
    int cpu_no = cpu_ports.scu_port;    // port-no that instr came in on
    int cpu_port = scu.ports[cpu_no];   // which port on the CPU connects to SCU


    // See scr.incl.pl1 

    if (scu.mode != 1) {
        log_msg(WARN_MSG, moi, "SCU mode is 'PROGRAM', not 'MANUAL' -- sscr not allowed to set switches.\n");
        cancel_run(STOP_BUG);
    }

    // get settings from reg A

    int pima = 0;
    scu.interrupts[pima].mask_assign.raw = getbits36(reg_A, 0, 9);
    pima_parse_raw(pima, moi);

    // BUG: We don't allow changes to many of the settings
    //getbits36(reg_A, 9, 3);   // size of lower store -- 2^(5+5) == 1024 K-words
    //getbits36(reg_A, 12, 4);  // all four stores online
    // requester's port cannot be set from bits 16..20 of A 
    // scu.mode = getbits36(reg_A, 21, 1);  // programmable flag; not for sscr
    // getbits36(reg_A, 22, 1); // non-existent address logic enabled
    // getbits36(reg_A, 23, 7); // nea size
    // getbits36(reg_A, 30, 1); // internally interlaced
    // getbits36(reg_A, 31, 1); // store B is lower?
    // check enable masks -- see rscr reg a 32..36
    // Set enable masks for ports 0-3
    for (int i = 0; i < 4; ++ i) {
        int enabled = getbits36(reg_A, 32+i, 1);
        int port = i+pima*4;
        if (! enabled) {
            if (scu.ports[port] < 0)
                log_msg(INFO_MSG, moi, "Port %d still disabled.\n", port);
            else
                log_msg(INFO_MSG, moi, "Port %d now disabled.\n", port);
            scu.ports[port] = -1;
        } else
            if (scu.ports[port] < 0) {
                log_msg(ERR_MSG, moi, "Cannot set port-enable-register flag on port %d\n", port);
                cancel_run(STOP_BUG);
            } else
                log_msg(INFO_MSG, moi, "Port %d still enabled.\n", port);
        // BUG: switch to using enable/disable flag
    }


    // get settings from reg Q
    
    pima = 1;
    scu.interrupts[pima].mask_assign.raw = getbits36(reg_Q, 0, 9);
    pima_parse_raw(pima, moi);

    // BUG  getbits36(reg_Q, 57-36, 7, 0);  // cyclic port priority switches; BUG
    for (int i = 0; i < 4; ++ i) {
        int enabled = getbits36(reg_Q, 32+i, 1);
        int port = i+pima*4;
        if (! enabled) {
            if (scu.ports[port] < 0)
                log_msg(INFO_MSG, moi, "Port %d still disabled.\n", port);
            else
                log_msg(INFO_MSG, moi, "Port %d now disabled.\n", port);
            scu.ports[port] = -1;
        } else
            if (scu.ports[port] < 0) {
                log_msg(ERR_MSG, moi, "Cannot set port-enable-register flag on port %d\n", port);
                cancel_run(STOP_BUG);
            } else
                log_msg(INFO_MSG, moi, "Port %d still enabled.\n", port);
        // BUG: switch to using enable/disable flag
    }

    log_msg(NOTIFY_MSG, moi, "Doing BUG check.\n");
    int ret = 0;
    t_uint64 a = reg_A;
    t_uint64 q = reg_Q;
    scu_get_config_switches(addr);
    if (a == reg_A) {
        log_msg(NOTIFY_MSG, moi, "we handled reg A correctly\n");
    } else {
        log_msg(ERR_MSG, moi, "sscr specified reg A %012llo\n", a);
        log_msg(ERR_MSG, moi, "we used              %012llo\n", reg_A);
        reg_A = a;
        ret = 1;
    }
    if (q == reg_Q) {
        log_msg(NOTIFY_MSG, moi, "we handled reg Q correctly\n");
    } else {
        log_msg(ERR_MSG, moi, "sscr specified reg Q %012llo\n", q);
        log_msg(ERR_MSG, moi, "we used              %012llo\n", reg_Q);
        reg_Q = q;
        ret = 1;
    }

    if (ret == 0) {
        log_msg(WARN_MSG, moi, "Unfinished but OK.\n");
        cancel_run(STOP_WARN);
    }
    else {
        log_msg(ERR_MSG, moi, "Unfinished and incorrect.\n");
        cancel_run(STOP_BUG);
    }
    return ret;
}


int scu_get_mask(t_uint64 addr, int port)
{
    // BUG: addr should determine which SCU is selected
    // Implements part of the rscr instruction, function y00[0-7]2x
    const char *moi = "SCU::get-mask";

    if (scu_hw_arg_check("getmask", addr, port) > 1)
        return 1;
    else
        log_msg(WARN_MSG, "getmask", "continuing...\n");
    int cpu_no = cpu_ports.scu_port;    // port-no that instr came in on
    int cpu_port = scu.ports[cpu_no];   // which port on the CPU connects to SCU

    // Find which of the 4 masks are assigned to the specified port
    // Unlike sscr scu_set_mask, we don't care about the CPU's port
    int port_pima = 0;
    int port_found = 0;
    for (int p = 0; p < ARRAY_SIZE(scu.interrupts); ++p) {
        if (! scu.interrupts[p].avail)
            continue;
        if (scu.interrupts[p].mask_assign.unassigned)
            continue;
        if (scu.interrupts[p].mask_assign.port == port) {
            port_pima = p;
            log_msg(NOTIFY_MSG, moi, "Found MASK %d assigned to port %d\n", p, port);
            ++ port_found;
        }
    }

    if (! port_found) {
        // TODO: AL-39 doesn't say what to do if the port has no mask
        // assigned.   However, rmcm zeros register A and Q for a
        // similar case...
        reg_A = 0;
        reg_Q = 0;
        log_msg(WARN_MSG, moi, "No masks assigned to port %d\n", cpu_no);
        return 0;
    }
    if (port_found > 1)
        log_msg(WARN_MSG, moi, "Multiple masks assigned to port %d\n", cpu_no);

    // See AN87
    reg_A = setbits36(0, 0, 16, scu.interrupts[port_pima].exec_intr_mask >> 16);
    unsigned mask = 0;
    for (int i = 0; i < 4; ++ i) {
        int enabled = scu.ports[port] < 0;
        mask |= enabled;
        mask <<= 1;
    }
    reg_A |= mask;

    reg_Q = setbits36(0, 0, 16, scu.interrupts[port_pima].exec_intr_mask & MASKBITS(16));
    mask = 0;
    for (int i = 0; i < 4; ++ i) {
        int enabled = scu.ports[port+4] < 0;
        mask |= enabled;
        mask <<= 1;
    }
    reg_Q |= mask;

    return 0;
}


int scu_get_calendar(t_uint64 addr)
{
    // BUG: addr should determine which SCU is selected

    if (scu_hw_arg_check("get-calendar", addr, 0) != 0)
        return 1;
    int cpu_no = cpu_ports.scu_port;    // port-no that instr came in on
    int cpu_port = scu.ports[cpu_no];   // which port on the CPU connects to SCU

    // 52 bit clock
    // microseconds since 0000 GMT, Jan 1, 1901 // not 1900 which was a per century exception to leap years

    // sim_os_msec() gives elapsed time in microseconds; But does it return an OS agnositic value?

    // gettimeofday() is POSIX complient
    struct timeval tv;
    struct timezone tz;
    if (gettimeofday(&tv, &tz) != 0) {
        log_msg(ERR_MSG, "SCU::getcal", "Error from OS gettimeofday\n");
        reg_A = 0;
        reg_Q = 0;
        return 1;
    }
    t_uint64 seconds = tv.tv_sec;
    seconds += (t_uint64) 69 * 365 * 24 * 3600; // returned time is since epoch of 00:00:00 UTC, Jan 1, 1970
    t_uint64 now = seconds * 1000000 + tv.tv_usec;
    reg_Q = now & MASK36;
    reg_A = (now >> 36) & MASK36;
    calendar_a = reg_A; // only for debugging
    calendar_q = reg_Q; // only for debugging
    // log_msg(INFO_MSG, "calendar", "UNIX time %ld; multics time {%012llo, %012llo}\n", tv.tv_sec, reg_A, reg_Q);

    return 0;
}


int scu_cioc(t_uint64 addr)
{
    // BUG: addr should determine which SCU is selected

    if (scu_hw_arg_check("cioc", addr, 0) != 0)
        return 1;
    int cpu_no = cpu_ports.scu_port;    // port-no that instr came in on
    int cpu_port = scu.ports[cpu_no];   // which port on the CPU connects to SCU

    t_uint64 word;
    int ret;
    if ((ret = fetch_word(TPR.CA, &word)) != 0) {
        cancel_run(STOP_BUG);
        return ret;
    }
    int port = word & 7;
    log_msg(DEBUG_MSG, "SCU::cioc", "Contents of %llo are: %llo => port %d\n", addr, word, port);
    // OK ... what's a connect signal (as opposed to an interrupt?
    // A connect signal does the following (AN70, 8-7):
    //  IOM target: connect strobe to IOM
    //  CPU target: connect fault
    
    // todo: check if enabled & not masked

    static int n_cioc = 0;
    {
        //static int n_cioc = 0;
        log_msg(NOTIFY_MSG, "SCU::cioc", "CIOC # %d\n", ++ n_cioc);
        if (n_cioc >= 306) {        // BUG: temp hack to increase debug level
            extern DEVICE cpu_dev;
            ++ opt_debug; ++ cpu_dev.dctrl;
        }
    }
    log_msg(DEBUG_MSG, "SCU::cioc", "Connect sent to port %d => %d\n", port, scu.ports[port]);

    // we only have one IOM, so signal it
    // todo: sanity check port connections
    iom_interrupt();

    if (n_cioc >= 306) {
        extern DEVICE cpu_dev;
        -- opt_debug; -- cpu_dev.dctrl;
    }

    return 0;
}


static int pima_parse_raw(int pima, const char *moi)
{
    char pima_name = (pima == 0) ? 'A' : 'B';
    flag_t unassigned = scu.interrupts[pima].mask_assign.raw & 1;
    if (unassigned) {
        scu.interrupts[pima].mask_assign.unassigned = 1;
        log_msg(NOTIFY_MSG, moi, "Unassigning MASK %c.\n", pima_name);
    }
    int found = 0;
    for (int p = 0; p < 8; ++p)
        if (((1<<(8-p) & scu.interrupts[pima].mask_assign.raw)) != 0) {
            ++ found;
            scu.interrupts[pima].mask_assign.port = p;
            log_msg(NOTIFY_MSG, moi, "Assigning port %d to MASK %c.\n", p, pima_name);
        }
    if (unassigned) {
        if (found != 0) {
            log_msg(WARN_MSG, moi, "%d ports enabled for unassigned MASK %c: %#o\n", found, pima_name, scu.interrupts[pima].mask_assign.raw);
            cancel_run(STOP_WARN);
        }
        return found != 0;
    } else {
        scu.interrupts[pima].mask_assign.unassigned = found == 0;
        if (found != 1) {
            log_msg(WARN_MSG, moi, "%d ports enabled for MASK %c: %#o\n", found, pima_name, scu.interrupts[pima].mask_assign.raw);
            cancel_run(STOP_WARN);
        }
        return found != 1;
    }
}
