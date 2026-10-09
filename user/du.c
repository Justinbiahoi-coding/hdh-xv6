#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

// Kernel chan duong dan dai hon MAXPATH (argstr/copyinstr trong
// kernel/syscall.c), nen buffer lon hon the cung vo dung.
#define PATHMAX MAXPATH

// Buffer duong dan dung chung, khong dat tren stack: stack cua tien trinh
// xv6 chi co USERSTACK trang (2 trang = 8KB cho lab util), de qui voi mang
// PATHMAX byte moi khung se tran stack o do sau vai chuc cap.
char path[PATHMAX];

// Tra ve tong so byte cua cac muc nam BEN TRONG thu muc path[0..plen).
//
// Quy uoc kich thuoc thu muc: tong cua mot thu muc = tong kich thuoc cac con,
// KHONG cong st.size cua ban than inode thu muc (moi thu muc xv6 chiem it
// nhat 1 block = 1024 byte cho danh sach dirent). Vi vay thu muc rong bao 0,
// dung nhu mau trong de bai. Quy uoc nay duoc ap dung nhat quan o moi cap.
//
// Ham tu in cac dong cua CAC CON (khong in dong cua chinh no) de thu tu
// hau to dung nhu mau de: con truoc, cha sau. Dong cua goc do main() in.
static long
du(int plen, int printall, int print)
{
  int fd, namepos;
  struct dirent de;
  struct stat st;
  long total = 0, sub;

  if((fd = open(path, O_RDONLY)) < 0){
    fprintf(2, "du: cannot open %s\n", path);
    return 0;
  }

  namepos = plen;
  if(plen > 0 && path[plen-1] != '/')
    path[namepos++] = '/';
  if(namepos + DIRSIZ >= PATHMAX){
    fprintf(2, "du: path too long: %s\n", path);
    close(fd);
    return 0;
  }

  while(read(fd, &de, sizeof(de)) == sizeof(de)){
    if(de.inum == 0)
      continue;
    // de.name dai dung DIRSIZ byte va KHONG co '\0' neu ten dung het cho,
    // nen phai copy DIRSIZ byte roi tu dat dau ket chuoi.
    memmove(path + namepos, de.name, DIRSIZ);
    path[namepos + DIRSIZ] = 0;
    // "." tro ve chinh thu muc nay va ".." tro ve cha: di vao se lap vo tan
    // va dem trung kich thuoc.
    if(strcmp(path + namepos, ".") == 0 || strcmp(path + namepos, "..") == 0)
      continue;
    if(stat(path, &st) < 0){
      fprintf(2, "du: cannot stat %s\n", path);
      continue;
    }

    if(st.type == T_DIR){
      sub = du(namepos + strlen(path + namepos), printall, print);
      total += sub;
      // du() de qui da tra lai path[] nguyen trang nen path dang la duong
      // dan cua thu muc con.
      if(print)
        printf("%ld\t%s\n", sub, path);
    } else {
      total += st.size;
      if(print && printall)
        printf("%ld\t%s\n", (long)st.size, path);
    }
  }

  close(fd);
  path[plen] = 0;
  return total;
}

int
main(int argc, char *argv[])
{
  char *root = 0;
  int printall = 0;
  int sflag = 0;
  int i;
  struct stat st;
  long total;

  for(i = 1; i < argc; i++){
    if(strcmp(argv[i], "-a") == 0)
      printall = 1;
    else if(strcmp(argv[i], "-s") == 0)
      sflag = 1;
    else if(argv[i][0] == '-' || root != 0){
      fprintf(2, "usage: du [path] [-a] [-s]\n");
      exit(1);
    } else
      root = argv[i];
  }
  if(root == 0)
    root = ".";

  if(strlen(root) >= PATHMAX){
    fprintf(2, "du: path too long: %s\n", root);
    exit(1);
  }
  if(stat(root, &st) < 0){
    fprintf(2, "du: cannot stat %s\n", root);
    exit(1);
  }

  strcpy(path, root);
  if(st.type == T_DIR)
    total = du(strlen(root), printall, !sflag);   // -s: chi in dong tong cuoi
  else
    total = st.size;

  printf("%ld\t%s\n", total, root);
  exit(0);
}
