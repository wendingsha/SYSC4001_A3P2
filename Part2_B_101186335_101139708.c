#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>

#define NUM_QUESTIONS 5
#define RUBRIC_FILE "rubric.txt"
#define EXAMS_DIR "exams"

// semaphores
#define SEM_RUBRIC 0
#define SEM_EXAM 1
#define NUM_SEMS 2

// status
#define STATUS_UNMARKED 0
#define STATUS_IN_PROGRESS 1
#define STATUS_MARKED 2

// Shared Memory Structure
typedef struct {
    struct {
        int id;
        char grade;
    } rubric[NUM_QUESTIONS];
    
    struct {
        char student_id[10];
        int questions_status[NUM_QUESTIONS]; 
        int exam_file_index; 
    } exam;

    int all_finished;
} SharedData;

void ta_process(int ta_id, int shm_id, int sem_id);
void load_rubric(SharedData *shared);
void save_rubric(SharedData *shared);
void load_exam(SharedData *shared, int exam_index);
void delay_ms(int min_ms, int max_ms);
void sem_wait(int sem_id, int sem_num);
void sem_signal(int sem_id, int sem_num);

int main(int argc, char *argv[]) {
    int num_tas;
    int i;
    int shm_id;
    int sem_id;
    key_t key;
    unsigned short sem_vals[NUM_SEMS];
    SharedData *shared;
    pid_t pid;

    // args check
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <number_of_tas>\n", argv[0]);
        return 1;
    }

    num_tas = atoi(argv[1]);
    if (num_tas < 2) {
        printf("Error: Number of TAs must be at least 2.\n");
        return 1;
    }

    srand(time(NULL));

    // shared mem
    key = IPC_PRIVATE;
    shm_id = shmget(key, sizeof(SharedData), IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("shmget");
        return 1;
    }

    // sems
    sem_id = semget(IPC_PRIVATE, NUM_SEMS, IPC_CREAT | 0666);
    if (sem_id < 0) {
        perror("semget");
        return 1;
    }

    // set sems to 1
    sem_vals[0] = 1;
    sem_vals[1] = 1;
    semctl(sem_id, 0, SETALL, sem_vals);

    // attach shared mem
    shared = (SharedData *)shmat(shm_id, NULL, 0);
    if (shared == (void *)-1) {
        perror("shmat");
        return 1;
    }

    // init
    load_rubric(shared);
    shared->exam.exam_file_index = 1;
    load_exam(shared, 1);
    shared->all_finished = 0;

    printf("Main: Loaded Rubric and Exam 1.\n");

    // make TAs
    for (i = 0; i < num_tas; i++) {
        pid = fork();
        if (pid == 0) {
            ta_process(i + 1, shm_id, sem_id);
            exit(0);
        } else if (pid < 0) {
            perror("fork");
            return 1;
        }
    }

    // wait
    for (i = 0; i < num_tas; i++) {
        wait(NULL);
    }

    printf("Main: All TAs finished. Cleaning up.\n");

    // cleanup everything
    shmdt(shared);
    shmctl(shm_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);

    return 0;
}

