#ifndef ANTZ_LIB_H
#define ANTZ_LIB_H
/*! \file antz_lib.h 
 * \brief Header for antz_lib.c
 * \author Oscar Saleta Reig
 */

//#define VERSION "4.0.1"
#define PVM_ENCODING PvmDataRaw ///< Little Endian encoding
#define MAX_NODE_LENGTH 6 ///< Max length of node names (a0X)
#define FNAME_SIZE 150 ///< Max length of filename (including path)
#define BUFFER_SIZE 2048 ///< Max size of buffer for reading files
#define MSG_GREETING 1 ///< Flag for initializing task
#define MSG_WORK 2 ///< Flag for telling task to do work
#define MSG_RESULT 3 ///< Flag that indicates that message contains results
#define MSG_STOP 4 ///< Flag for stopping a task

extern struct rusage;

int getLineCount(char *inp_dataFile);
int parseNodefile(char *nodefile, int nNodes, char ***nodes, int **nodeCores);
int memcheck(int flag, long int max_task_size);
int prtusage(int pid, int taskNumber, char *out_dir, struct rusage usage);
int parifile(int taskId, char *args, char *programfile, char *directory);
int sagefile(int taskId, char *args, char *programfile, char *directory);
int prterror (int pid, int taskNumber, char *out_dir, double time);

#endif /* ANTZ_LIB_H */
