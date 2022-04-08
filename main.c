/*
 * This file is part of ECT60ctrl (https://github.com/millerfield/ECT60ctrl).
 * Copyright (c) 2022 Stephan Meyer.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <mqueue.h>
#include <pthread.h>
#include <sched.h> /* sched_setscheduler() */
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

/****************************************************************************/

#include "ecrt.h"
#ifdef PIGPIO_OUT
#include "pigpio.h"
#endif
#include "servo_gui.h"
/****************************************************************************/
// Optional features
#define CONFIGURE_PDOS  1
//#define SDO_ACCESS      1

// Timing parameter
#define CYCLE_FREQ 1000
#define CLOCK_SOURCE CLOCK_MONOTONIC
#define CALC_TIMING

/****************************************************************************/

#define NSEC_PER_SEC (1000000000L)
#define PERIOD_NS (NSEC_PER_SEC / CYCLE_FREQ)

#define DIFF_NS(A, B) (((B).tv_sec - (A).tv_sec) * NSEC_PER_SEC + \
        (B).tv_nsec - (A).tv_nsec)

#define TIMESPEC2NS(T) ((uint64_t) (T).tv_sec * NSEC_PER_SEC + (T).tv_nsec)

/****************************************************************************/

// EtherCAT
static ec_master_t *master = NULL;
static ec_domain_t *domain1 = NULL;
static ec_slave_config_t *sc_ECT60_config = NULL;
static t_queue_data queue_data;
static unsigned int digout;
static pthread_mutex_t mymutex;
static pthread_cond_t mycondition;
static mqd_t myqueue;
int long curmessages;

#define MAXMSG 10

/****************************************************************************/

// process data
static uint8_t *domain1_pd = NULL;

#define rtelligentpos    1001, 0

#define Rtelligent_ECT60 0x00000a88, 0x0a880002

// offsets for PDO entries
static unsigned int CiA402_reg6041;
static unsigned int CiA402_reg6061;
static unsigned int CiA402_reg606c;
static unsigned int CiA402_reg60fd;
static unsigned int CiA402_reg6040;
static unsigned int CiA402_reg6083;
static unsigned int CiA402_reg6084;
static unsigned int CiA402_reg60ff;
static unsigned int CiA402_reg6060;
static unsigned int CiA402_reg2006;

const static ec_pdo_entry_reg_t domain1_regs[] = {
    {rtelligentpos,  Rtelligent_ECT60, 0x6041, 0, &CiA402_reg6041},
    {rtelligentpos,  Rtelligent_ECT60, 0x6061, 0, &CiA402_reg6061},
    {rtelligentpos,  Rtelligent_ECT60, 0x606c, 0, &CiA402_reg606c},
    {rtelligentpos,  Rtelligent_ECT60, 0x60fd, 0, &CiA402_reg60fd},
    {rtelligentpos,  Rtelligent_ECT60, 0x6040, 0, &CiA402_reg6040},
    {rtelligentpos,  Rtelligent_ECT60, 0x6083, 0, &CiA402_reg6083},
    {rtelligentpos,  Rtelligent_ECT60, 0x6084, 0, &CiA402_reg6084},
    {rtelligentpos,  Rtelligent_ECT60, 0x60ff, 0, &CiA402_reg60ff},
    {rtelligentpos,  Rtelligent_ECT60, 0x6060, 0, &CiA402_reg6060},
    {rtelligentpos,  Rtelligent_ECT60, 0x2006, 0, &CiA402_reg2006},
    {}
};

static unsigned int counter = 0;
static unsigned int sync_ref_counter = 0;
const struct timespec cycletime = {0, PERIOD_NS};

/*****************************************************************************/

#if CONFIGURE_PDOS

// Cia402 In and Out --------------------------

static ec_pdo_entry_info_t rtelligent_TX_pdo_entries[] = {
    {0x6041, 0, 16},
    {0x6061, 0, 8},
    {0x606c, 0, 32},
    {0x60fd, 0, 32}
};

