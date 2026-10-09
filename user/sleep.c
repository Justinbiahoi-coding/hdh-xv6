#include "kernel/types.h"
#include "user/user.h"

#define INT_MAX 0x7fffffff      
// Doi chuoi sang so. Tra ve -1 neu chuoi rong, co ky tu khong phai chu so,hoac gia tri vuot qua INT_MAX.
// Khong dung atoi() vi no khong co cach bao loi: atoi("abc") tra ve 0,khong phan biet duoc voi chuoi "0" hop le.
static int parse_ticks(const char *s){
  int ket_qua = 0;
  int i, chu_so;
  if(s[0] == '\0')              // chuoi rong
    return -1;
  for(i = 0; s[i] != '\0'; i++){
    if(s[i] < '0' || s[i] > '9')
      return -1;
    chu_so = s[i] - '0';
    // Kiem tran truoc khi nhan. Tran so nguyen co dau la undefined behavior,trinh bien dich se xoa phep kiem tra neu viet sau phep nhan.
    if(ket_qua > (INT_MAX - chu_so) / 10)
      return -1;
    ket_qua = ket_qua * 10 + chu_so;
  }
  return ket_qua;
}
int main(int argc, char *argv[]){
  int so_tick;
  // Dung != 2 de bat thieu doi so lan thua doi so.
  if(argc != 2){
    fprintf(2, "usage: sleep ticks\n");   // fd 2 = stderr
    exit(1);
  }
  so_tick = parse_ticks(argv[1]);
  if(so_tick < 0){
    fprintf(2, "sleep: ticks phai la so nguyen khong am\n");
    exit(1);
  }
  // sys_sleep tra ve -1 neu tien trinh bi kill luc dang ngu.
  if(sleep(so_tick) < 0){
    fprintf(2, "sleep: sleep failed\n");
    exit(1);
  }
  // return se nhay vao dia chi rac.
  exit(0);
}
