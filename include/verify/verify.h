#pragma once

#include <stdio.h>				/* sprintf */
#include <stdint.h>				/* int64_t */
#include <assert.h>				/* assert */
#include <limits.h>             /* max */
#include <math.h>               /* ceil */

#include "../compat.h"
#include "../edge_struct.h"		/*	packed_edge	*/
#include "../xalloc.h"			/*	xmalloc	*/
#include "../options.h"			/* VERBOSE */
#include "../opencl.h"

#define OPTIMAL_VERIFY_MOD 32
#define VERIFY_ITEMS_BY_GROUP 32

#ifdef _WIN32
    #define VERIFY_OMP(x) __pragma(x)
#else
    #define VERIFY_OMP(x) _Pragma(#x)
#endif

#define VERIFY_DEVICE_TYPE CL_DEVICE_TYPE_ALL

/*	=============== Functions ===============	*/

/*!
 * \fn int64_t verify_bfs_tree (int64_t*, int64_t ,int64_t , const packed_edge*, int64_t)
 * \brief Verifi qu'un parcours en largeur est correcte.
 * \param int64_t* : La liste des parent de chaque sommet apres le bfs
 * \param int64_t :
 * \param int64_t
 * \param
 * \param
 * \return
 */
int64_t verify_bfs_tree (int64_t*, int64_t ,int64_t , const packed_edge*, int64_t);
