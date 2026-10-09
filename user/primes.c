// primes.c -- sang so nguyen te kieu day chuyen (Doug McIlroy).
//
// Moi tien trinh phu trach dung mot so nguyen te: no doc so tu tien trinh
// truoc, bo cac so chia het cho so nguyen te cua minh, va day phan con lai
// sang tien trinh sau. So dau tien doc duoc chinh la so nguyen te cua no.
//
//   [sinh 2..280] -> [loc boi 2] -> [loc boi 3] -> [loc boi 5] -> ...
//
// Truyen so nguyen 4 byte truc tiep qua pipe, khong dung dang ASCII.

#include "kernel/types.h"
#include "user/user.h"

#define MAX 280

// Ham nay khong bao gio tra ve (luon ket thuc bang exit), bao cho trinh
// bien dich biet de no khong canh bao de quy vo han.
void loc(int left) __attribute__((noreturn));

// loc -- than cua mot tang trong day chuyen.
// left: dau doc cua pipe noi voi tang truoc.
void
loc(int left)
{
  int nguyen_to, n, right[2], pid;

  // So dau tien doc duoc luon la so nguyen te: moi so nho hon no deu da
  // bi cac tang truoc loc sach.
  if(read(left, &nguyen_to, sizeof(int)) != sizeof(int)){
    close(left);        // khong con so nao -> day chuyen ket thuc
    exit(0);
  }
  printf("prime %d\n", nguyen_to);

  // Chi tao tang sau khi that su co so di qua duoc. Tao san tang moi cho
  // moi so nguyen te se vuot gioi han tien trinh truoc khi toi 280.
  pid = -1;
  while(read(left, &n, sizeof(int)) == sizeof(int)){
    if(n % nguyen_to == 0)
      continue;                      // boi so -> bo

    if(pid < 0){                     // lan dau co so sot: dung tang sau
      if(pipe(right) < 0){
        fprintf(2, "primes: pipe failed\n");
        exit(1);
      }
      if((pid = fork()) < 0){
        fprintf(2, "primes: fork failed\n");
        exit(1);
      }
      if(pid == 0){
        close(left);                 // tien trinh con khong dung dau nay
        close(right[1]);             // con chi doc, khong ghi
        loc(right[0]);               // khong bao gio tra ve
      }
      close(right[0]);               // cha chi ghi, khong doc
    }

    if(write(right[1], &n, sizeof(int)) != sizeof(int)){
      fprintf(2, "primes: write failed\n");
      exit(1);
    }
  }

  close(left);
  if(pid >= 0){
    // Dong dau ghi thi read() o tang sau moi tra ve 0 va day chuyen moi
    // ket thuc duoc. Quen dong o day se treo toan bo chuong trinh.
    close(right[1]);
    wait(0);            // cho ca nhanh phia sau xong roi moi thoat
  }
  exit(0);
}

int
main(void)
{
  int p[2], i, pid;

  if(pipe(p) < 0){
    fprintf(2, "primes: pipe failed\n");
    exit(1);
  }
  if((pid = fork()) < 0){
    fprintf(2, "primes: fork failed\n");
    exit(1);
  }

  if(pid == 0){
    close(p[1]);
    loc(p[0]);
  }

  // Tien trinh goc chi sinh so 2..MAX roi day vao day chuyen.
  close(p[0]);
  for(i = 2; i <= MAX; i++){
    if(write(p[1], &i, sizeof(int)) != sizeof(int)){
      fprintf(2, "primes: write failed\n");
      exit(1);
    }
  }
  close(p[1]);          // bao het so cho tang dau tien
  wait(0);              // chi thoat sau khi ca day chuyen da in xong
  exit(0);
}
