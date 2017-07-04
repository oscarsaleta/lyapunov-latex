/*! \file antz_lib.c 
 * \brief File with generic functions and constant definitions
 * \author Oscar Saleta Reig
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <pvm3.h>
#include "antz_lib.h"

/**
 * Count how many lines a textfile has
 *
 * \param[in] *fileName name of file to be counted
 *
 * \return int lineCount
 */
int getLineCount(char *fileName) {
    FILE *f_aux;
    char str_tmp[BUFFER_SIZE];
    int lineCount;

    sprintf(str_tmp,"wc -l %s",fileName);
    f_aux = popen(str_tmp,"r");
    if (f_aux == NULL) 
        return -1;
    fgets(str_tmp,1024,f_aux);
    sscanf(str_tmp,"%d",&lineCount);
    fclose(f_aux);

    return lineCount;
}

/**
 * Parse the node file and return an array of strings containing the names
 * of the nodes, and an array of ints containing their number of CPUs
 *
 * \param[in] *nodefile name of node file
 * \param[in] nNodes number of nodes
 * \param[in] ***nodes pointer to array of strings of node names
 * \param[in] **nodeCores pointer to array of ints of CPUs per node
 *
 * \returns 0 if successful, 1 if error opening file, 2 if error reading
 */
int parseNodefile(char *nodefile, int nNodes, char ***nodes, int **nodeCores) {
    int i;
    FILE *f_nodes;
    // Open node file
    f_nodes = fopen(nodefile,"r");
    if (f_nodes == NULL) {
        return 1;
    }
    // Allocate memory for node and cpu array
    *nodes = (char**)malloc(nNodes*sizeof(char*));
    for (i=0; i<nNodes; i++)
        (*nodes)[i] = (char*)malloc(MAX_NODE_LENGTH*sizeof(char));
    *nodeCores = (int*)malloc(nNodes*sizeof(int));
    // Read node file and store the info
    for (i=0; i<nNodes; i++) {
        if (fscanf(f_nodes,"%s %d",(*nodes)[i],&(*nodeCores)[i])!=2) {
            return 2;
        }
    }
    fclose(f_nodes);
    return 0;
}

        

/**
 * Memory check for tasks. If no guess is given, limit to 25% of max RAM
 *
 * \param[in] flag Tells the function if there is a guess by the user
 * \param[in] max_task_size If flag!=0 use this as constraint to memory
 *
 * \returns 0 if there is enough memory, 1 if there is not enough memory
 */
int memcheck(int flag, long int max_task_size) {
    FILE *f;
    char buffer[1024];
    char *token;
    int maxmem,freemem;

    f = fopen("/proc/meminfo","r");
    fgets(buffer,1024,f); // reads total memory
    token = strtok(buffer," ");
    token = strtok(NULL," ");
    sscanf(token,"%d",&maxmem);
    fgets(buffer,1024,f); // reads free memory
    token = strtok(buffer," ");
    token = strtok(NULL," ");
    sscanf(token,"%d",&freemem);
    fclose(f);
    if (flag==0) {
        if (freemem < 0.15*maxmem)
            return 1;
    } else {
        if (freemem < max_task_size)
            return 1;
    }
    return 0;
}


/**
 * Print usage information to output file
 *
 * \param[in] pid Proccess identifier (within system)
 * \param[in] taskNumber Task identifier (within our program)
 * \param[in] usage Struct that contains all the resource usage information
 * \param[in] out_dir Output file name
 *
 * \returns 0 if successful
 */
