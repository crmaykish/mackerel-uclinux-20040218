#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/config.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/kernel.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/segment.h>
#include <asm/bitops.h>
#include <asm/delay.h>
#include <asm/mackerel.h>
#include <asm/traps.h>

#include "68681serial.h"

#define NUM_PORTS 1
#define CONSOLE_PORT 0

struct tty_driver serial_driver, callout_driver;
static int serial_refcount;

/* serial subtype definitions */
#define SERIAL_TYPE_NORMAL 1
#define SERIAL_TYPE_CALLOUT 2

mackerel_info *console_info;
mackerel_info mackerel_ports[NUM_PORTS];

#define SERIAL_MAGIC 0x5301

static struct tty_struct *mackerel_tty_table[NUM_PORTS];
static struct termios *mackerel_termios[NUM_PORTS];
static struct termios *mackerel_termios_locked[NUM_PORTS];

static int mackerel_write(struct tty_struct *tty, int from_user,
						  const unsigned char *buf, int count)
{
	char c;
	int flags;

	save_flags(flags);
	cli();

	while ((c = *(buf++)) != 0)
	{
		duart_putc(c);
	}

	restore_flags(flags);
	return count;
}

static void mackerel_console_print(const char *p)
{
	char c;

	while ((c = *(p++)) != 0)
	{
		if (c == '\n')
			duart_putc('\r');
		duart_putc(c);
	}

	return;
}

/*
 * This is the serial driver's generic interrupt routine
 */
void mackerel_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned char ch;
	mackerel_info *info = (mackerel_info *)dev_id;
	struct tty_struct *tty = info->tty;

	// Determine the type of interrupt
	unsigned char misr = MEM(DUART_MISR);

	// TODO: do we need this check here now that it's being handled upstream?
	if (misr & DUART_INTR_RXRDY)
	{
		// RX character available
		ch = MEM(DUART_RBB); // Read the available byte. This clears the interrupt

		if (tty)
		{
			if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
				queue_task_irq_off(&tty->flip.tqueue, &tq_timer);
			}
			tty->flip.count++;
			*tty->flip.flag_buf_ptr++ = 0; /* no error */
			*tty->flip.char_buf_ptr++ = ch;
			queue_task_irq_off(&tty->flip.tqueue, &tq_timer);
		}

	}
}

/*
 * This routine is called whenever a serial port is opened.  It
 * enables interrupts for a serial port, linking in its structure into
 * the IRQ chain.   It also performs the serial-specific
 * initialization for the tty structure.
 */
int mackerel_open(struct tty_struct *tty, struct file *filp)
{
	mackerel_info *info;
	int line;

	line = MINOR(tty->device) - tty->driver.minor_start;
	if ((line < 0) || (line >= NUM_PORTS))
		return -ENODEV;
	info = &mackerel_ports[line];

	tty->driver_data = info;
	info->tty = tty;

	return 0;
}

/* write_room function appears to be required, despite docs to the contrary */

int mackerel_write_room(struct tty_struct *tty)
{
	return 256; /* pretend there's lots of space available for writing */
}

static void mackerel_init_port(mackerel_info *info, int port_num)
{
	info->line = port_num;
	info->irq = VEC_INT1 - VEC_SPUR;
}

/* mackerel_init inits the driver, called from tty_init in tty_io.c during system init */
int mackerel_init(void)
{
	int flags;

	printk("XR68C681 driver version 0.1 by Colin Maykish\n");

	/* Initialize the tty_driver structure */
	/* uClinux: Not all of this is exactly right for us. */

	memset(&serial_driver, 0, sizeof(struct tty_driver));
	// serial_driver.magic = TTY_DRIVER_MAGIC;
	serial_driver.name = "ttyS";
	serial_driver.major = TTY_MAJOR;
	serial_driver.minor_start = 64;

#if 0
        /* additional initializations for devfs (not supportet in uclinux 2.0.x) */
        serial_driver.devfs_name = "ttys/";
        serial_driver.driver_name = "68681";
        serial_driver.owner = THIS_MODULE;
#endif

	serial_driver.num = NUM_PORTS;
	serial_driver.type = TTY_DRIVER_TYPE_SERIAL;
	serial_driver.subtype = SERIAL_TYPE_NORMAL;
	serial_driver.init_termios = tty_std_termios;

	serial_driver.init_termios.c_cflag = B57600 | CS8 | CREAD | HUPCL | CLOCAL;
	serial_driver.flags = TTY_DRIVER_REAL_RAW;
	serial_driver.refcount = &serial_refcount;
	serial_driver.table = mackerel_tty_table;				/* ??? */
	serial_driver.termios = mackerel_termios;				/* ??? */
	serial_driver.termios_locked = mackerel_termios_locked; /* ??? */

	serial_driver.open = mackerel_open;				/* required: called when a device is opened */
	serial_driver.write = mackerel_write;			/* required: returns number of characters accepted for writing */
	serial_driver.write_room = mackerel_write_room; /* apparently required */

	/*
	 * The callout device is just like normal device except for
	 * major number and the subtype code.
	 */
	// callout_driver = serial_driver;
	// callout_driver.name = "cua";
	// callout_driver.major = TTYAUX_MAJOR;
	// callout_driver.subtype = SERIAL_TYPE_CALLOUT;

	if (tty_register_driver(&serial_driver))
		panic("Couldn't register serial driver\n");
	// if (tty_register_driver(&callout_driver))
	// 	panic("Couldn't register callout driver\n");

	save_flags(flags);
	cli();

	{
		int i;
		save_flags(flags);
		cli();

		for (i = 0; i < NUM_PORTS; ++i)
		{
			mackerel_info *info = &mackerel_ports[i];

			if (i != CONSOLE_PORT)
			{
				/* console port is allready initialized */
				mackerel_init_port(info, i);
			}

			// Unmask RX interrupt in DUART (leave timer interrupt unmasked)
			MEM(DUART_IMR) = DUART_INTR_COUNTER | DUART_INTR_RXRDY; 

			if (request_irq(VEC_INT1-VEC_SPUR,
							mackerel_interrupt,
							IRQ_FLG_LOCK,
							"XR68C681 console",
							info))
				panic("XR68C681: Unable to attach interrupt\n");

			printk("ttyS%d at 0x%08x (irq = %d)",
				   info->line,
				   (unsigned)info->portaddrs.input, info->irq);
			printk(" is a XR68C681\n");
		}

		restore_flags(flags);
	}

	return 0;
}

extern void register_console(void (*proc)(const char *));

void mackerel_console_initialize(void)
{
	console_info = &mackerel_ports[CONSOLE_PORT];
	mackerel_init_port(console_info, CONSOLE_PORT);

	register_console(mackerel_console_print);

	/* don't request the console interrupt yet, IRQ table hasn't
	   been initialized. */
}
