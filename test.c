#include <stdio.h>
#include <semaphore.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#define BUFFER_FILE "/tmp/test1"
#define PRODUCER_MAX 1000
#define buf_size 10
#define char_buf_size buf_size*(sizeof(int) / sizeof(char))

void producer() {
  int buffer_id;
  sem_t* empty = sem_open("empty", O_CREAT, 0777, 0);
  // perror("sem_empty:producer");
  sem_t* sync = sem_open("sync", O_CREAT, 0777, 0);
  // perror("sem_sync:producer");
  sem_t* full = sem_open("full", O_CREAT, 0777, 0);
  // perror("sem_full:producer");
  int i, j, k;

  int buf[buf_size] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
  char* char_buf = (char*)buf;

  sem_getvalue(empty, &i);
  sem_getvalue(sync, &j);
  sem_getvalue(full, &k);
  // printf("Producer %d: %p:%d %p:%d %p:%d\n", getpid(), empty, i, sync, j,
  // full,
  //  k);

  sem_post(sync);
  sem_post(empty);

  buffer_id = open(BUFFER_FILE, O_RDWR | O_CREAT);
  write(buffer_id, char_buf, char_buf_size);
  close(buffer_id);

  while (i <= PRODUCER_MAX) {
    int j;
    sem_wait(empty);
    sem_wait(sync);

    buffer_id = open(BUFFER_FILE, O_RDWR | O_CREAT);
    read(buffer_id, char_buf, char_buf_size);
    close(buffer_id);

    for (j = 0; j < buf_size; j++) {
      if (buf[j] < 0) {
        buf[j] = i;
        // printf("PROD %d\n", i);
        i++;
      }
    }

    buffer_id = open(BUFFER_FILE, O_RDWR | O_CREAT | O_TRUNC);
    write(buffer_id, char_buf, char_buf_size);
    close(buffer_id);

    sem_post(sync);
    sem_post(full);
  }
  sem_wait(empty);
  sem_wait(sync);

  buffer_id = open(BUFFER_FILE, O_RDWR | O_CREAT);
  read(buffer_id, char_buf, char_buf_size);
  close(buffer_id);

  for (j = 0; j < buf_size; j++) {
    buf[j] = -2;
  }

  buffer_id = open(BUFFER_FILE, O_RDWR | O_CREAT | O_TRUNC);
  write(buffer_id, char_buf, char_buf_size);
  close(buffer_id);

  sem_post(sync);
  sem_post(full);
}

void consumer() {
  sem_t* empty = sem_open("empty", O_CREAT, 0777, 0);
  // perror("sem_empty:consumer");
  sem_t* sync = sem_open("sync", O_CREAT, 0777, 0);
  // perror("sem_sync:consumer");
  sem_t* full = sem_open("full", O_CREAT, 0777, 0);
  // perror("sem_full:consumer");

  int buffer_id;
  int buf[buf_size] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
  char* char_buf = (char*)buf;
  int i, j;

  // printf("Consumer %d: %p %p %p\n", getpid(), empty, sync, full);

  while (1) {
    int min = 0;
    int num;
    int has_data = 0;
    sem_wait(full);
    sem_wait(sync);
    // printf("Consumer %d: awake\n", getpid());

    buffer_id = open(BUFFER_FILE, O_RDWR | O_CREAT, 0777);
    read(buffer_id, char_buf, char_buf_size);
    close(buffer_id);

    for (j = 0; j < buf_size; j++) {
      if (buf[j] >= 0) {
        printf("%d: %d\n", getpid(), buf[j]);
        buf[j] = -1;
        has_data = 1;
        break;
      }
      if (buf[j] == -2) {
        has_data = 1;
        break;
      }
    }

    buffer_id = open(BUFFER_FILE, O_RDWR | O_CREAT | O_TRUNC);
    write(buffer_id, char_buf, char_buf_size);
    close(buffer_id);

    sem_post(sync);
    if (has_data)
      sem_post(full);
    else
      sem_post(empty);
    sleep(0);
  }
}

int main(int argc, char** argv) {
  pid_t pid_1;
  pid_t pid_2;
  sem_unlink("empty");
  sem_unlink("full");
  sem_unlink("sync");

  if ((pid_1 = fork())) {
    if ((pid_2 = fork())) {
      producer();
      waitpid(pid_2, NULL, 0);
    } else {
      consumer();
    }
    waitpid(pid_1, NULL, 0);
  } else {
    pid_2 = fork();
    consumer();
    if (pid_2) waitpid(pid_2, NULL, 0);
  }
  return 0;
}
