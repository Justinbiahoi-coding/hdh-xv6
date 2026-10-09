// pingpong.c -- cha va con truyen qua lai mot byte qua hai pipe.
//
// PID in ra khong co dinh, no la bo dem tang dan trong kernel. Cai dung
// la: hai PID lien nhau, con (in ping) lon hon cha (in pong) mot don vi.

#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
  int p2c[2];       // cha -> con (ping)
  int c2p[2];       // con -> cha (pong)
  char buf;
  int pid;

  // Phai dung HAI pipe. fork() nhan doi bang file descriptor, nen voi mot
  // pipe duy nhat thi ca hai tien trinh deu giu ca dau doc lan dau ghi, va
  // cha co the doc lai chinh byte minh vua ghi truoc khi con kip doc.
  //
  // Pipe phai tao TRUOC fork thi con moi thua huong duoc.
  if(pipe(p2c) < 0 || pipe(c2p) < 0){
    fprintf(2, "pingpong: pipe failed\n");
    exit(1);
  }

  if((pid = fork()) < 0){
    fprintf(2, "pingpong: fork failed\n");
    exit(1);
  }

  if(pid == 0){
    // Con: chi doc p2c, chi ghi c2p. Dong hai dau con lai de moi chieu
    // chi con mot nguoi ghi va mot nguoi doc.
    close(p2c[1]);
    close(c2p[0]);

    // read chan cho den khi cha ghi. Tra ve 1 neu doc duoc,
    // 0 neu het du lieu, -1 neu loi.
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
    exit(0);        // bat buoc, khong thi con chay tiep xuong phan cua cha
  }

  // Cha: chi ghi p2c, chi doc c2p.
  close(p2c[0]);
  close(c2p[1]);

  if(write(p2c[1], "x", 1) != 1){
    fprintf(2, "pingpong: parent write failed\n");
    exit(1);
  }

  // Cha bi chan o day cho den khi con ghi. Chinh su chan nay bao dam
  // "ping" in truoc "pong", khong can sleep hay wait de ep thu tu.
  if(read(c2p[0], &buf, 1) != 1){
    fprintf(2, "pingpong: parent read failed\n");
    exit(1);
  }
  printf("%d: received pong\n", getpid());

  close(p2c[1]);
  close(c2p[0]);
  wait(0);          // thu hoach con, tranh de lai zombie
  exit(0);
}
