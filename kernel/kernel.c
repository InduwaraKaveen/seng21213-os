/* =============================================================================
 * SENG21213-OS :: Main Kernel  (Stage 0 – Foundations)
 * File   : kernel/kernel.c
 *
 * PURPOSE
 *   This is the heart of your operating system. Right now it:
 *     1. Initialises VGA text-mode display
 *     2. Initialises the keyboard driver
 *     3. Prints a splash screen
 *     4. Runs a minimal interactive shell ("ksh")
 *
 * ASSIGNMENT MILESTONES  (what YOU will add in later lectures)
 *   Lecture  9  – Process Management  →  process.h / process.c / scheduler.c
 *   Lecture 10  – Threads             →  thread.h  / thread.c
 *   Lecture 11  – Memory Management   →  pmm.h     / pmm.c / vmm.c
 *   Lecture 12  – File System         →  fs.h      / fs.c
 *
 * CODING CONVENTION
 *   - Prefix kernel-internal functions with k_ (e.g. k_strcmp)
 *   - All driver APIs live in their own .h/.c pair
 *   - NEVER call malloc – use the PMM you build in Lecture 11
 * ============================================================================*/

#include "vga.h"
#include "keyboard.h"
#include "../include/types.h"
#include "../include/interrupts.h"
#include "../include/idt.h"
#include "../include/pit.h"
#include "../include/process.h"
#include "../include/scheduler.h"
#include "../include/thread.h"
#include "../include/mutex.h"
#include "../include/semaphore.h"
#include "../include/pmm.h"
#include "../include/fs.h"

static void test_process1(void)
{
    for (;;) {
        vga_putchar('A');

        for (volatile uint32_t i = 0; i < 500000; i++) {
        }
    }
}

static void test_thread1(void)
{
    for (;;) {
        vga_puts_color("T", VGA_LIGHT_CYAN, VGA_BLACK);
        for (volatile uint32_t i = 0; i < 1000000; i++) {
        }
    }
}

static volatile int mutex_test_counter = 0;
static mutex_t mutex_test_lock;

static semaphore_t semaphore_test_empty;
static semaphore_t semaphore_test_full;
static mutex_t semaphore_test_lock;
static int semaphore_test_buffer;
static volatile int semaphore_test_produced = 0;
static volatile int semaphore_test_consumed = 0;
static volatile int semaphore_test_errors = 0;

static void semaphore_test_producer(void)
{
    for (int i = 0; i < 100; i++) {
        semaphore_wait(&semaphore_test_empty);

        mutex_lock(&semaphore_test_lock);
        semaphore_test_buffer = i;
        semaphore_test_produced++;
        mutex_unlock(&semaphore_test_lock);

        semaphore_signal(&semaphore_test_full);
    }

    for (;;) {
        __asm__ __volatile__("hlt");
    }
}

static void mutex_test_thread1(void)
{
    for (int i = 0; i < 100000; i++) {
        mutex_lock(&mutex_test_lock);
        mutex_test_counter++;
        mutex_unlock(&mutex_test_lock);
    }

    for (;;) {
        __asm__ __volatile__("hlt");
    }
}

static void semaphore_test_consumer(void)
{
    for (int i = 0; i < 100; i++) {
        semaphore_wait(&semaphore_test_full);

        mutex_lock(&semaphore_test_lock);
        if (semaphore_test_buffer != i) {
            semaphore_test_errors++;
        }
        semaphore_test_consumed++;
        mutex_unlock(&semaphore_test_lock);

        semaphore_signal(&semaphore_test_empty);
    }

    for (;;) {
        __asm__ __volatile__("hlt");
    }
}

static void mutex_test_thread2(void)
{
    for (int i = 0; i < 100000; i++) {
        mutex_lock(&mutex_test_lock);
        mutex_test_counter++;
        mutex_unlock(&mutex_test_lock);
    }

    for (;;) {
        __asm__ __volatile__("hlt");
    }
}

static void test_process2(void)
{
    for (;;) {
        vga_putchar('B');

        for (volatile uint32_t i = 0; i < 500000; i++) {
        }
    }
}

/* ---------------------------------------------------------------------------
 * Forward declarations of shell commands
 * --------------------------------------------------------------------------*/
