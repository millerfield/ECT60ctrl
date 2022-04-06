/*
 * servo_gui.h
 *
 *  Created on: 07.02.2022
 *      Author: guest
 */

#ifndef EXAMPLES_DC_RTELLIGENT_SERVO_GUI_H_
#define EXAMPLES_DC_RTELLIGENT_SERVO_GUI_H_

typedef struct
{
	int long velocity;
	int long velocity_setpoint;
}t_queue_data;

void ncurses_gui_init(ec_master_t*, ec_domain_t*, uint8_t *, pthread_mutex_t*, pthread_cond_t*);

void ncurses_gui_deinit(void);

#endif /* EXAMPLES_DC_RTELLIGENT_SERVO_GUI_H_ */