int prtusage(int pid, int taskNumber, char *out_dir, struct rusage usage) {
    FILE *memlog;
    char memlogfilename[FNAME_SIZE];
    
    sprintf(memlogfilename,"%s/task%d_mem.txt",out_dir,taskNumber);
    memlog = fopen(memlogfilename,"w");
    fprintf(memlog,"TASK %d RESOURCE USAGE (PID %d)\n",taskNumber,pid);
    fprintf(memlog,"----------------------\n");
    fprintf(memlog,"User CPU time used:               %20.10g\n",
                usage.ru_utime.tv_sec+usage.ru_utime.tv_usec/1e6);
    fprintf(memlog,"System CPU time used:             %20.10g\n",
                usage.ru_stime.tv_sec+usage.ru_stime.tv_usec/1e6);
    fprintf(memlog,"Maximum resident set size (KB):   %20ld\n",
                usage.ru_maxrss);
    fprintf(memlog,"Integral shared memory size (KB): %20ld\n",
                usage.ru_ixrss);
    fprintf(memlog,"Integral unshared data size (KB)  %20ld\n",
                usage.ru_idrss);
    fprintf(memlog,"Integral shared stack size (KB):  %20ld\n",
                usage.ru_isrss);
    fprintf(memlog,"Page reclaims (soft page faults): %20ld\n",
                usage.ru_minflt);
    fprintf(memlog,"Page faults (hard page faults):   %20ld\n",
                usage.ru_majflt);
    fprintf(memlog,"Swaps:                            %20ld\n",
                usage.ru_nswap);
    fprintf(memlog,"Block input operations:           %20ld\n",
                usage.ru_inblock);
    fprintf(memlog,"block output operations:          %20ld\n",
                usage.ru_oublock);
    fprintf(memlog,"IPC messages sent:                %20ld\n",
                usage.ru_msgsnd);
    fprintf(memlog,"IPC messages received:            %20ld\n",
                usage.ru_msgrcv);
    fprintf(memlog,"Signals received:                 %20ld\n",
                usage.ru_nsignals);
    fprintf(memlog,"Voluntary context switches:       %20ld\n",
                usage.ru_nvcsw);
    fprintf(memlog,"Involuntary context switches:     %20ld\n",
                usage.ru_nivcsw);
    fclose(memlog);

    return 0;
}


/**
 * Creates an auxiliary program for PARI execution
 *
 * \param[in] taskId Task number
 * \param[in] args Arguments string separated by commas
 * \param[in] programfile path to PARI script
 * \param[in] directory path to results directory
 *
 * \returns 0 if successful
 */
int parifile(int taskId, char *args, char *programfile, char *directory) {
    FILE *f;
    char aux[FNAME_SIZE];

    sprintf(aux,"%s/auxprog-%d.gp",directory,taskId);
    f = fopen(aux,"w");
    fprintf(f,"taskId=%d;\n",taskId);
    fprintf(f,"taskArgs=[%s];\n",args);
    fprintf(f,"\\r %s\n",programfile);
    fprintf(f,"\\q\n");

    fclose(f);
    return 0;
}


/**
 * Creates an auxiliary program for SAGE execution
 *
 * \param[in] taskId Task number
 * \param[in] args Arguments string separated by commas
 * \param[in] programfile path to script
 * \param[in] directory path to results directory
 *
 * \returns 0 if successful
 */
int sagefile(int taskId, char *args, char *programfile, char *directory) {
    FILE *f;
    char aux[FNAME_SIZE];

    sprintf(aux,"%s/auxprog-%d.sage",directory,taskId);
    f = fopen(aux,"w");
    fprintf(f,"taskId=%d;\n",taskId);
    fprintf(f,"taskArgs=[%s];\n",args);
    fprintf(f,"load('%s')\n",programfile);

    fclose(f);
    return 0;
}


/**
 * Informs that a process has been killed or stopped
 * this function runs instead of prtusage()
 *
 * \param[in] pid Proccess identifier (within system)
 * \param[in] taskNumber Task identifier (within our program)
 * \param[in] out_dir Output file name
 *
 * \returns 0 if successful
 */
int prterror (int pid, int taskNumber, char *out_dir, double time) {
    FILE *memlog;
    char memlogfname[FNAME_SIZE];

    sprintf(memlogfname,"%s/task%d_killed.log",out_dir,taskNumber);
    memlog = fopen(memlogfname,"w");
    fprintf(memlog,"TASK %d (PID %d) ERROR REPORT\n",taskNumber,pid);
    fprintf(memlog,"----------------------\n");
    fprintf(memlog,"Task was killed or stopped after %10.5G seconds.\n",time);
    
    fclose(memlog);
    return 0;
}