static ec_pdo_entry_info_t rtelligent_RX_pdo_entries[] = {
    {0x6040, 0, 16},
    {0x6083, 0, 32},
    {0x6084, 0, 32},
    {0x60ff, 0, 32},
    {0x6060, 0, 8},
	{0x2006, 0, 16}
};

static ec_pdo_info_t rtelligent_TX_pdo[] = {
    {0x1a01, sizeof(rtelligent_TX_pdo_entries)/sizeof(rtelligent_TX_pdo_entries[0]), rtelligent_TX_pdo_entries}
};

static ec_pdo_info_t rtelligent_RX_pdo[] = {
    {0x1602, sizeof(rtelligent_RX_pdo_entries)/sizeof(rtelligent_RX_pdo_entries[0]), rtelligent_RX_pdo_entries}
};

static ec_sync_info_t rtelligent_syncs[] = {
    {2, EC_DIR_OUTPUT, 1, rtelligent_RX_pdo},
    {3, EC_DIR_INPUT, 1, rtelligent_TX_pdo},
    {0xff}
};
#endif

/*****************************************************************************/

#if SDO_ACCESS
static ec_sdo_request_t *sdo_2006;
#endif

/*****************************************************************************/

void signal_handler(int signo)
{
  if (signo == SIGINT)
    printf("received SIGINT %d\n", signo);
  else if (signo == SIGTERM)
    printf("received SIGTERM %d\n", signo);

  ncurses_gui_deinit();
  exit(0);
  return;
}

/*****************************************************************************/

struct timespec timespec_add(struct timespec time1, struct timespec time2)
{
    struct timespec result;

    if ((time1.tv_nsec + time2.tv_nsec) >= NSEC_PER_SEC) {
        result.tv_sec = time1.tv_sec + time2.tv_sec + 1;
        result.tv_nsec = time1.tv_nsec + time2.tv_nsec - NSEC_PER_SEC;
    } else {
        result.tv_sec = time1.tv_sec + time2.tv_sec;
        result.tv_nsec = time1.tv_nsec + time2.tv_nsec;
    }

    return result;
}

/*****************************************************************************/

#if SDO_ACCESS
void read_sdo(void)
{
    switch (ecrt_sdo_request_state(sdo_2006)) {
        case EC_REQUEST_UNUSED: // request was not used yet
            ecrt_sdo_request_read(sdo_2006); // trigger first read
            break;
        case EC_REQUEST_BUSY:
            printf("Still busy...\n");
            break;
        case EC_REQUEST_SUCCESS:
            printf("SDO value: 0x%04X\n",
                    EC_READ_U16(ecrt_sdo_request_data(sdo_2006)));
            ecrt_sdo_request_read(sdo_2006); // trigger next read
            break;
        case EC_REQUEST_ERROR:
            printf("Failed to read SDO!\n");
            ecrt_sdo_request_read(sdo_2006); // retry reading
            break;
    }
}
#endif

/****************************************************************************/

