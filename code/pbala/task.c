/* Job parallelizer in PVM for SPMD executions in antz computing server
 * URL: https://github.com/oscarsaleta/PVMantz
 *
 * Copyright (C) 2016  Oscar Saleta Reig
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*! \file task.c 
 * \brief PVM task. Adapts to different programs, forks execution to track mem usage
 * \author Oscar Saleta Reig
 */
#include "antz_errcodes.h"
#include "antz_lib.h"

#include <config.h>

#include <fcntl.h>
#include <pvm3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>


/**
 * Main task function.
 *
 * \return 0 if successful
 */
int main(int argc, char *argv[]) {
    int myparent,taskNumber; // myparent is the master
    int me; // me is the PVM id of this child
    // work_code is a flag that tells the child what to do
    int work_code;
    char inp_programFile[FNAME_SIZE]; // name of the program
    char out_dir[FNAME_SIZE]; // name of output directory
    char arguments[BUFFER_SIZE]; // string of arguments as read from data file
    int task_type; // 0:maple, 1:C, 2:python
    long int max_task_size; // if given, max size in KB of a spawned process
    int flag_err; // 0=no err files, 1=yes err files
    int flag_mem; // 0=no mem files, 1=yes mem files
    int i; // for loops
    clock_t initt,endt;
    double difft, totalt=0;
    int state;

    myparent = pvm_parent();


    // Be greeted by master
    pvm_recv(myparent,MSG_GREETING);
    pvm_upkint(&me,1,1);
    pvm_upkint(&task_type,1,1);
    pvm_upklong(&max_task_size,1,1);
    pvm_upkint(&flag_err,1,1);
    pvm_upkint(&flag_mem,1,1);


    /* Perform generic check or use specific size info?
     *  memcheck_flag = 0 means generic check
     *  memcheck_flag = 1 means specific info
     */
    int memcheck_flag = max_task_size > 0 ? 1 : 0;

    // Work work work
    while (1) {
        /* Race condition. Mitigated by executing few CPUs on each node
         * Explanation: 2 tasks could check memory simultaneously and
         *  both conclude that there is enough because they see the same
         *  output, but maybe there is not enough memory for 2 tasks.
         */
        if (memcheck(memcheck_flag,max_task_size) == 1) {
            sleep(60); // arbitrary number that could be much lower
            continue;
        }
        // Receive inputs
        pvm_recv(myparent,MSG_WORK);
        pvm_upkint(&work_code,1,1);
        if (work_code == MSG_STOP) // if master tells task to shutdown
            break;
        pvm_upkint(&taskNumber,1,1);
        pvm_upkstr(inp_programFile);
        pvm_upkstr(out_dir);
        pvm_upkstr(arguments); // string of comma-separated arguments read from datafile

        time(&initt);
        /* Fork one process that will do the execution
         * the "parent task" will only wait for this process to end
         * and then report resource usage via getrusage()
         */
        pid_t pid = fork();
        // If fork fails, notify master and exit
        if (pid<0) {
            fprintf(stderr,"ERROR - task %d could not spawn execution process\n",taskNumber);
            int state=ST_FORK_ERR;
            pvm_initsend(PVM_ENCODING);
            pvm_pkint(&me,1,1);
            pvm_pkint(&taskNumber,1,1);
            pvm_pkint(&state,1,1);
            pvm_pkstr(arguments);
            pvm_pkdouble(&totalt,1,1);
            pvm_send(myparent,MSG_RESULT);
            pvm_exit();
            exit(1);
        }
        //Child code (work done here)
        if (pid == 0) {
            char output_file[BUFFER_SIZE];
            int err; // for executions

            // Move stdout to taskNumber_out.txt
            sprintf(output_file,"%s/task%d_stdout.txt",out_dir,taskNumber);
            int fd = open(output_file,O_WRONLY|O_CREAT|O_TRUNC,0666);
            dup2(fd,1);
            close(fd);

            // Move stderr to taskNumber_err.txt
            if (flag_err) {
                sprintf(output_file,"%s/task%d_stderr.txt",out_dir,taskNumber);
                fd = open(output_file,O_WRONLY|O_CREAT|O_TRUNC,0666);
                dup2(fd,2);
                close(fd);
            }

            /*
             * GENERATE EXECUTION OF PROGRAM
             */
            /* MAPLE */
            if (task_type == 0) {
                // NULL-terminated array of strings for calling the Maple script
                char **args;
                // 0: maple, 1: taskid, 2: X, 3: input, 4: NULL
                int nargs=4;
                args = (char**)malloc((nargs+1)*sizeof(char*));
                // Do not malloc for NULL
                for (i=0;i<nargs;i++)
                    args[i] = malloc(BUFFER_SIZE);
                // Fill up the array with strings
                sprintf(args[0],"maple");
                sprintf(args[1],"-tc \"taskId:=%d\"",taskNumber);
                sprintf(args[2],"-c \"taskArgs:=[%s]\"",arguments);
                sprintf(args[3],"%s",inp_programFile);
                args[4] = NULL;

                // Call the execution and check for errors
                err = execvp(args[0],args);
                perror("ERROR:: child Maple process");
                exit(err);

            /* C */
            } else if (task_type == 1) {
                // Preparations for C or Python execution 
                char *arguments_cpy;
                arguments_cpy=malloc(strlen(arguments));
                strcpy(arguments_cpy,arguments);
                // This counts how many commas there are in arguments, giving the number
                // of arguments passed to the program
                for(i=0;arguments_cpy[i];arguments_cpy[i]==','?i++:*arguments_cpy++);
                int nargs = i+1; // i = number of commas
                int nargs_tot; // will be the total number of arguments (nargs+program name+etc)
                char *token; // used for tokenizing arguments
                char **args; // this is the NULL-terminated array of strings

                /* In this case it's necessary to parse the arguments string
                 * breaking it into tokens and then arranging the args of the
                 * system call in a NULL-terminated array of strings which we
                 * pass to execvp
                 */
                // Tokenizing breaks the original string so we make a copy
                strcpy(arguments_cpy,arguments);
                // Args in system call are (program tasknum arguments), so 2+nargs
                nargs_tot = 2+nargs;
                args = (char**)malloc((nargs_tot+1)*sizeof(char*));
                args[nargs_tot]=NULL; // NULL-termination of args
                for (i=0;i<nargs_tot;i++)
                    args[i] = malloc(BUFFER_SIZE);
                // Two first command line arguments
                //strcpy(args[0],inp_programFile);
                sprintf(args[0],"./%s",inp_programFile);
                sprintf(args[1],"%d",taskNumber);
                // Tokenize arguments_cpy
                token = strtok(arguments_cpy,",");
                // Copy the token to its place in args
                for (i=2;i<nargs_tot;i++) {
                    strcpy(args[i],token);
                    token = strtok(NULL,",");
                }
                // Call the execution and check for errors
                err = execvp(args[0],args);
                perror("ERROR:: child C process");
                exit(err);

            /* PYTHON */
            } else if (task_type == 2) {
                // Preparations for C or Python execution 
                char *arguments_cpy;
                arguments_cpy=malloc(strlen(arguments));
                strcpy(arguments_cpy,arguments);
                // This counts how many commas there are in arguments, giving the number
                // of arguments passed to the program
                for(i=0;arguments_cpy[i];arguments_cpy[i]==','?i++:*arguments_cpy++);
                int nargs = i+1; // i = number of commas
                int nargs_tot; // will be the total number of arguments (nargs+program name+etc)
                char *token; // used for tokenizing arguments
                char **args; // this is the NULL-terminated array of strings

                // Same as in C, but adding "python" as first argument
                // Tokenizing breaks the original string so we make a copy
                strcpy(arguments_cpy,arguments);
                // args: (0)-python (1)-program (2)-tasknumber (3..nargs+3)-arguments
                nargs_tot = 3+nargs;
                args = (char**)malloc((nargs_tot+1)*sizeof(char*));
                args[nargs_tot]=NULL; // NULL-termination of args
                for (i=0;i<nargs_tot;i++)
                    args[i] = malloc(BUFFER_SIZE);
                // Two first command line arguments
                strcpy(args[0],"python");
                strcpy(args[1],inp_programFile);
                sprintf(args[2],"%d",taskNumber);
                // Tokenize arguments_cpy
                token = strtok(arguments_cpy,",");
                // Copy the token to its place in args
                for (i=3;i<nargs_tot;i++) {
                    strcpy(args[i],token);
                    token = strtok(NULL,",");
                }

                // Call the execution and check for errors
                err = execvp(args[0],args);
                perror("ERROR:: child Python process");
                exit(err);

            /* PARI/GP */
            } else if (task_type == 3) {
                // NULL-terminated array of strings
                char **args;
                char filename[FNAME_SIZE];
                sprintf(filename,"%s/auxprog-%d.gp",out_dir,taskNumber);
                // 0: gp, 1: -f, 2: -s400G, 3: file, 4: NULL
                int nargs=4;
                args = (char**)malloc((nargs+1)*sizeof(char*));
                // Do not malloc for NULL
                for (i=0;i<nargs;i++)
                    args[i] = malloc(BUFFER_SIZE);
                // Fill up the array with strings
                sprintf(args[0],"gp");
                sprintf(args[1],"-f");
                sprintf(args[2],"-s400G");
                sprintf(args[3],"%s",filename);
                args[4] = NULL;

                // Call the execution and check for errors
                err = execvp(args[0],args);
                perror("ERROR:: child PARI process");
                exit(err);

            /* SAGE */
            } else if (task_type == 4) {
                // NULL-terminated array of strings 
                char **args;
                char filename[FNAME_SIZE];
                sprintf(filename,"%s/auxprog-%d.sage",out_dir,taskNumber);
                // 0: sage, 1: file, 3: NULL
                int nargs=2;
                args = (char**)malloc((nargs+1)*sizeof(char*));
                // Do not malloc for NULL
                for (i=0;i<nargs;i++)
                    args[i] = malloc(BUFFER_SIZE);
                // Fill up the array with strings
                sprintf(args[0],"sage");
                sprintf(args[1],"%s",filename);
                //sprintf(args[1],"-c \"taskId=%d;taskArgs:=[%s];load('%s')\"",taskNumber,arguments,inp_programFile);
                args[2] = NULL;

                // Call the execution and check for errors
                err = execvp(args[0],args);
                perror("ERROR:: child Sage process");
                exit(err);
            }

        }


        /* Attempt at measuring memory usage for the child process */
        siginfo_t infop; // Stores information about the child execution
        waitid(P_PID,pid,&infop,WEXITED); // Wait for the execution to end
        // Computation time
        time(&endt);
        difft = difftime(endt,initt);
        totalt += difft;
        if (infop.si_code == CLD_KILLED
                || infop.si_code == CLD_DUMPED) {
            prterror(pid,taskNumber,out_dir,difft);
            state=ST_TASK_KILLED;
        } else if (flag_mem) {
            struct rusage usage; // Stores information about the child's resource usage
            getrusage(RUSAGE_CHILDREN,&usage); // Get child resource usage
            prtusage(pid,taskNumber,out_dir,usage); // Print resource usage to file
            state=0;
        }

        // Send response to master
        pvm_initsend(PVM_ENCODING);
        pvm_pkint(&me,1,1);
        pvm_pkint(&taskNumber,1,1);
        pvm_pkint(&state,1,1);
        pvm_pkstr(arguments);
        pvm_pkdouble(&difft,1,1);
        pvm_pkdouble(&totalt,1,1);
        pvm_send(myparent,MSG_RESULT);
    }


    // Dismantle slave
    pvm_exit();
    exit(0);
}
