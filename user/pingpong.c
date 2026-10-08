#include "kernel/types.h"
#include "user/user.h"

// Hai pipe mot chieu:
//   p2c -- cha ghi, con doc  (ping)
//   c2p -- con ghi, cha doc  (pong)
//
// Mot pipe duy nhat khong du: fork() nhan doi bang file descriptor, nen
// ca hai tien trinh deu giu ca dau doc va dau ghi cua cung pipe do. Cha
// co the doc lai chinh byte minh vua ghi truoc khi con kip doc.

int
main(void)
{
  int p2c[2], c2p[2];
  char buf;
  int pid;

  if(pipe(p2c) < 0 || pipe(c2p) < 0){
    fprintf(2, "pingpong: pipe failed\n");
    exit(1);
  }

  if((pid = fork()) < 0){
    fprintf(2, "pingpong: fork failed\n");
    exit(1);
  }

  if(pid == 0){
    // Con: chi doc p2c, chi ghi c2p -- dong hai dau con lai.
    close(p2c[1]);
    close(c2p[0]);

    if(read(p2c[0], &buf, 1) != 1){
      fprintf(2, "pingpong: child read failed\n");
      exit(1);
    }
    printf("%d: received ping\n", getpid());

    if(write(c2p[1], &buf, 1) != 1){
      fprintf(2, "pingpong: child write failed\n");
      exit(1);
    }

    close(p2c[0]);
    close(c2p[1]);
    exit(0);
  }

  // Cha: chi ghi p2c, chi doc c2p.
  close(p2c[0]);
  close(c2p[1]);

  if(write(p2c[1], "x", 1) != 1){
    fprintf(2, "pingpong: parent write failed\n");
    exit(1);
  }

  if(read(c2p[0], &buf, 1) != 1){
    fprintf(2, "pingpong: parent read failed\n");
    exit(1);
  }
  printf("%d: received pong\n", getpid());

  close(p2c[1]);
  close(c2p[0]);
  wait(0);          // thu hoach tien trinh con, tranh de lai zombie
  exit(0);
}