void ta_process(int ta_id, int shm_id, int sem_id) {
    SharedData *shared;
    int i;
    int work_done_this_cycle;
    int question_to_mark;
    char current_student[10];
    int all_marked;
    int next_index;

    shared = (SharedData *)shmat(shm_id, NULL, 0);
    if (shared == (void *)-1) {
        perror("shmat child");
        exit(1);
    }

    srand(time(NULL) ^ (getpid() << 16)); 

    while (1) {
        // check if done
        sem_wait(sem_id, SEM_EXAM);
        if (shared->all_finished) {
            sem_signal(sem_id, SEM_EXAM);
            break;
        }
        sem_signal(sem_id, SEM_EXAM);

        // review rubric
        printf("TA %d: Reviewing rubric.\n", ta_id);
        for (i = 0; i < NUM_QUESTIONS; i++) {
            delay_ms(500, 1000);
            
            if (rand() % 2 == 0) { 
                // lock it
                sem_wait(sem_id, SEM_RUBRIC);
                
                printf("TA %d: Modifying rubric for Q%d.\n", ta_id, i + 1);
                shared->rubric[i].grade++; 
                save_rubric(shared);
                
                sem_signal(sem_id, SEM_RUBRIC);
            }
        }

        // mark phase
        work_done_this_cycle = 0;
        
        while (1) {
            question_to_mark = -1;
            
            // lock exam to check
            sem_wait(sem_id, SEM_EXAM);
            
            if (shared->all_finished) {
                sem_signal(sem_id, SEM_EXAM);
                break; 
            }

            all_marked = 1;
            for (i = 0; i < NUM_QUESTIONS; i++) {
                if (shared->exam.questions_status[i] != STATUS_MARKED) {
                    all_marked = 0;
                }
                if (shared->exam.questions_status[i] == STATUS_UNMARKED) {
                    question_to_mark = i;
                    shared->exam.questions_status[i] = STATUS_IN_PROGRESS;
                    strcpy(current_student, shared->exam.student_id);
                    break; 
                }
            }

            if (all_marked) {
                next_index = shared->exam.exam_file_index + 1;
                printf("TA %d: Exam %s finished. Loading Exam %d...\n", ta_id, shared->exam.student_id, next_index);
                load_exam(shared, next_index);
                sem_signal(sem_id, SEM_EXAM);
                continue;
            }
            
            sem_signal(sem_id, SEM_EXAM);

            if (question_to_mark != -1) {
                printf("TA %d: Marking Q%d for Student %s.\n", ta_id, question_to_mark + 1, current_student);
                delay_ms(1000, 2000);
                
                sem_wait(sem_id, SEM_EXAM);
                shared->exam.questions_status[question_to_mark] = STATUS_MARKED;
                printf("TA %d: Finished Q%d for Student %s.\n", ta_id, question_to_mark + 1, current_student);
                sem_signal(sem_id, SEM_EXAM);
                
                work_done_this_cycle = 1;
            } else {
                // yield
                usleep(100000); 
            }
        }
        
        if (shared->all_finished) break;
    }

    printf("TA %d: Finished work.\n", ta_id);
    shmdt(shared);
}

void load_rubric(SharedData *shared) {
    FILE *f;
    char line[256];
    int i = 0;
    int id;
    char grade;

    // open file for reading
    f = fopen(RUBRIC_FILE, "r");
    if (!f) {
        printf("Failed to open rubric\n");
        exit(1);
    }
    
    // read lines like "1, A"
    while (fgets(line, sizeof(line), f) && i < NUM_QUESTIONS) {
        if (sscanf(line, "%d, %c", &id, &grade) == 2) {
            shared->rubric[i].id = id;
            shared->rubric[i].grade = grade;
            i++;
        }
    }
    fclose(f);
}

void save_rubric(SharedData *shared) {
    FILE *f;
    int i;

    // open file for writing (overwrite)
    f = fopen(RUBRIC_FILE, "w");
    if (!f) return;
    
    // write back all lines
    for (i = 0; i < NUM_QUESTIONS; i++) {
        fprintf(f, "%d, %c\n", shared->rubric[i].id, shared->rubric[i].grade);
    }
    fclose(f);
}

void load_exam(SharedData *shared, int exam_index) {
    char filepath[64];
    FILE *f;
    char line[256];
    int i;

    // construct filename e.g. exams/exam_01.txt
    sprintf(filepath, "%s/exam_%02d.txt", EXAMS_DIR, exam_index);
    
    f = fopen(filepath, "r");
    if (!f) {
        // if we cant open it, assume we are done
        shared->all_finished = 1;
        return;
    }

    if (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0; // strip newline
        strcpy(shared->exam.student_id, line);
        
        // check for end condition
        if (strcmp(line, "9999") == 0) {
            shared->all_finished = 1;
            printf("System: Student 9999 reached. Terminating.\n");
        }
    }
    fclose(f);

    // reset exam status in shared memory
    shared->exam.exam_file_index = exam_index;
    for (i = 0; i < NUM_QUESTIONS; i++) {
        shared->exam.questions_status[i] = STATUS_UNMARKED;
    }
}

void delay_ms(int min_ms, int max_ms) {
    // helper to sleep for a random range
    int ms = min_ms + rand() % (max_ms - min_ms + 1);
    usleep(ms * 1000);
}

void sem_wait(int sem_id, int sem_num) {
    // P operation: decrement semaphore
    struct sembuf sb;
    sb.sem_num = sem_num;
    sb.sem_op = -1;
    sb.sem_flg = 0;
    semop(sem_id, &sb, 1);
}

void sem_signal(int sem_id, int sem_num) {
    // V operation: increment semaphore
    struct sembuf sb;
    sb.sem_num = sem_num;
    sb.sem_op = 1;
    sb.sem_flg = 0;
    semop(sem_id, &sb, 1);
}
