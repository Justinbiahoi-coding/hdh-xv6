# Giải thích chi tiết `user/cp.c`

Chương trình `cp` sao chép nội dung một file sang một file khác. Nếu file đích chưa tồn tại
thì tạo mới, nếu đã tồn tại thì ghi đè toàn bộ.

Cú pháp: `cp src dst`

| Đối số | Ý nghĩa |
|---|---|
| `src` | File nguồn, mở ở chế độ chỉ đọc |
| `dst` | File đích, được tạo mới hoặc ghi đè |

---

## 1. Các dòng `#include`

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"
```

| File | Vì sao cần |
|---|---|
| `kernel/types.h` | Các kiểu cơ sở của xv6 (`uint`, `ushort`, `uchar`, `uint64`). Phải include **trước** `user.h` vì `user.h` dùng `uint` trong một số nguyên mẫu. |
| `kernel/stat.h` | `struct stat` và các hằng `T_DIR`, `T_FILE`, `T_DEVICE`. Cần cho hai lớp kiểm tra an toàn ở mục 3. |
| `kernel/fcntl.h` | Các cờ của `open`: `O_RDONLY`, `O_WRONLY`, `O_CREATE`, `O_TRUNC`. Không include file này thì `O_CREATE` là identifier không khai báo → lỗi biên dịch. |
| `user/user.h` | Toàn bộ API userspace: `open`, `read`, `write`, `close`, `stat`, `fstat`, `fprintf`, `exit`. |

xv6 không có `stdio.h`/`fcntl.h` của thư viện C chuẩn — chương trình được biên dịch với
`-nostdlib`. Thứ tự bốn dòng này theo đúng mẫu của `user/cat.c`.

---

## 2. Các quyết định thiết kế

### 2.1 Kích thước buffer: 512 byte

Đề cho chọn 512 hoặc 1024. Nhóm chọn **512** vì đó đúng bằng `BSIZE` — kích thước một block
của file system xv6 (khai báo trong `kernel/fs.h`). Mỗi lần gọi `read()` lấy trọn một block,
nên không có block nào bị kernel đọc hai lần, và không có lần `read` nào bị cắt ngang giữa
hai block. Chọn 1024 vẫn chạy đúng nhưng mỗi lần gọi `read` kernel phải lấy hai block, không
được lợi gì thêm.

### 2.2 Buffer đặt ở vùng toàn cục, không đặt trên stack

```c
char buf[512];
```

Khai báo ngoài mọi hàm nên `buf` nằm trong vùng `.bss`, không nằm trên stack. Lý do: stack của
một tiến trình xv6 chỉ có **một trang 4096 byte** (xem `kernel/exec.c`). Một mảng 512 byte trên
stack thì vẫn vừa, nhưng đây là thói quen mà `user/cat.c` và `user/wc.c` đều theo, và nó giữ
cho việc đổi lên buffer lớn hơn sau này không có nguy cơ tràn stack.

### 2.3 Cờ `O_CREATE|O_WRONLY|O_TRUNC` cho file đích

| Cờ | Vai trò |
|---|---|
| `O_WRONLY` | Chỉ ghi. Không cần `O_RDWR` vì chương trình không bao giờ đọc lại `dst`. |
| `O_CREATE` | Tạo file nếu chưa tồn tại. Thiếu cờ này thì `cp a b` với `b` chưa có sẽ thất bại. |
| `O_TRUNC` | Cắt file về 0 byte nếu đã tồn tại. |

`O_TRUNC` là cờ dễ bị bỏ sót nhất và hậu quả rất khó thấy: nếu `dst` đang là file **dài** và
`src` là file **ngắn**, thiếu `O_TRUNC` thì phần đuôi của file cũ vẫn còn nguyên sau phần dữ
liệu mới ghi vào. Kết quả là `dst` trở thành một file lai ghép của hai nội dung, trông như copy
thành công nhưng sai. Mục 5 có test chứng minh: copy file 6 byte lên file 2403 byte, sau đó
`dst` đúng 6 byte.

Lưu ý: `open` của xv6 **không nhận tham số mode** như `open` của POSIX (chỉ hai tham số:
đường dẫn và cờ), nên không cần truyền quyền `0644`.

### 2.4 Hàm `writeall` — xử lý `write` ghi thiếu byte

```c
static int
writeall(int fd, char *p, int n)
```

`write()` trả về **số byte thực sự đã ghi**, và con số này có thể nhỏ hơn `n`. Cách viết thông
thường `if(write(fd, buf, n) != n) → lỗi` sẽ báo lỗi oan trong trường hợp ghi thiếu nhưng
không hề có lỗi gì. Vì vậy `writeall` lặp lại từ chỗ còn thiếu: dịch con trỏ `p` lên `w` byte,
giảm `n` đi `w` byte, cho đến khi hết.

Điều kiện `<= 0` (chứ không chỉ `< 0`) chặn thêm trường hợp `write` trả về 0: không ghi được
byte nào mà cũng không báo lỗi, nếu chỉ kiểm `< 0` thì vòng lặp sẽ quay vô hạn.

Trên xv6 thực tế `filewrite` cho file thường luôn ghi đủ hoặc trả `-1`, nên vòng lặp này gần
như luôn chạy đúng một lần. Nhưng đề yêu cầu rõ "chú ý `write` có thể ghi thiếu byte", và đây
là hành vi hợp lệ của `write` theo POSIX (rõ nhất khi đích là pipe đang đầy), nên viết đúng
ngay từ đầu.

`static` giới hạn hàm trong file này, tránh trùng tên khi linker ghép với `ulib.o`, `printf.o`.

### 2.5 Hai lớp kiểm tra an toàn thêm ngoài đề

Khi chạy thử các trường hợp biên, nhóm phát hiện hai lỗi mất dữ liệu mà bản cài đặt theo sát
đề vẫn mắc. Cả hai đều được chặn.

**(a) `cp a a` xóa trắng file.** `O_TRUNC` cắt file ngay tại lúc `open()`, tức là *trước* khi
vòng lặp `read` kịp đọc byte đầu tiên. Nếu `src` và `dst` là cùng một file, thứ tự thực thi là:
mở `a` để đọc → mở `a` với `O_TRUNC` làm `a` thành 0 byte → `read` trả về 0 ngay → kết thúc với
`exit(0)`. Chương trình báo thành công trong khi dữ liệu đã mất hẳn.

Cách chặn: so sánh `src` với `dst` **trước khi mở** `dst`, bằng `fstat` trên fd nguồn và `stat`
trên đường dẫn đích, rồi so cặp `(dev, ino)`:

```c
if(stat(argv[2], &ds) >= 0 && ds.dev == ss.dev && ds.ino == ss.ino){
```

So theo **inode** chứ không so theo chuỗi tên, nên bắt được cả `cp a ./a` — hai tên khác nhau
về mặt văn bản nhưng trỏ về cùng một inode. Nếu so bằng `strcmp(argv[1], argv[2])` thì
`cp a ./a` vẫn lọt và vẫn mất dữ liệu.

Điều kiện `stat(argv[2], &ds) >= 0` đặt trước: `stat` thất bại nghĩa là `dst` chưa tồn tại,
đó là trường hợp bình thường nhất, không phải lỗi.

**(b) `cp . d` tạo ra file rác.** `open()` của xv6 cho phép mở một thư mục ở chế độ chỉ đọc,
và `read()` trên fd đó trả về **nội dung thô của các `struct dirent`**. Không chặn thì
`cp . d` chạy "thành công" và sinh ra một file 1024 byte chứa dữ liệu nhị phân của thư mục.
Cách chặn: `fstat` nguồn rồi kiểm `ss.type == T_DIR`.

Hai kiểm tra này nằm ngoài đặc tả của đề. Nhóm vẫn thêm vì chúng chặn **mất dữ liệu**, và
chi phí chỉ là một lần `fstat` cộng một lần `stat` trước khi copy. `cp` của UNIX thật cũng báo
lỗi ở đúng hai trường hợp này, với thông báo tương tự.

### 2.6 Kết thúc bằng `exit`, không bằng `return`

Mọi nhánh đều kết thúc bằng `exit(0)` khi thành công hoặc `exit(1)` khi lỗi. Chương trình
userspace của xv6 không có runtime C chuẩn để hứng giá trị trả về của `main`: `user/user.ld`
đặt điểm vào thẳng ở `main`, nên `return` từ `main` sẽ nhảy vào một địa chỉ rác. Đề cũng yêu
cầu rõ dùng `exit`.

Mọi thông báo lỗi ghi ra **file descriptor 2** bằng `fprintf(2, ...)` theo quy ước UNIX
(0 = stdin, 1 = stdout, 2 = stderr), để khi người dùng chuyển hướng output thì lỗi vẫn hiện
trên màn hình thay vì lẫn vào file.

### 2.7 Đóng file descriptor trên mọi nhánh lỗi

Mỗi nhánh `exit(1)` sau khi đã mở file đều `close()` các fd đang giữ. Về mặt kỹ thuật việc này
không bắt buộc — `exit()` khiến kernel chạy `proc_freefiles` và đóng hết fd của tiến trình.
Nhóm vẫn viết tường minh để thể hiện rõ quyền sở hữu tài nguyên, và vì cùng một đoạn code nếu
sau này chuyển vào một hàm được gọi nhiều lần (ví dụ `cp` nhiều file một lượt) thì rò rỉ fd sẽ
thành lỗi thật.

---

## 3. Luồng chạy

Trường hợp `cp a b` chạy đúng, với `a` dài 6 byte:

```
Người dùng gõ: cp a b
        |
        v
sh tách thành ["cp", "a", "b"], fork + exec
        |
        v
main(): argc = 3, argv[1] = "a", argv[2] = "b"
        |
        +--> argc != 3 ?  KHÔNG --> đi tiếp
        |
        +--> open("a", O_RDONLY) --> src = 3   (0,1,2 đã dùng cho console)
        |
        +--> fstat(src, &ss) --> ss.type = T_FILE, ss.size = 6, ss.ino = <inode của a>
        |
        +--> ss.type == T_DIR ?  KHÔNG --> đi tiếp
        |
        +--> stat("b", &ds):
        |       b chưa tồn tại -> trả về -1 -> điều kiện sai -> đi tiếp
        |       (nếu b tồn tại và cùng inode với a -> báo lỗi, thoát)
        |
        +--> open("b", O_CREATE|O_WRONLY|O_TRUNC) --> dst = 4
        |       kernel tạo inode mới cho b, hoặc cắt b về 0 byte nếu đã có
        |
        +--> vòng lặp copy:
        |       read(src, buf, 512) --> 6   -> writeall(dst, buf, 6) --> ghi đủ 6 byte
        |       read(src, buf, 512) --> 0   -> hết file, thoát vòng lặp
        |
        +--> n < 0 ?  KHÔNG (n == 0 là EOF bình thường) --> đi tiếp
        |
        v
close(src); close(dst); exit(0)  -> sh in lại dấu nhắc $
```

Với file lớn hơn buffer, chỉ khác ở số vòng lặp. `README` dài 2403 byte:
`read` trả về 512, 512, 512, 512, 355, rồi 0 — tức 5 vòng ghi cộng một lần đọc EOF.

Phân biệt `n == 0` và `n < 0` là chỗ dễ sai: `read` trả `0` nghĩa là **hết file**, trạng thái
bình thường để kết thúc; chỉ `n < 0` mới là lỗi đọc. Nếu viết `while((n = read(...)) >= 0)`
thì vòng lặp không bao giờ dừng.

---

## 4. Bảng các nhánh lỗi

| Lệnh | Nhánh rẽ ở đâu | Thông báo | Mã thoát |
|---|---|---|---|
| `cp` | kiểm `argc != 3` | `usage: cp src dst` | 1 |
| `cp a` | kiểm `argc != 3` | `usage: cp src dst` | 1 |
| `cp a b c` | kiểm `argc != 3` | `usage: cp src dst` | 1 |
| `cp nofile out` | `open(src)` trả về < 0 | `cp: cannot open nofile` | 1 |
| `cp a .` | `open(dst)` trả về < 0 (xv6 không cho mở thư mục để ghi) | `cp: cannot open .` | 1 |
| `cp . d` | `ss.type == T_DIR` | `cp: . is a directory` | 1 |
| `cp a a` | `(dev, ino)` của src và dst giống nhau | `cp: a and a are the same file` | 1 |
| `cp a ./a` | như trên, bắt được nhờ so inode | `cp: a and ./a are the same file` | 1 |
| lỗi `fstat` nguồn | `fstat` trả về < 0 | `cp: cannot stat <src>` | 1 |
| `write` thất bại giữa lúc copy | `writeall` trả về < 0 | `cp: write error` | 1 |
| `read` thất bại giữa lúc copy | `n < 0` sau vòng lặp | `cp: read error` | 1 |
| `cp a b` hợp lệ | không rẽ | (không in gì) | 0 |

Hai thông báo `usage: cp src dst` và `cp: cannot open nofile` khớp **chính xác** từng ký tự với
output mong đợi trong đề.

Thành công thì chương trình **không in gì cả** — đúng quy ước UNIX "no news is good news",
và cũng đúng với transcript trong đề (sau `cp a b` là dấu nhắc `$` ngay).

---

## 5. Kết quả kiểm thử

Không có autograder cho `cp` (`grade-lab-util` chỉ chấm `sleep`, `pingpong`, `primes`, `find`,
`xargs`). Toàn bộ test chạy thủ công trong QEMU qua script `xv6run.py`.

### 5.1 Đúng transcript trong đề

```
python3 xv6run.py 'echo hello > a' 'cp a b' 'cat b' 'cp a' 'cp nofile out' 'cp a b c'
```

Output thật:

```
xv6 kernel is booting

hart 2 starting
hart 1 starting
init: starting sh
$ echo khoi dong
khoi dong
$ echo hello > a
$ cp a b
$ cat b
hello
$ cp a
usage: cp src dst
$ cp nofile out
cp: cannot open nofile
$ cp a b c
usage: cp src dst
$
```

Ba dòng đầu của đề (`cp a b` → `cat b` → `hello`) và hai dòng lỗi khớp từng ký tự.
Lệnh `cp a b c` là trường hợp thừa đối số, đề không nêu nhưng cũng được chặn.

### 5.2 File rỗng, file lớn hơn buffer, ghi đè, và hai lớp kiểm tra an toàn

```
python3 xv6run.py 'grep zzzz README > e' 'cp e e2' 'wc e2' 'cp README r2' \
    'wc README' 'wc r2' 'echo hello > a' 'cp a r2' 'wc r2' 'cp a a' \
    'cp a ./a' 'wc a' 'cp . d' 'cp a .'
```

Output thật:

```
xv6 kernel is booting

hart 1 starting
hart 2 starting
init: starting sh
$ echo khoi dong
khoi dong
$ grep zzzz README > e
$ cp e e2
$ wc e2
0 0 0 e2
$ cp README r2
$ wc README
49 337 2403 README
$ wc r2
49 337 2403 r2
$ echo hello > a
$ cp a r2
$ wc r2
1 1 6 r2
$ cp a a
cp: a and a are the same file
$ cp a ./a
cp: a and ./a are the same file
$ wc a
1 1 6 a
$ cp . d
cp: . is a directory
$ cp a .
cp: cannot open .
$
```

Đọc kết quả:

| Test | Cách tạo tình huống | Kết luận |
|---|---|---|
| **File rỗng** | `grep zzzz README > e` — `grep` không tìm thấy gì nên tạo ra `e` dài 0 byte | `cp e e2` chạy xong, `wc e2` cho `0 0 0` → vòng lặp `read` chạy 0 lần, không báo lỗi oan |
| **File lớn hơn buffer** | `cp README r2`, `README` dài 2403 byte = 5 lần buffer 512 | `wc r2` khớp hoàn toàn `wc README`: `49 337 2403` → ghép 5 lần đọc/ghi đúng, không mất và không lặp byte nào |
| **Ghi đè có `O_TRUNC`** | `r2` đang dài 2403 byte, copy `a` (6 byte) lên nó | `wc r2` cho `1 1 6` → file bị cắt về đúng 6 byte, không còn phần đuôi 2397 byte của `README` |
| **`src` trùng `dst`** | `cp a a` và `cp a ./a` | Báo lỗi, và `wc a` sau đó vẫn cho `1 1 6` → **file nguồn còn nguyên**, không bị `O_TRUNC` xóa |
| **Nguồn là thư mục** | `cp . d` | Báo lỗi thay vì tạo file rác 1024 byte |
| **Đích là thư mục** | `cp a .` | `open(dst)` thất bại → `cp: cannot open .` |

### 5.3 Kiểm nội dung file lớn bằng `cat`

`wc` chỉ so khớp số byte; để chắc chắn từng byte đúng chứ không phải chỉ đúng kích thước:

```
python3 xv6run.py 'cp README r2' 'cat r2'
```

Output thật (trích đầu và cuối, phần giữa là danh sách người đóng góp của xv6):

```
$ cp README r2
$ cat r2
xv6 is a re-implementation of Dennis Ritchie's and Ken Thompson's Unix
Version 6 (v6).  xv6 loosely follows the structure and style of v6,
but is implemented for a modern RISC-V multiprocessor using ANSI C.

ACKNOWLEDGMENTS
...
BUILDING AND RUNNING XV6

You will need a RISC-V "newlib" tool chain from
https://github.com/riscv/riscv-gnu-toolchain, and qemu compiled for
riscv64-softmmu.  Once they are installed, and in your shell
search path, you can run "make qemu".
$
```

Nội dung `r2` trùng khớp `README`, kể cả dòng cuối và ký tự xuống dòng cuối file. Đây là bằng
chứng cho thấy chỗ ghép giữa các lần `read` 512 byte không bị lỗi lệch byte.

### 5.4 Lệnh build đã dùng

```sh
make TOOLPREFIX=riscv64-elf- fs.img
```

Build sạch, không warning nào từ `user/cp.c` (Makefile bật `-Wall -Werror` nên bất kỳ warning
nào cũng sẽ làm build dừng). Kiểm `ls -l user/_cp` xác nhận ELF đã được tạo và `mkfs/mkfs` đã
đóng gói nó vào `fs.img`.

### 5.5 Chưa kiểm được

- **Nhánh `cp: read error` và `cp: write error`.** Hai nhánh này cần `read`/`write` thất bại
  giữa lúc copy — trên xv6 chỉ xảy ra khi đĩa lỗi hoặc file system hết block. Chưa dựng được
  tình huống đó, nên hai nhánh này chỉ được kiểm bằng đọc code, không có test chạy thật.
- **Nhánh `cp: cannot stat`.** `fstat` trên một fd vừa `open` thành công thì không có lý do gì
  thất bại, nên nhánh này là phòng xa, không có cách kích hoạt.
- **Vòng lặp lặp lại trong `writeall`.** Trên file thường, `filewrite` của xv6 luôn ghi đủ hoặc
  trả `-1`, nên chưa quan sát được lần nào vòng `while` trong `writeall` chạy quá một vòng.

---

## 6. Khó khăn và ghi chú cho báo cáo

**1. `cp a a` xóa mất file — lỗi phát hiện muộn.** Bản cài đặt đầu tiên làm đúng y đề và chạy
đúng cả 5 dòng transcript mẫu. Chỉ khi thử thêm `cp a a` mới thấy file bị xóa trắng mà chương
trình vẫn báo thành công (`exit(0)`, không in gì). Nguyên nhân: `O_TRUNC` cắt file ngay tại
`open()`, trước khi `read` chạy. Bài học: thứ tự mở file quan trọng, và test "copy một file lên
chính nó" là trường hợp biên phải nghĩ tới ngay từ đầu vì hậu quả là **mất dữ liệu im lặng** —
loại lỗi tệ nhất. Đã sửa bằng cách so cặp `(dev, ino)` trước khi mở `dst` (mục 2.5).

**2. So tên file không đủ.** Cách chặn đầu tiên nhóm nghĩ ra là `strcmp(argv[1], argv[2])`.
Test `cp a ./a` cho thấy nó vẫn lọt: hai chuỗi khác nhau nhưng cùng một inode. Phải so bằng
`(dev, ino)` lấy từ `fstat`/`stat` mới đúng. Đây là lý do cần `#include "kernel/stat.h"`.

**3. `open` của xv6 cho mở thư mục để đọc.** Khác với kỳ vọng ban đầu, `cp . d` không báo lỗi
mà sinh ra một file 1024 byte chứa dữ liệu nhị phân của các `struct dirent`. Phải tự kiểm
`ss.type == T_DIR`. Ngược lại, `cp a .` thì `open(dst, O_WRONLY)` tự thất bại vì xv6 không cho
mở thư mục ở chế độ ghi — nên nhánh này không cần kiểm thêm.

**4. Thiếu `kernel/kernel` khi chạy test.** `make TOOLPREFIX=riscv64-elf- fs.img` chỉ build
`fs.img`, **không** build kernel. Lần chạy `xv6run.py` đầu tiên QEMU tắt ngay với
`BrokenPipeError` vì không có `kernel/kernel`. Phải build thêm:
`make TOOLPREFIX=riscv64-elf- kernel/kernel` (hoặc chạy thẳng `make qemu`, target này build cả
hai). Ghi chú này đã có trong `CLAUDE.md` cho `fs.img`, nhưng kernel cũng nằm ngoài target mặc
định.

**5. Ký tự đầu tiên bị console xv6 ăn mất.** Giống hệt vấn đề đã gặp khi test `sleep`: lệnh đầu
tiên đẩy qua stdin của QEMU luôn mất ký tự đầu. `xv6run.py` đã tự chèn lệnh mồi
`echo khoi dong` ở đầu để hứng phần mất mát — đó là dòng `khoi dong` xuất hiện trong mọi output
ở mục 5, không phải một phần của test.

**6. QEMU giữ khóa ghi trên `fs.img`.** Chỉ một tiến trình QEMU được mở `fs.img` một lúc. Nếu
một lần chạy trước chưa tắt hẳn, lần sau báo `Failed to get write lock`. Kiểm bằng
`pgrep -f qemu-system-riscv64` và tắt tiến trình còn sót trước khi chạy lại.

**7. Quyết định làm nhiều hơn đề yêu cầu.** Đề chỉ đòi chặn thiếu đối số và lỗi mở/đọc/ghi.
Nhóm thêm hai kiểm tra (nguồn là thư mục, nguồn trùng đích) và hàm `writeall`. Đánh đổi: thêm
khoảng 20 dòng, bù lại không có đường nào làm mất dữ liệu người dùng. Ba thông báo lỗi mới
(`is a directory`, `are the same file`, `cannot stat`) được đặt theo đúng văn phong của `cp`
UNIX thật; hai thông báo mà đề chỉ định (`usage: cp src dst`, `cp: cannot open <file>`) giữ
nguyên từng ký tự.

**8. Môi trường build.** Như đã nêu trong `docs/sleep.md`: máy dùng macOS, toolchain prefix là
`riscv64-elf-` nên mọi lệnh `make` phải truyền `TOOLPREFIX=riscv64-elf-`, và `Makefile` đã được
thêm một dòng `CFLAGS += -std=gnu17 -Wno-unused-but-set-variable` để build được với GCC 16.
Phần `cp` **không** cần sửa gì thêm trong `Makefile` ngoài đúng một dòng `$U/_cp\` trong danh
sách `UPROGS`.
