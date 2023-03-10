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


#include <stdbool.h>
#include <unistd.h>
#include <locale.h>
#include <curses.h>
#include <pthread.h>
#include <mqueue.h>
#include <ecrt.h>
#include <errno.h>
#include "servo_gui.h"
#ifdef PIGPIO_OUT
#include "pigpio.h"
#endif

#define NCURSES_GUI



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
extern rxpdo_queue_data_t rxpdo_queue_data;
extern bool winch_required;
WINDOW *win_ethcat, *win_cia402, *win_params;

void print_master_state(WINDOW* win)
{
    ec_master_state_t ms;
    ec_domain_state_t ds;

    // Read actual master state
    ecrt_master_state(master, &ms);
    // Read actual domain
    ecrt_domain_state(domain, &ds);


    // Compare states with state from n-1 request and print if changed
    if (ms.slaves_responding != master_state.slaves_responding)
        mvwprintw(win, 1, 2, "%u slave(s).", ms.slaves_responding);
    if (ms.al_states != master_state.al_states)
    	mvwprintw(win, 2, 2, "AL states: 0x%02X.", ms.al_states);
    if (ms.link_up != master_state.link_up)
    	mvwprintw(win, 3, 2, "Link is %s.", ms.link_up ? "up" : "down");
    if (ds.working_counter != domain_state.working_counter)
        mvwprintw(win, 4, 2, "Domain1: WC %u.", ds.working_counter);
    if (ds.wc_state != domain_state.wc_state)
    	mvwprintw(win, 5, 2, "Domain1: State %u.", ds.wc_state);

    // Store states persistent
    master_state = ms;
    domain_state = ds;
}


void dialog_cia402(WINDOW* win, txpdo_queue_data_t* ptxpdo, rxpdo_queue_data_t* prxpdo)
{
	mvwprintw(win_cia402, 1, 2, "Expected velocity: %7ld", prxpdo->velocity_setpoint);
	mvwprintw(win_cia402, 2, 2, "Actual velocity: %7ld", ptxpdo->velocity);
	mvwprintw(win_cia402, 3, 2, "Variance: %7ld", ptxpdo->velocity);
	mvwprintw(win_cia402, 4, 2, "Mode of operation: %1d", ptxpdo->mode_of_operation);

}

void dialog_parameters(WINDOW* win)
{

}

// Function for exchanging data with the real time cyclic_task of ethercat
void exchange_data(txpdo_queue_data_t* p_txdata, rxpdo_queue_data_t* p_rxdata)
{
	struct mq_attr attr;

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
        // Receive RX PDO's via queue
        mq_receive(myqueue, (char *)p_txdata, sizeof(txpdo_queue_data_t)+1, 0);
        // Get actual number of messages from queue
        mq_getattr(myqueue, &attr);
        // and store into the shared variable
       	curmessages = attr.mq_curmsgs;
       	// Write TX PDO's via global
       	rxpdo_queue_data.velocity_setpoint = p_rxdata->velocity_setpoint;
       	// Unlock mutex
       	pthread_mutex_unlock(mymutex);
        //************** unlock queue ***********************//
#ifdef PIGPIO_OUT
        		// Disable GPIO15
        		gpioWrite(15, 0);
#endif

}

// This function is not allowed to contain a blocking call except exchange_data()
void* ncurses_gui(void* arg)
{
	int keypressed;
	txpdo_queue_data_t txpdo_data = {0};
	rxpdo_queue_data_t rxpdo_data = {0};
    struct sched_param param = {};
    // The scheduler priority of this thread is set to the highest possible -1.
    param.sched_priority = sched_get_priority_max(SCHED_FIFO)-1;

    printf("Using priority %i.\n", param.sched_priority);
    if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
        perror("sched_setscheduler failed\n");
    }

    ncurses_gui_reinit();


	while(1)
	{
		// Exchange data with the ethercat realtime thread. This is synched with a condition from the real time thread and is a blocking call.
		exchange_data(&txpdo_data, &rxpdo_data);

		// Get char from keyboard buffer non blocking
		keypressed = wgetch(win_ethcat);

        if(keypressed == KEY_UP)
        {
        	rxpdo_data.velocity_setpoint += 1000;
        }
        else if(keypressed == KEY_DOWN)
        {
        	rxpdo_data.velocity_setpoint -= 1000;
        }


        if(winch_required == true)
        {
        	ncurses_gui_reinit();
        }
		// print out latest process data
		print_master_state(win_ethcat);
		dialog_cia402(win_cia402, &txpdo_data, &rxpdo_data);
		dialog_parameters(win_params);
		wrefresh(win_ethcat);
		wrefresh(win_cia402);
		wrefresh(win_params);
	}
	endwin();

	return NULL;
}

void ncurses_gui_thread(ec_master_t* pmaster, ec_domain_t* pdomain, uint8_t *pdomain_pd, pthread_mutex_t* pmutex, pthread_cond_t* pcondition)
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

// The function is called initializing the ncurses windows after start or resizing the terminal.
// It must NOT be called in lock situation within function exchange_data
void ncurses_gui_reinit(void)
{
	int ymax, xmax;

	endwin();

	setlocale(LC_ALL, "");

	initscr();
	cbreak();
	noecho();
	curs_set(0);

	if(!has_colors())
	{
		printf("terminal does not support colors");
	}
	start_color();

	getmaxyx(stdscr, ymax, xmax);

	win_ethcat = newwin((ymax/2)-2, (xmax/2)-2, 1, 1);
	win_cia402 = newwin((ymax/2)-2, (xmax/2)-2, 1, xmax/2);
	win_params = newwin((ymax/2)-2, (xmax)-3, ymax/2, 1);
	// Set for getch() non blocking
	if (nodelay (win_ethcat, true) == ERR) {
	    // some error occurred.
	}
	if (nodelay (win_cia402, true) == ERR) {
	    // some error occurred.
	}
	// Turn arrow keys on
	keypad(win_ethcat, true);
	keypad(win_cia402, true);
	init_pair(1, COLOR_BLUE, COLOR_WHITE);
	box(win_ethcat, 0, 0);
	box(win_cia402, 0, 0);
	box(win_params, 0, 0);
	wattron(win_ethcat, A_STANDOUT | A_BOLD | COLOR_PAIR(1));
	mvwprintw(win_ethcat, 0, 1, "EtherCAT");
	wattroff(win_ethcat, A_STANDOUT | A_BOLD | COLOR_PAIR(1));
	wattron(win_cia402, A_STANDOUT | A_BOLD | COLOR_PAIR(1));
	mvwprintw(win_cia402, 0, 1, "CIA402 (PDO's)");
	wattroff(win_cia402, A_STANDOUT | A_BOLD | COLOR_PAIR(1));
	wattron(win_params, A_STANDOUT | A_BOLD | COLOR_PAIR(1));
	mvwprintw(win_params, 0, 1, "Parameter (SDO's)");
	wattroff(win_params, A_STANDOUT | A_BOLD | COLOR_PAIR(1));
	wrefresh(win_ethcat);
	wrefresh(win_cia402);
	wrefresh(win_params);

	timeout(0);
	winch_required = false;
}

void ncurses_gui_deinit(void)
{
    // Remove queue
    mq_close(myqueue);
    // Close ncurses window
    endwin();
}


#endif