void cyclic_task()
{
	int long ret;

    struct timespec wakeupTime, time;
#ifdef CALC_TIMING
    struct timespec startTime, endTime, lastStartTime = {};
    uint32_t period_ns = 0, exec_ns = 0, latency_ns = 0,
             latency_min_ns = 0, latency_max_ns = 0,
             period_min_ns = 0, period_max_ns = 0,
             exec_min_ns = 0, exec_max_ns = 0;
#endif

    // get current time
    clock_gettime(CLOCK_SOURCE, &wakeupTime);

    while(1) {


    	wakeupTime = timespec_add(wakeupTime, cycletime);
        clock_nanosleep(CLOCK_SOURCE, TIMER_ABSTIME, &wakeupTime, NULL);

#ifdef PIGPIO_OUT
        // Enable GPIO14
        gpioWrite(14, 1);
#endif

        // Write application time to master
        //
        // It is a good idea to use the target time (not the measured time) as
        // application time, because it is more stable.
        //
        ecrt_master_application_time(master, TIMESPEC2NS(wakeupTime));


#ifdef CALC_TIMING
        clock_gettime(CLOCK_SOURCE, &startTime);
        latency_ns = DIFF_NS(wakeupTime, startTime);
        period_ns = DIFF_NS(lastStartTime, startTime);
        exec_ns = DIFF_NS(lastStartTime, endTime);
        lastStartTime = startTime;

        if (latency_ns > latency_max_ns) {
            latency_max_ns = latency_ns;
        }
        if (latency_ns < latency_min_ns) {
            latency_min_ns = latency_ns;
        }
        if (period_ns > period_max_ns) {
            period_max_ns = period_ns;
        }
        if (period_ns < period_min_ns) {
            period_min_ns = period_ns;
        }
        if (exec_ns > exec_max_ns) {
            exec_max_ns = exec_ns;
        }
        if (exec_ns < exec_min_ns) {
            exec_min_ns = exec_ns;
        }
#endif

        // receive process data
        ecrt_master_receive(master);
        ecrt_domain_process(domain1);

        // check process data state (optional)

        //************** lock queue ***********************//
        // Lock mutex
        pthread_mutex_lock(&mymutex);
        // Read velocity from ethercat TX-PDO's
        queue_data.velocity = EC_READ_S32((void*)(domain1_pd + CiA402_reg606c));
        // Read setpoint velocity from RX-PDO's
        queue_data.velocity_setpoint = EC_READ_S32((void*)(domain1_pd + CiA402_reg60ff));
        // Read mode of operation from TX-PDO's
        queue_data.mode_of_operation = EC_READ_S8((void*)(domain1_pd + CiA402_reg6061));
        // Send data over message queue to gui thread
        ret = mq_send(myqueue, (const char *)&queue_data, sizeof(t_queue_data)+1, 0);
        {	// read number of current messages in queue
        	struct mq_attr attr;
        	mq_getattr(myqueue, &attr);
        	// And store in global variable shared with gui thread
        	curmessages = attr.mq_curmsgs;
        }
        // Throw signal to cond var waiting thread
        pthread_cond_signal(&mycondition);
        // Unlock mutex
        pthread_mutex_unlock(&mymutex);
        //************** unlock queue ***********************//

        if (counter) {
            counter--;
        } else { // do this at 1 Hz
            counter = CYCLE_FREQ;

            // check for master state (optional)
            //check_master_state(); deleteme

#ifdef CALC_TIMING
            // output timing stats
//            printf("period     %10u ... %10u\n",
//                    period_min_ns, period_max_ns);
//            printf("exec       %10u ... %10u\n",
//                    exec_min_ns, exec_max_ns);
//            printf("latency    %10u ... %10u\n",
//                    latency_min_ns, latency_max_ns);
            period_max_ns = 0;
            period_min_ns = 0xffffffff;
            exec_max_ns = 0;
            exec_min_ns = 0xffffffff;
            latency_max_ns = 0;
            latency_min_ns = 0xffffffff;


#endif

#if SDO_ACCESS
            // read process data SDO
            read_sdo();
#endif
        }

        // write process data
	    EC_WRITE_U16(domain1_pd + CiA402_reg6040, 0x1f);
	    EC_WRITE_U8(domain1_pd + CiA402_reg6060, 0x3);
	    EC_WRITE_S32(domain1_pd + CiA402_reg6083, 0xa000);
	    EC_WRITE_S32(domain1_pd + CiA402_reg6084, 0xa000);
	    EC_WRITE_S32(domain1_pd + CiA402_reg60ff, 0x1000);

	    {
		    digout = EC_READ_U16((void*)(domain1_pd  + CiA402_reg2006));
	    }


        if (sync_ref_counter) {
            sync_ref_counter--;
        } else {
            sync_ref_counter = 1; // sync every cycle

            clock_gettime(CLOCK_SOURCE, &time);
            ecrt_master_sync_reference_clock_to(master, TIMESPEC2NS(time));
        }
        ecrt_master_sync_slave_clocks(master);

        // send process data
        ecrt_domain_queue(domain1);
        ecrt_master_send(master);

#ifdef CALC_TIMING
        clock_gettime(CLOCK_SOURCE, &endTime);
#endif
#ifdef PIGPIO_OUT
        // Disable GPIO14
        gpioWrite(14, 0);
#endif


    }
}

