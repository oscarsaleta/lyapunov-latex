#ifndef ANTZ_ERRCODES_H
#define ANTZ_ERRCODES_H
/*! \file errcodes.h
 * \brief Header for error codes in PBala
 * \author Oscar Saleta Reig
 */

/* ERROR CODES FOR PBala.c MAIN RETURN VALUES */
#define E_ARGS 10
#define E_NODE_LINES 11
#define E_NODE_OPEN 12
#define E_NODE_READ 13
#define E_CWD 14
#define E_PVM_MYTID 15
#define E_PVM_PARENT 16
#define E_DATAFILE_LINES 17
#define E_OUTFILE_OPEN 18
#define E_PVM_SPAWN 19
#define E_DATAFILE_FIRSTCOL 20
#define E_OUTDIR 21
#define E_WRONG_TASK 22

/* ERROR CODES FOR task.c SLAVE RETURN STATUS */
#define ST_FORK_ERR 10
#define ST_TASK_KILLED 11

#endif /* ANTZ_ERRCODES_H */