#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

// Kernel chan duong dan dai hon MAXPATH (argstr/copyinstr trong
// kernel/syscall.c), nen buffer lon hon the cung vo dung: open() se that bai
// truoc khi ta dung den phan du.
#define PATHMAX MAXPATH

// Moi cap sau them 4 ky tu vao tien to, nhung ky tu ve cay la UTF-8 nen
// chiem toi 6 byte moi cap. Do sau bi PATHMAX chan o khoang PATHMAX/2 cap.
#define PREFMAX 512

// Ky tu ve cay -- chon style Unicode cho khop dung mau trong de bai.
// Da kiem chung that trong QEMU: console xv6 (kernel/uart.c -> uartputc) day
// tung byte ra UART ma khong qua lop ma hoa nao, nen chuoi UTF-8 di qua
// nguyen ven va terminal host hien dung cac ky tu khung. De bai cho phep
// dung ASCII thay the; neu can, chi doi 4 dong #define duoi day sang
// "|-- ", "`-- ", "|   ", "    " la xong, phan con lai khong phai sua.
#define TEE_MID   "\u251c\u2500\u2500 "
#define TEE_LAST  "\u2514\u2500\u2500 "
#define PAD_MID   "\u2502   "
#define PAD_LAST  "    "

// Hai buffer dung chung, khong dat tren stack: stack cua tien trinh xv6 chi
// co USERSTACK trang (2 trang = 8KB cho lab util), de qui voi mang
// PATHMAX+PREFMAX byte moi khung se tran stack o do sau vai cap.
// Ca hai ham duoi day quy uoc: sua xong thi tra lai noi dung cho nguoi goi.
char path[PATHMAX];
char prefix[PREFMAX];

static void treedir(int plen, int preflen, int level, int maxdepth, int onlydir);

// In mot muc con roi de qui xuong neu no la thu muc.
// namepos la vi tri trong path[] danh cho ten muc con.
static void
emit(int namepos, int preflen, char *name, short type, int last,
     int level, int maxdepth, int onlydir)
{
  int n;

  printf("%s%s%s\n", prefix, last ? TEE_LAST : TEE_MID, name);
  if(type != T_DIR)
    return;

  n = strlen(last ? PAD_LAST : PAD_MID);
  if(preflen + n >= PREFMAX){
    fprintf(2, "tree: tree too deep\n");
    return;
  }
  strcpy(prefix + preflen, last ? PAD_LAST : PAD_MID);
  strcpy(path + namepos, name);
  treedir(namepos + strlen(name), preflen + n, level + 1, maxdepth, onlydir);
  prefix[preflen] = 0;
}

// Liet ke cac muc con cua path[0..plen) dang o do sau level.
static void
treedir(int plen, int preflen, int level, int maxdepth, int onlydir)
{
  int fd, namepos;
  struct dirent de;
  struct stat st;
  char cur[DIRSIZ+1];
  char pname[DIRSIZ+1];
  short ptype = 0;
  int havepend = 0;

  // -L depth: level dem tu 0 o goc, cac muc con cua goc o level 1. Nen
  // "-L 1" = chi in goc va con truc tiep <=> dung liet ke khi level == 1.
  if(maxdepth >= 0 && level >= maxdepth)
    return;

  if((fd = open(path, O_RDONLY)) < 0){
    fprintf(2, "tree: cannot open %s\n", path);
    return;
  }

  namepos = plen;
  if(plen > 0 && path[plen-1] != '/')
    path[namepos++] = '/';
  if(namepos + DIRSIZ >= PATHMAX){
    fprintf(2, "tree: path too long: %s\n", path);
    close(fd);
    return;
  }

  // Phai biet mot muc co phai muc CUOI cung hay khong moi chon duoc giua
  // "|--" va "`--", nhung read() chi di mot chieu va xv6 khong co lseek.
  // Giai phap: giu muc vua doc lai mot nhip (pname/ptype), chi in no khi da
  // biet con muc nao sau nua hay khong.
  while(read(fd, &de, sizeof(de)) == sizeof(de)){
    if(de.inum == 0)
      continue;
    // de.name dai dung DIRSIZ byte va KHONG co '\0' neu ten dung het cho,
    // nen phai copy DIRSIZ byte roi tu dat dau ket chuoi.
    memmove(cur, de.name, DIRSIZ);
    cur[DIRSIZ] = 0;
    // "." tro ve chinh thu muc nay va ".." tro ve cha: de qui vao se lap vo tan.
    if(strcmp(cur, ".") == 0 || strcmp(cur, "..") == 0)
      continue;
    strcpy(path + namepos, cur);
    if(stat(path, &st) < 0){
      fprintf(2, "tree: cannot stat %s\n", path);
      continue;
    }
    if(onlydir && st.type != T_DIR)
      continue;

    // emit() ghi de len path+namepos (va de qui ghi tiep phia sau), nen ten
    // vua doc phai duoc giu rieng trong cur[] truoc khi goi emit().
    if(havepend)
      emit(namepos, preflen, pname, ptype, 0, level, maxdepth, onlydir);
    strcpy(pname, cur);
    ptype = st.type;
    havepend = 1;
  }

  // Dong fd truoc khi de qui vao muc cuoi: moi cap de qui giu mot fd mo,
  // tien trinh xv6 chi co NOFILE = 16 fd.
  close(fd);
  if(havepend)
    emit(namepos, preflen, pname, ptype, 1, level, maxdepth, onlydir);
  path[plen] = 0;
}

// Doc so nguyen khong am. Tra -1 neu chuoi rong hoac co ky tu khong phai
// chu so -- atoi() cua xv6 im lang tra 0 trong ca hai truong hop do.
static int
parsedepth(const char *s)
{
  int n = 0;

  if(*s == '\0')
    return -1;
  for(; *s; s++){
    if(*s < '0' || *s > '9')
      return -1;
    if(n > (0x7fffffff - (*s - '0')) / 10)
      return -1;
    n = n * 10 + (*s - '0');
  }
  return n;
}

int
main(int argc, char *argv[])
{
  char *root = 0;
  int maxdepth = -1;   // -1 = khong gioi han
  int onlydir = 0;
  int i;
  struct stat st;

  for(i = 1; i < argc; i++){
    if(strcmp(argv[i], "-L") == 0){
      if(i + 1 >= argc){
        fprintf(2, "tree: -L can mot so nguyen khong am\n");
        exit(1);
      }
      if((maxdepth = parsedepth(argv[++i])) < 0){
        fprintf(2, "tree: -L can mot so nguyen khong am\n");
        exit(1);
      }
    } else if(strcmp(argv[i], "-d") == 0){
      onlydir = 1;
    } else if(argv[i][0] == '-' || root != 0){
      fprintf(2, "usage: tree [path] [-L depth] [-d]\n");
      exit(1);
    } else {
      root = argv[i];
    }
  }
  if(root == 0)
    root = ".";

  if(strlen(root) >= PATHMAX){
    fprintf(2, "tree: path too long: %s\n", root);
    exit(1);
  }
  if(stat(root, &st) < 0){
    fprintf(2, "tree: cannot stat %s\n", root);
    exit(1);
  }

  strcpy(path, root);
  prefix[0] = 0;
  printf("%s\n", root);
  if(st.type == T_DIR)
    treedir(strlen(root), 0, 0, maxdepth, onlydir);

  exit(0);
}
