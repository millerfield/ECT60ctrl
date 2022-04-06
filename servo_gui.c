/* 
 * This file is part of the XXX distribution (https://github.com/xxxx or http://xxx.github.io).
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


#include <unistd.h>
#include "curses.h"
#include <pthread.h>
#include <mqueue.h>
#include "ecrt.h"
#include <errno.h>
#include "servo_gui.h"
#ifdef PIGPIO_OUT
#include "pigpio.h"
#endif

#define NCURSES_GUI
//#define MSGSIZE 5



#ifdef NCURSES_GUI

static ec_master_state_t master_state = {};
static ec_domain_state_t domain_state = {};
static ec_master_t *master = NULL;
static ec_domain_t *domain = NULL;
static uint8_t *domain_pd = NULL;
static pthread_mutex_t* mymutex;
static mqd_t myqueue;
static pthread_cond_t* mycondition;
extern long int curmessages;
WINDOW *win;

void print_master_state(WINDOW* win, int curs_y, int curs_x)
{
    ec_master_state_t ms;

    // Read actual master state
    ecrt_master_state(master, &ms);

    // Compare states with state from n-1 request and print if changed
    if (ms.slaves_responding != master_state.slaves_responding)
        mvwprintw(win, curs_y, curs_x, "%u slave(s).", ms.slaves_responding);
    if (ms.al_states != master_state.al_states)
    	mvwprintw(win, curs_y + 1, curs_x, "AL states: 0x%02X.", ms.al_states);
    if (ms.link_up != master_state.link_up)
    	mvwprintw(win, curs_y + 2, curs_x, "Link is %s.", ms.link_up ? "up" : "down");

    // Store state persistent
    master_state = ms;
}

void print_domain1_state(WINDOW* win, int curs_y, int curs_x)
{
    ec_domain_state_t ds;

    ecrt_domain_state(domain, &ds);

    if (ds.working_counter != domain_state.working_counter)
        mvwprintw(win, curs_y, curs_x, "Domain1: WC %u.", ds.working_counter);
    if (ds.wc_state != domain_state.wc_state)
    	mvwprintw(win, curs_y + 1, curs_x, "Domain1: State %u.", ds.wc_state);

    domain_state = ds;
}


void* ncurses_gui(void* arg)
{
	int ymax, xmax;
	//int long velocity;
	struct mq_attr attr;
	signed char mode_of_operation;
	t_queue_data queue_data;


    struct sched_param param = {};
    param.sched_priority = sched_get_priority_max(SCHED_FIFO)-1;

    printf("Using priority %i.\n", param.sched_priority);
    if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
        perror("sched_setscheduler failed\n");
    }

	initscr();
	noecho();
	curs_set(0);

	getmaxyx(stdscr, ymax, xmax);

	win = newwin(ymax-2, xmax-2, 1, 1);
	refresh();
	box(win, 0, 0);

	while(1)
	{
		int i;

#ifdef PIGPIO_OUT
                // Enable GPIO15
                gpioWrite(15, 1);
#endif
		//************** lock queue ***********************//
		// Lock mutex
		pthread_mutex_lock(mymutex);
		// as long as queue is empty, go to conditional wait for signal from main thread with mutex unlocked
        while (curmessages == 0) {
#ifdef PIGPIO_OUT
        		// Disable GPIO15
        		gpioWrite(15, 0);
#endif
        		// No new messages in queue, so wait until condition is signaled
                pthread_cond_wait(mycondition, mymutex);
#ifdef PIGPIO_OUT
                // Enable GPIO15
                gpioWrite(15, 1);
#endif
            }
       	// Comming here if queue is not empty triggered by signal from main thread
        mq_receive(myqueue, (char *)&queue_data, sizeof(t_queue_data)+1, 0);
        // Get actual number of messages from queue
        mq_getattr(myqueue, &attr);
        // and store into the shared variable
       	curmessages = attr.mq_curmsgs;
       	// Unlock mutex
       	pthread_mutex_unlock(mymutex);
        //************** unlock queue ***********************//
#ifdef PIGPIO_OUT
        		// Disable GPIO15
        		gpioWrite(15, 0);
#endif

		// print out latest process data
		//print_master_state(win, 12, 10);
		//print_domain1_state(win, 15, 10);
		//mode_of_operation = EC_READ_S8((void*)(domain_pd + CiA402_reg6061));
//		mvwprintw(win, 11, 10, "Mode of operation: %d", mode_of_operation);
    	mvwprintw(win, 10, 10, "Expected velocity: %ld", queue_data.velocity_setpoint);
    	mvwprintw(win, 11, 10, "Actual velocity: %ld", queue_data.velocity);
    	mvwprintw(win, 12, 10, "Variance: %ld", queue_data.velocity);
		wrefresh(win);
	}
	endwin();

	return NULL;
}

void ncurses_gui_init(ec_master_t* pmaster, ec_domain_t* pdomain, uint8_t *pdomain_pd, pthread_mutex_t* pmutex, pthread_cond_t* pcondition)
{
	pthread_t ncurses_thread_id;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 200000);	// TODO deleteme


	// Assign pointer to master and domain persistent
	master = pmaster;
	domain = pdomain;
	domain_pd = pdomain_pd;
	mymutex = pmutex;
	mycondition = pcondition;
#define myqueue_name "/myqueue"
	// open the message queue for communication between threads
	myqueue = mq_open(myqueue_name, O_RDONLY);

	// Create a new thread which handles the ncurses GUI
    pthread_create(&ncurses_thread_id, &attr, &ncurses_gui, NULL);
}


void ncurses_gui_deinit(void)
{
	endwin();
}


#endif

