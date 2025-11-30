#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>

#define NUM_QUESTIONS 5
#define RUBRIC_FILE "rubric.txt"
#define EXAMS_DIR "exams"

// Shared Memory Structure
typedef struct {
    struct {
        int id;
        char grade;
    } rubric[NUM_QUESTIONS];
    
    struct {
        char student_id[10];
        int questions_marked[NUM_QUESTIONS]; 
        int exam_file_index; 
    } exam;

    int all_finished;
} SharedData;

void ta_process(int ta_id, int shm_id);
void load_rubric(SharedData *shared);
void save_rubric(SharedData *shared);
void load_exam(SharedData *shared, int exam_index);
void delay_ms(int min_ms, int max_ms);

int main(int argc, char *argv[]) {
    int num_tas;
    int i;
    int shm_id;
    key_t key;
    SharedData *shared;
    pid_t pid;

    // check args
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

    // shared memory stuff
    key = IPC_PRIVATE;
    shm_id = shmget(key, sizeof(SharedData), IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("shmget");
        return 1;
    }

    // attach to pointer
    shared = (SharedData *)shmat(shm_id, NULL, 0);
    if (shared == (void *)-1) {
        perror("shmat");
        return 1;
    }

    // load first stuff
    load_rubric(shared);
    shared->exam.exam_file_index = 1;
    load_exam(shared, 1);
    shared->all_finished = 0;

    printf("Main: Loaded Rubric and Exam 1.\n");

    // make processes
    for (i = 0; i < num_tas; i++) {
        pid = fork();
        if (pid == 0) {
            ta_process(i + 1, shm_id);
            exit(0);
        } else if (pid < 0) {
            perror("fork");
            return 1;
        }
    }

    // wait for children
    for (i = 0; i < num_tas; i++) {
        wait(NULL);
    }

    printf("Main: All TAs finished. Cleaning up.\n");

    // detach and remove shared memory
    shmdt(shared);
    shmctl(shm_id, IPC_RMID, NULL);

    return 0;
}

void ta_process(int ta_id, int shm_id) {
    SharedData *shared;
    int i;
    int marked_any;
    int all_marked;
    int next_index;

    shared = (SharedData *)shmat(shm_id, NULL, 0);
    if (shared == (void *)-1) {
        perror("shmat child");
        exit(1);
    }

    srand(time(NULL) ^ (getpid() << 16)); 

    while (!shared->all_finished) {
        // check rubric
        printf("TA %d: Reviewing rubric.\n", ta_id);
        for (i = 0; i < NUM_QUESTIONS; i++) {
            delay_ms(500, 1000);
            
            // random chance to change it
            if (rand() % 2 == 0) { 
                printf("TA %d: Modifying rubric for Q%d.\n", ta_id, i + 1);
                shared->rubric[i].grade++; 
                save_rubric(shared);
            }
        }

        // mark exam
        marked_any = 0;
        for (i = 0; i < NUM_QUESTIONS; i++) {
            if (shared->exam.questions_marked[i] == 0) {
                printf("TA %d: Marking Q%d for Student %s.\n", ta_id, i + 1, shared->exam.student_id);
                
                // simulated work
                delay_ms(1000, 2000);
                
                shared->exam.questions_marked[i] = 1;
                
                printf("TA %d: Finished marking Q%d for Student %s.\n", ta_id, i + 1, shared->exam.student_id);
                marked_any = 1;
            }
        }

        // check if done
        all_marked = 1;
        for (i = 0; i < NUM_QUESTIONS; i++) {
            if (shared->exam.questions_marked[i] == 0) {
                all_marked = 0;
                break;
            }
        }

        if (all_marked && !shared->all_finished) {
            // get next exam
            next_index = shared->exam.exam_file_index + 1;
            printf("TA %d: Exam %s finished. Loading Exam %d...\n", ta_id, shared->exam.student_id, next_index);
            
            load_exam(shared, next_index);
        }
        
        // dont busy wait
        if (!marked_any && !all_marked) {
            usleep(100000); 
        }
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
        shared->exam.questions_marked[i] = 0;
    }
}

void delay_ms(int min_ms, int max_ms) {
    // helper to sleep for a random range
    int ms = min_ms + rand() % (max_ms - min_ms + 1);
    usleep(ms * 1000);
}