static void cmd_ticks(void);
static void cmd_help(void);
static void cmd_clear(void);
static void cmd_about(void);
static void cmd_echo(const char *args);
static void cmd_mem(void);
static void cmd_ls(void);
static void cmd_touch(const char *name);
static void cmd_cat(const char *name);
static void cmd_write_file(const char *name, char *argv[], int argc);
static void cmd_rm(const char *name);

/* ---------------------------------------------------------------------------
 * Utility: minimal string helpers (no libc in a freestanding kernel!)
 * --------------------------------------------------------------------------*/
static int k_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}

static size_t k_strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static int k_tokenize(char *line, char *argv[], int max_args)
{
    int argc = 0;

    while (*line && argc < max_args) {
        while (*line == ' ') {
            line++;
        }

        if (*line == '\0') {
            break;
        }

        argv[argc++] = line;

        while (*line && *line != ' ') {
            line++;
        }

        if (*line == '\0') {
            break;
        }

        *line = '\0';
        line++;
    }

    return argc;
}


/* ---------------------------------------------------------------------------
 * Splash Screen
 * --------------------------------------------------------------------------*/
static void print_splash(void) {
    vga_clear(VGA_BLACK);

    /* Top banner box */
    vga_draw_box(0, 0, 7, 80, VGA_LIGHT_MAGENTA);

    vga_set_cursor(1, 2);
    vga_puts_color("  SENG21213-OS  |  Computer Architecture & Operating Systems",
                   VGA_YELLOW, VGA_BLACK);

    vga_set_cursor(2, 2);
    vga_puts_color("  Stage 0: Kernel Foundations", VGA_LIGHT_CYAN, VGA_BLACK);

    vga_set_cursor(3, 2);
    vga_puts_color("  Faculty of Engineering – Department of Software Engineering",
                   VGA_LIGHT_GREY, VGA_BLACK);

    vga_set_cursor(4, 2);
    vga_puts_color("  Built by students, for students.  Type 'help' to begin.",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    vga_set_cursor(5, 2);
    vga_puts_color("  CPU: i686 (32-bit Protected Mode)  |  Display: VGA 80x25",
                   VGA_DARK_GREY, VGA_BLACK);

    vga_set_cursor(8, 0);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts("  Welcome! This kernel was compiled from source and booted entirely\n");
    vga_puts("  from bare metal. There is no Linux or Windows underneath – only\n");
    vga_puts("  the code you and your team write.\n");
    vga_puts("\n");
    vga_puts("  Assignment milestones to implement:\n");
    vga_puts_color("    [L09] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Process Management  – PCB, ready queue, round-robin scheduler\n");
    vga_puts_color("    [L10] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Threads & Sync      – kernel threads, mutex, semaphore\n");
    vga_puts_color("    [L11] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Memory Management   – physical page allocator, virtual memory\n");
    vga_puts_color("    [L12] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("File System         – RAM disk, FAT-like directory structure\n");
    vga_puts("\n");
}

/* ---------------------------------------------------------------------------
 * Shell command implementations
 * --------------------------------------------------------------------------*/
static void cmd_ticks(void)
{
    vga_printf("Timer ticks: %u\\n", timer_ticks);
}

static void cmd_ps(void)
{
    vga_puts("\n  PID   NAME       STATE       TICKS\n");
    vga_puts("  -----------------------------------\n");

    for (int i = 0; i < MAX_PROCS; i++) {
        if (proc_table[i].state == PROC_UNUSED) {
            continue;
        }

        const char *state;

        switch (proc_table[i].state) {
            case PROC_READY:
                state = "READY";
                break;

            case PROC_RUNNING:
                state = "RUNNING";
                break;

            case PROC_BLOCKED:
                state = "BLOCKED";
                break;

            case PROC_ZOMBIE:
                state = "ZOMBIE";
                break;

            default:
                state = "UNKNOWN";
                break;
        }

        vga_printf("  %u  %s  %s  %u\n",
                   proc_table[i].pid,
                   proc_table[i].name,
                   state,
                   proc_table[i].ticks);
    }

    vga_puts("\n");
}

static void cmd_help(void) {
    vga_puts_color("\n  SENG21213-OS Shell Commands\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  ─────────────────────────────────────────────\n");
    vga_puts("  help    – Show this help message\n");
    vga_puts("  clear   – Clear the screen\n");
    vga_puts("  about   – About this OS and course\n");
    vga_puts("  echo    – Echo text to screen\n");
    vga_puts("  mem     – Show physical memory usage\n");
    vga_puts("  meminfo – Show physical memory usage\n");
    vga_puts_color("\n  Milestones (to implement):\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ps      – [L09] List processes\n");
    vga_puts("  kill    – [L09] Terminate a process\n");
    vga_puts("  threads – [L10] List kernel threads\n");
    vga_puts("  free    – Show free physical memory\n");
    vga_puts("  ls      – List files\n");
    vga_puts("  touch   – Create an empty file\n");
    vga_puts("  cat     – Print file contents\n");
    vga_puts("  write   – Write text to a file\n");
    vga_puts("  rm      – Remove a file\n\n");
}

static void cmd_clear(void) {
    vga_clear(VGA_BLACK);
}

static void cmd_about(void) {
    vga_puts_color("\n  About SENG21213-OS\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ─────────────────────────────────────────────\n");
    vga_puts("  Architecture : x86 (i686), 32-bit Protected Mode\n");
    vga_puts("  Bootloader   : Custom MBR (NASM)\n");
    vga_puts("  Kernel       : Freestanding C (GCC, no libc)\n");
    vga_puts("  VM Target    : QEMU (qemu-system-i386)\n");
    vga_puts("  Course       : SENG 21213 – Sem 2\n");
    vga_puts("  Reference    : Stallings, OS: Internals & Design Principles\n\n");
}

static void cmd_echo(const char *args) {
    vga_puts("  ");
    vga_puts(args);
    vga_puts("\n");
}

static void cmd_mem(void) {
    uint32_t total_kb = (pmm_total_frames() * FRAME_SIZE) / 1024u;
    uint32_t free_kb  = (pmm_free_frames() * FRAME_SIZE) / 1024u;
    uint32_t used_kb  = total_kb - free_kb;

    vga_puts_color("\n  Physical Memory\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ─────────────────────────────────────────────\n");
    vga_printf("  Total : %u KB\n", total_kb);
    vga_printf("  Used  : %u KB\n", used_kb);
    vga_printf("  Free  : %u KB\n", free_kb);
    vga_printf("  Frames: %u free / %u total\n\n",
               pmm_free_frames(), pmm_total_frames());
}

/* ---------------------------------------------------------------------------
 * Stage 4 filesystem commands
 * --------------------------------------------------------------------------*/

static inode_t shell_ls_buffer[MAX_INODES];

static void cmd_ls(void)
{
    int count = fs_ls(shell_ls_buffer, MAX_INODES);

    vga_puts("\n  NAME                     SIZE\n");
    vga_puts("  ------------------------ --------\n");

    for (int i = 0; i < count; i++) {
        vga_printf("  %-24s %u\n",
                   shell_ls_buffer[i].name,
                   shell_ls_buffer[i].size);
    }

    vga_printf("  %d file(s)\n\n", count);
}

static void cmd_touch(const char *name)
{
    if (name == 0 || name[0] == '\0') {
        vga_puts("  Usage: touch <file>\n");
        return;
    }

    int fd = fs_open(name, O_WRONLY | O_CREAT);

    if (fd < 0) {
        vga_puts("  Cannot create file\n");
        return;
    }

    fs_close(fd);
    vga_puts("  OK\n");
}

static void cmd_cat(const char *name)
{
    if (name == 0 || name[0] == '\0') {
        vga_puts("  Usage: cat <file>\n");
        return;
    }

    int fd = fs_open(name, O_RDONLY);

    if (fd < 0) {
        vga_puts("  File not found\n");
        return;
    }

    char buffer[513];

    for (;;) {
        int n = fs_read(fd, buffer, 512);

        if (n <= 0) {
            break;
        }

        buffer[n] = '\0';
        vga_puts(buffer);

        if (n < 512) {
            break;
        }
    }

    vga_puts("\n");
    fs_close(fd);
}

static void cmd_write_file(const char *name, char *argv[], int argc)
{
    if (name == 0 || name[0] == '\0' || argc < 3) {
        vga_puts("  Usage: write <file> <text...>\n");
        return;
    }

    int fd = fs_open(name, O_WRONLY | O_CREAT | O_TRUNC);

    if (fd < 0) {
        vga_puts("  Cannot open file for writing\n");
        return;
    }

    int total = 0;

    for (int i = 2; i < argc; i++) {
        if (i > 2) {
            char space = ' ';
            total += fs_write(fd, &space, 1);
        }

        int len = (int)k_strlen(argv[i]);

        if (len > 0) {
            int written = fs_write(fd, argv[i], len);
            total += written;

            if (written != len) {
                vga_puts("  Write incomplete\n");
                break;
            }
        }
    }

    fs_close(fd);

    vga_printf("  Wrote %d bytes\n", total);
}

static void cmd_rm(const char *name)
{
    if (name == 0 || name[0] == '\0') {
        vga_puts("  Usage: rm <file>\n");
        return;
    }

    if (fs_unlink(name) < 0) {
        vga_puts("  File not found\n");
        return;
    }

    vga_puts("  OK\n");
}

/* ---------------------------------------------------------------------------
 * Shell process
 * --------------------------------------------------------------------------*/
static char  shell_buf[256];
static char  prompt[] = "\n  ksh> ";

static void shell_run(void) {
    vga_puts_color("\n  Kernel Shell ready. Type 'help' for commands.\n",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    while (true) {
        vga_puts_color(prompt, VGA_LIGHT_GREEN, VGA_BLACK);
        kb_readline(shell_buf, sizeof(shell_buf));

        /* Tokenize command line */
        char *argv[16];
        int argc = k_tokenize(shell_buf, argv, 16);

        if (argc == 0) {
            continue;
        }

        const char *cmd = argv[0];

        /* Dispatch */
        if (k_strcmp(cmd, "help")  == 0) { cmd_help();  continue; }
        if (k_strcmp(cmd, "clear") == 0) { cmd_clear(); continue; }
        if (k_strcmp(cmd, "about") == 0) { cmd_about(); continue; }
        if (k_strcmp(cmd, "mem")     == 0 ||
            k_strcmp(cmd, "meminfo")  == 0 ||
            k_strcmp(cmd, "free")     == 0) {
            cmd_mem();
            continue;
        }
        if (k_strcmp(cmd, "ticks") == 0) { cmd_ticks(); continue; }
        if (k_strcmp(cmd, "ps")    == 0) { cmd_ps();    continue; }

        if (k_strcmp(cmd, "ls") == 0) {
            cmd_ls();
            continue;
        }

        if (k_strcmp(cmd, "touch") == 0) {
            if (argc != 2) {
                vga_puts("  Usage: touch <file>\n");
            } else {
                cmd_touch(argv[1]);
            }
            continue;
        }

        if (k_strcmp(cmd, "cat") == 0) {
            if (argc != 2) {
                vga_puts("  Usage: cat <file>\n");
            } else {
                cmd_cat(argv[1]);
            }
            continue;
        }

        if (k_strcmp(cmd, "write") == 0) {
            if (argc < 3) {
                vga_puts("  Usage: write <file> <text...>\n");
            } else {
                cmd_write_file(argv[1], argv, argc);
            }
            continue;
        }

        if (k_strcmp(cmd, "rm") == 0) {
            if (argc != 2) {
                vga_puts("  Usage: rm <file>\n");
            } else {
                cmd_rm(argv[1]);
            }
            continue;
        }

        if (k_strcmp(cmd, "echo") == 0) {
            if (argc == 1) {
                cmd_echo("");
            } else {
                for (int i = 1; i < argc; i++) {
                    if (i > 1) {
                        vga_putchar(' ');
                    }
                    vga_puts(argv[i]);
                }
                vga_puts("\n");
            }
            continue;
        }

        if (k_strcmp(cmd, "threads") == 0) {
            vga_puts_color("  Threads:\n", VGA_LIGHT_CYAN, VGA_BLACK);
            vga_printf("  Current thread index: %d\n", current_thread);
            vga_puts("  TID   PID   STATE   ESP       TICKS   NAME\n");
            vga_puts("  ---------------------------------------\n");

            for (int i = 0; i < MAX_THREADS; i++) {
                if (thread_table[i].state == PROC_UNUSED) {
                    continue;
                }

                const char *state;

                switch (thread_table[i].state) {
                    case PROC_READY:
                        state = "READY";
                        break;

                    case PROC_RUNNING:
                        state = "RUNNING";
                        break;

                    case PROC_BLOCKED:
                        state = "BLOCKED";
                        break;

                    case PROC_ZOMBIE:
                        state = "ZOMBIE";
                        break;

                    default:
                        state = "UNKNOWN";
                        break;
                }

                vga_printf("  %u    %u    %s    %x  %u    %s\n",
                           thread_table[i].tid,
                           thread_table[i].pid,
                           state,
                           thread_table[i].esp,
                           thread_table[i].ticks,
                           thread_table[i].name);
            }

            vga_puts("\n");
            continue;
        }

        if (k_strcmp(cmd, "mutex") == 0) {
            vga_printf("  Mutex test counter: %d\n", mutex_test_counter);

            if (mutex_test_counter == 200000) {
                vga_puts_color("  PASS: final counter is 200000\n",
                               VGA_LIGHT_GREEN, VGA_BLACK);
            } else {
                vga_puts_color("  FAIL: final counter is not 200000\n",
                               VGA_LIGHT_RED, VGA_BLACK);
            }

            vga_puts("\n");
            continue;
        }

        if (k_strcmp(cmd, "semaphore") == 0) {
            vga_printf("  Semaphore produced: %d\n",
                       semaphore_test_produced);
            vga_printf("  Semaphore consumed: %d\n",
                       semaphore_test_consumed);
            vga_printf("  Semaphore errors: %d\n",
                       semaphore_test_errors);

            if (semaphore_test_produced == 100 &&
                semaphore_test_consumed == 100 &&
                semaphore_test_errors == 0) {
                vga_puts_color("  PASS: producer/consumer test succeeded\n",
                               VGA_LIGHT_GREEN, VGA_BLACK);
            } else {
                vga_puts_color("  FAIL: producer/consumer test failed\n",
                               VGA_LIGHT_RED, VGA_BLACK);
            }

            vga_puts("\n");
            continue;
        }

        /* Milestone stubs */
        if (k_strcmp(cmd, "kill") == 0) {
            vga_puts_color("  [TODO] This command is not yet implemented.\n",
                           VGA_YELLOW, VGA_BLACK);
            vga_puts("  Implement it as part of your lecture assignment.\n");
            continue;
        }

        vga_puts_color("  Unknown command: ", VGA_LIGHT_RED, VGA_BLACK);
        vga_puts(cmd);
        vga_puts("\n  Type 'help' for a list of commands.\n");
    }
}

/* ---------------------------------------------------------------------------
 * Kernel entry point – called from kernel_entry.asm
 * --------------------------------------------------------------------------*/
void kernel_main(void) {
    vga_init();
    kb_init();
    pmm_init();

    fs_init();

    proc_init();
    scheduler_init();
    thread_init();

    mutex_init(&mutex_test_lock);
    mutex_test_counter = 0;
    semaphore_init(&semaphore_test_empty, 1);
    semaphore_init(&semaphore_test_full, 0);
    mutex_init(&semaphore_test_lock);
    semaphore_test_buffer = 0;
    semaphore_test_produced = 0;
    semaphore_test_consumed = 0;
    semaphore_test_errors = 0;

    proc_table[0].pid = 0;
    proc_table[0].state = PROC_RUNNING;
    proc_table[0].entry = NULL;
    proc_table[0].name[0] = 'i';
    proc_table[0].name[1] = 'd';
    proc_table[0].name[2] = 'l';
    proc_table[0].name[3] = 'e';
    proc_table[0].name[4] = '\0';

    idt_init();
    pit_init();

    __asm__ __volatile__("sti");

    print_splash();
    shell_run();

    /* Should never reach here */
    __asm__ __volatile__("hlt");
}
