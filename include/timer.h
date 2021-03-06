#pragma once

/*	=============== Includes ===============	*/

#include "compat.h"
#include <time.h>       /*	timespec	*/
#ifdef _WIN32
#include <windows.h>	/*	timeval	*/
#endif

/*	=============== Functions ===============	*/

/*!
 * \fn void tic(void)
 * \brief enregistre le temp des l'instant ou l'on appelle cette fonction
 */
void tic(void);
/*!
 * \fn void tic(void)
 * \brief enregistre le temp des l'instant ou l'on appelle cette fonction et le soustrait au temp precedant
 * \return le temp total
 */
double toc(void);

/*	=============== Defines ===============	*/

#define TIME(timevar, what) do { tic (); what; timevar = toc(); } while (0)