/****************************************************************************/

/****************************************************************************/

int main(int argc, char **argv)
{
	// open a message queue for communication between threads
	struct mq_attr attr = {.mq_flags = 0, .mq_maxmsg = MAXMSG, .mq_msgsize = sizeof(t_queue_data)+1, .mq_curmsgs = 0};
#define myqueue_name "/myqueue"
	myqueue = mq_open(myqueue_name, O_WRONLY | O_CREAT | O_NONBLOCK, 0660, &attr);
	// Init the mutex for safe interthread communication
	pthread_mutex_init(&mymutex, NULL);
	pthread_cond_init(&mycondition, NULL);
#ifdef PIGPIO_OUT
	int pigpio_version;
#endif

	if ((signal(SIGINT, signal_handler) == SIG_ERR) || (signal(SIGTERM, signal_handler) == SIG_ERR))
	{
		perror("signal handler registration failed");
	}

    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        perror("mlockall failed");
        return -1;
    }

#ifdef PIGPIO_OUT
    pigpio_version = gpioInitialise(); 			// Initialise pigpio
    if(pigpio_version < 0)
    {
    	printf("pigpio could not be initialised. Maybe library not installed or pigpio.service is running.\n");
    }
    else
    {
    	printf("pigpio with version %d initialised\n", pigpio_version);
    	gpioSetMode( 14, PI_OUTPUT);					// Set GPIO14 to output
    }
#endif

    master = ecrt_request_master(0);
    if (!master)
        return -1;

    domain1 = ecrt_master_create_domain(master);
    if (!domain1)
        return -1;

    if (!(sc_ECT60_config = ecrt_master_slave_config(master,
                    rtelligentpos, Rtelligent_ECT60))) {
        fprintf(stderr, "Failed to get slave configuration.\n");
        return -1;
    }

    if (ecrt_slave_config_pdos(sc_ECT60_config, EC_END, rtelligent_syncs)) {
        fprintf(stderr, "Failed to configure PDOs.\n");
        return -1;
    }

#if SDO_ACCESS
    printf("Creating SDO requests...\n");
    if (!(sdo_2006 = ecrt_slave_config_create_sdo_request(sc_ECT60_config, 0x2006, 0, 2))) {
        printf("Failed to create SDO request.\n");
    }
    ecrt_sdo_request_timeout(sdo_2006, 500); // ms
#endif

    printf("Registering PDO entries...\n");
    if (ecrt_domain_reg_pdo_entry_list(domain1, domain1_regs)) {
        fprintf(stderr, "PDO entry registration failed!\n");
        return -1;
    }

    // configure SYNC signals for this slave
    ecrt_slave_config_dc(sc_ECT60_config, 0x0700, PERIOD_NS, 4400000, 0, 0);


    printf("Activating master...\n");
    if (ecrt_master_activate(master))
        return -1;

    if (!(domain1_pd = ecrt_domain_data(domain1))) {
        return -1;
    }

    /* Call ncurses gui thread */
    ncurses_gui_init(master, domain1, domain1_pd, &mymutex, &mycondition);
    /* Set priority */

    struct sched_param param = {};
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);

    printf("Using priority %i.\n", param.sched_priority);
    if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
        perror("sched_setscheduler failed\n");
    }

    // Start cyclic ethercat communication within main context
    printf("Starting cyclic function.\n");
    cyclic_task();

    // After task ends, cleanup
    ecrt_release_master(master);

#ifdef PIGPIO_OUT
	gpioTerminate();
#endif

    // Destroy mutex
    pthread_mutex_destroy(&mymutex);
    pthread_cond_destroy(&mycondition);
    // Remove queue
    mq_close(myqueue);
    return 0;
}

/****************************************************************************/
