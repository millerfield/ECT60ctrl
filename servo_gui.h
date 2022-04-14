/*
 * servo_gui.h
 *
 *  Created on: 07.02.2022
 *      Author: guest
 */

#ifndef EXAMPLES_DC_RTELLIGENT_SERVO_GUI_H_
#define EXAMPLES_DC_RTELLIGENT_SERVO_GUI_H_

// Data type send from cyclic_task via queue to ncurses_gui task
typedef struct
{
	int long velocity;
	char mode_of_operation;
}txpdo_queue_data_t;

// Data type send from ncurses_gui task to cyclic_task (no queue)
typedef struct
{
	int long velocity_setpoint;
}rxpdo_queue_data_t;

void ncurses_gui_thread(ec_master_t*, ec_domain_t*, uint8_t *, pthread_mutex_t*, pthread_cond_t*);
void ncurses_gui_reinit(void);
void ncurses_gui_deinit(void);

#endif /* EXAMPLES_DC_RTELLIGENT_SERVO_GUI_H_ */
