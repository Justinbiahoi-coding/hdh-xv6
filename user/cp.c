#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

// 512 byte = BSIZE, dung bang kich thuoc mot block cua file system xv6
// (xem kernel/fs.h). Moi lan read() lay dung mot block nen khong co block
// nao phai doc hai lan. Buffer dat o vung toan cuc giong user/cat.c vi
// stack cua tien trinh xv6 chi co mot trang 4096 byte.
char buf[512];

// Ghi cho du n byte ra fd. write() co the ghi thieu hon so byte yeu cau
// (dia het cho, hoac ben nhan la pipe dang day), nen phai lap lai tu cho
// con thieu. Tra ve 0 neu ghi du, -1 neu loi.
static int
writeall(int fd, char *p, int n)
{
  int w;

  while(n > 0){
    if((w = write(fd, p, n)) <= 0)
      return -1;
    p += w;
    n -= w;
  }
  return 0;
}

int
main(int argc, char *argv[])
{
  int src, dst, n;
  struct stat ss, ds;

  if(argc != 3){            // != bat ca truong hop thieu va thua doi so
    fprintf(2, "usage: cp src dst\n");
    exit(1);
  }

  if((src = open(argv[1], O_RDONLY)) < 0){
    fprintf(2, "cp: cannot open %s\n", argv[1]);
    exit(1);
  }

  if(fstat(src, &ss) < 0){
    fprintf(2, "cp: cannot stat %s\n", argv[1]);
    close(src);
    exit(1);
  }

  // open() cua xv6 cho mo thu muc o che do chi doc, va read() tren fd do
  // tra ve noi dung tho cua cac struct dirent. Khong chan thi "cp . d" se
  // tao ra mot file rac 1024 byte thay vi bao loi.
  if(ss.type == T_DIR){
    fprintf(2, "cp: %s is a directory\n", argv[1]);
    close(src);
    exit(1);
  }

  // Phai so sanh src voi dst TRUOC khi mo dst, vi O_TRUNC cat file ngay
  // luc open(): "cp a a" se xoa trang a roi moi doc -> mat du lieu.
  // So theo cap (dev, ino) chu khong so theo ten, nen bat duoc ca truong
  // hop hai ten khac nhau cung tro ve mot inode ("a" va "./a").
  if(stat(argv[2], &ds) >= 0 && ds.dev == ss.dev && ds.ino == ss.ino){
    fprintf(2, "cp: %s and %s are the same file\n", argv[1], argv[2]);
    close(src);
    exit(1);
  }

  // O_CREATE tao dst neu chua co; O_TRUNC xoa noi dung cu neu da co --
  // thieu O_TRUNC thi copy file ngan len file dai se con lai phan duoi
  // cua file cu, dst thanh lai tap cua hai file.
  if((dst = open(argv[2], O_CREATE|O_WRONLY|O_TRUNC)) < 0){
    fprintf(2, "cp: cannot open %s\n", argv[2]);
    close(src);
    exit(1);
  }

  while((n = read(src, buf, sizeof(buf))) > 0){
    if(writeall(dst, buf, n) < 0){
      fprintf(2, "cp: write error\n");
      close(src);
      close(dst);
      exit(1);
    }
  }

  if(n < 0){                // n == 0 la het file, chi n < 0 moi la loi
    fprintf(2, "cp: read error\n");
    close(src);
    close(dst);
    exit(1);
  }

  close(src);
  close(dst);
  exit(0);                  // user.ld vao thang main, return se nhay vao rac
}
