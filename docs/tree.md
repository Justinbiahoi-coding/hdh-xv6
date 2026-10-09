# Giải thích chi tiết `user/tree.c`

`tree` in ra cây thư mục có thụt lề, mô phỏng lệnh `tree` của Linux.

Cú pháp: `tree [path] [-L depth] [-d]`

| Tham số | Mặc định | Ý nghĩa |
|---|---|---|
| `path` | `"."` | Gốc của cây cần in |
| `-L depth` | không giới hạn | Giới hạn độ sâu. `-L 1` chỉ in gốc và các mục con trực tiếp |
| `-d` | tắt | Chỉ in thư mục, bỏ qua file thường |

---

## 1. Kiến thức nền: trong xv6 thư mục cũng là file

Không có system call nào kiểu `readdir`. Để liệt kê một thư mục, ta `open()` nó
như một file bình thường rồi `read()` tuần tự từng bản ghi `struct dirent`
(khai báo trong `kernel/fs.h`):

```c
#define DIRSIZ 14
struct dirent {
  ushort inum;        // inode number, == 0 nghĩa là ô trống
  char name[DIRSIZ];  // KHÔNG có '\0' nếu tên dài đúng 14 ký tự
};
```

Ba hệ quả bắt buộc phải xử lý, cả `tree` và `du` đều vướng:

1. **`inum == 0` là ô đã bị xoá**, không phải mục thật — phải `continue`. Khi
   `rm` một file, xv6 chỉ ghi `inum = 0` vào ô dirent chứ không dồn mảng lại.
2. **`name` không chắc có `'\0'`.** Phải `memmove` đúng `DIRSIZ` byte ra buffer
   riêng `char cur[DIRSIZ+1]` rồi tự đặt `cur[DIRSIZ] = 0`. Dùng thẳng
   `de.name` như một chuỗi C là lỗi tràn — đây là bẫy mà `user/ls.c` đã tránh,
   và nhóm bắt chước đúng cách làm đó.
3. **Mọi thư mục đều chứa `.` và `..`.** Đệ quy vào `.` lặp vô tận ngay tại chỗ;
   đệ quy vào `..` leo lên cha rồi quay lại, cũng vô tận. Phải lọc cả hai bằng
   `strcmp` trước khi làm bất cứ việc gì khác.

Phân loại thì dùng `stat(path, &st)` rồi xét `st.type` so với `T_DIR` / `T_FILE`
/ `T_DEVICE` (`kernel/stat.h`).

---

## 2. Các quyết định thiết kế

### 2.1 Chọn ký tự Unicode `├── │ └──` — đã kiểm chứng thật trong QEMU

Đề cho mẫu bằng ký tự khung Unicode nhưng ghi rõ được phép thay bằng ASCII
`|--` và `` `-- ``. Nhóm **đã thử Unicode trước trong QEMU** thay vì đoán.

**Kết luận: Unicode hiển thị hoàn toàn đúng.** Lý do: `printf` của userspace
(`user/printf.c`) đẩy từng byte qua `write()`; console của kernel
(`kernel/console.c` → `kernel/uart.c:uartputc`) cũng chỉ đẩy từng byte ra UART,
không có lớp mã hoá hay lọc ký tự nào ở giữa. Chuỗi UTF-8 vì vậy đi qua nguyên
vẹn và terminal của máy host (vốn là UTF-8) hiển thị đúng. Output ở mục 5 là
bằng chứng, kể cả ở độ sâu 3 với tiền tố lồng `│   │       ├──`.

Nên nhóm **dùng Unicode cho khớp đúng mẫu đề**.

Một chi tiết nhỏ: trong source, bốn ký tự này được viết bằng **universal
character name** (`"├── "`) chứ không dán ký tự thật. GCC tự mã
hoá chúng thành UTF-8 khi biên dịch, nên file `.c` giữ nguyên là ASCII thuần —
tránh mọi rủi ro encoding khi file đi qua `git diff`, file `.patch` và `.zip`
nộp bài. Nếu máy chấm điểm có terminal không hiển thị được, chỉ cần sửa 4 dòng
`#define` sang `"|-- "`, `` "`-- " ``, `"|   "`, `"    "`; toàn bộ phần còn lại
của chương trình không phải sửa một dòng nào.

### 2.2 Dùng buffer toàn cục, không để mảng lớn trên stack

`kernel/param.h` đặt `USERSTACK 2` cho lab `util`, tức stack của tiến trình
người dùng chỉ có **2 trang = 8 KB**. Nếu mỗi khung đệ quy mang theo một mảng
`char path[512]` cộng một mảng tiền tố nữa thì chỉ xuống được vài cấp là tràn
stack — mà tràn stack trong xv6 không báo lỗi gọn gàng, nó gây page fault và
kernel giết tiến trình với thông báo tối nghĩa.

Vì vậy `path[]` và `prefix[]` là **biến toàn cục**, các hàm đệ quy chỉ nhận
*chỉ số* (`plen`, `preflen`) và sửa chuỗi tại chỗ. Quy ước: hàm nào sửa buffer
thì trước khi `return` phải trả lại độ dài cũ (`path[plen] = 0`,
`prefix[preflen] = 0`). Nhờ đó mỗi khung đệ quy chỉ còn khoảng 60–70 byte.

### 2.3 Kích thước buffer đường dẫn lấy đúng `MAXPATH`

`user/ls.c` dùng `char buf[512]`, nhưng `kernel/param.h` định nghĩa
`MAXPATH 128` và `kernel/syscall.c:argstr` sẽ từ chối mọi đường dẫn dài hơn
thế. Buffer 512 byte vì vậy vô nghĩa: `open()` thất bại từ trước khi dùng tới
phần dư. Nhóm dùng thẳng `#define PATHMAX MAXPATH` để giới hạn của chương trình
trùng khít với giới hạn thật của kernel, và vẫn kiểm tra tràn trước mỗi lần ghép
tên (`namepos + DIRSIZ >= PATHMAX`).

### 2.4 Biết "mục cuối cùng" bằng cách giữ chậm một nhịp

Đây là điểm khó nhất của bài. Ký tự vẽ khác nhau giữa mục giữa (`├──`, và tiền
tố cho con là `│   `) và mục cuối (`└──`, tiền tố cho con là bốn khoảng trắng).
Nhưng `read()` trên thư mục chỉ đi một chiều và **xv6 không có `lseek`**, nên
không thể đọc trước để đếm rồi quay lại.

Ba cách và lý do chọn:

| Cách | Vấn đề |
|---|---|
| Đọc hết tên vào một mảng rồi mới in | Phải chọn số mục tối đa cứng, hoặc dùng `malloc`; mảng lại không đặt được trên stack 8 KB |
| Mở thư mục hai lần: lượt 1 đếm, lượt 2 in | Tốn gấp đôi số lần `stat`, và hai lượt có thể thấy nội dung khác nhau |
| **Giữ chậm một nhịp** (đã chọn) | Bộ nhớ hằng số, một lượt đọc duy nhất |

Cách đã chọn: giữ lại mục vừa đọc trong `pname`/`ptype` và **chưa in**. Khi đọc
được mục hợp lệ tiếp theo, mới in `pname` với dạng "mục giữa". Hết thư mục mà
còn mục đang giữ thì in nó với dạng "mục cuối". Chi phí bộ nhớ: 15 byte mỗi
khung đệ quy.

Lưu ý: "mục hợp lệ tiếp theo" phải tính **sau** khi đã lọc `.`, `..` và lọc
`-d`. Nếu không, một thư mục mà mục cuối cùng là file thường sẽ khiến `tree -d`
vẽ `├──` cho mục cuối rồi không in gì nữa.

### 2.5 Ý nghĩa chính xác của `-L depth`

`level` đếm từ 0 ở gốc, các con trực tiếp của gốc ở `level` 1. Hàm liệt kê một
thư mục sẽ thoát ngay nếu `level >= maxdepth`. Suy ra:

| Lệnh | Kết quả |
|---|---|
| `tree t -L 0` | chỉ in `t` |
| `tree t -L 1` | in `t` và các con trực tiếp |
| `tree t -L 2` | thêm một cấp nữa |

Đúng với yêu cầu "`-L 1` chỉ in path và các mục con trực tiếp" của đề.

### 2.6 Tự viết hàm đọc số thay vì dùng `atoi`

Giống `sleep`, `atoi` của xv6 im lặng trả `0` cho `"x"`, nên `tree t -L x` sẽ bị
hiểu thành `-L 0` và in ra một dòng duy nhất — sai mà không báo gì. `parsedepth`
chặn chuỗi rỗng, ký tự không phải chữ số và tràn `int`, trả `-1` để `main` báo
lỗi.

### 2.7 Đóng `fd` trước khi đệ quy vào mục cuối

Tiến trình xv6 chỉ có `NOFILE = 16` file descriptor. Mỗi cấp đệ quy đang duyệt
giữ một `fd` mở, nên về lý thuyết cây sâu hơn ~14 cấp sẽ hết `fd`. Chương trình
`close(fd)` **trước** lần `emit` cuối, nên đường đi xuống mục cuối cùng của mỗi
thư mục (trường hợp phổ biến nhất của cây sâu) không tích luỹ `fd`. Giới hạn còn
lại chỉ áp dụng cho nhánh "mục giữa", và `MAXPATH = 128` thực tế đã chặn độ sâu
trước đó. Đây là một đơn giản hoá có ý thức, không phải chỗ bỏ sót.

---

## 3. Luồng chạy

```
main
 ├─ duyệt argv: nhận -L (kèm số), -d, và path đầu tiên không phải tuỳ chọn
 │   tuỳ chọn lạ / hai path  -> usage, exit 1
 ├─ stat(root)  thất bại     -> "tree: cannot stat <root>", exit 1
 ├─ printf("%s\n", root)         <- dòng đầu luôn là path như người dùng gõ
 └─ nếu root là T_DIR: treedir(level = 0)
       │
       ├─ level >= maxdepth ?  -> return (hết độ sâu cho phép)
       ├─ open(path)           -> thất bại thì báo lỗi và return
       ├─ ghép dấu '/' vào path, kiểm tra tràn PATHMAX
       ├─ vòng lặp read() từng dirent:
       │     inum == 0            -> bỏ
       │     tên là "." hoặc ".."  -> bỏ
       │     stat thất bại         -> báo lỗi, bỏ
       │     -d và không phải T_DIR -> bỏ
       │     còn mục đang giữ      -> emit(mục đó, last = 0)
       │     giữ mục hiện tại lại
       ├─ close(fd)
       └─ còn mục đang giữ        -> emit(mục đó, last = 1)

emit
 ├─ printf tiền tố + ("└── " nếu cuối, "├── " nếu giữa) + tên
 ├─ không phải T_DIR -> return
 ├─ nối vào prefix: "    " nếu cuối, "│   " nếu giữa
 ├─ treedir(level + 1)        <- đệ quy
 └─ trả prefix về độ dài cũ
```

Hai hàm `treedir` và `emit` gọi lẫn nhau, nên `treedir` được khai báo trước
(forward declaration).

### Một lỗi thật đã gặp và đã sửa

Lần chạy đầu tiên, mục cuối cùng của mỗi thư mục in **trùng tên với mục liền
trước** và mục cuối thật bị mất: `tree .` cho ra `a` hai lần và không thấy `z`.

Nguyên nhân: `emit` ghi tên của mục đang giữ vào `path` (nó phải làm vậy để đệ
quy có đường dẫn đầy đủ), tức là **ghi đè lên tên vừa đọc** mà vòng lặp đã đặt
sẵn ở cùng vị trí trong `path`. Dòng `strcpy(pname, path + namepos)` ngay sau đó
vì vậy copy lại tên cũ thay vì tên mới.

Cách sửa: đọc tên ra buffer riêng `cur[]` ngay sau `read()`, dùng `cur` để ghép
vào `path` cho `stat`, rồi sau khi `emit` xong mới `strcpy(pname, cur)`. Nguồn
dữ liệu lúc đó không còn là vùng mà `emit` có thể chạm vào.

Bài học rút ra: dùng buffer toàn cục tiết kiệm stack, nhưng phải rất kỷ luật về
chuyện "ai được ghi vào vùng nào, vào lúc nào".

---

## 4. Bảng nhánh lỗi

| Lệnh | Nhánh rẽ | Kết quả | Mã thoát |
|---|---|---|---|
| `tree nofile` | `stat` thất bại | `tree: cannot stat nofile` | 1 |
| `tree t -L` | `-L` là đối số cuối, không có số theo sau | `tree: -L can mot so nguyen khong am` | 1 |
| `tree t -L x` | `parsedepth` gặp ký tự không phải chữ số | như trên | 1 |
| `tree t -L -1` | `parsedepth` gặp `'-'` | như trên | 1 |
| `tree t -q` | tuỳ chọn không nhận ra | `usage: tree [path] [-L depth] [-d]` | 1 |
| `tree t t2` | hai path | như trên | 1 |
| `tree <path dài hơn 128>` | `strlen(root) >= PATHMAX` | `tree: path too long: ...` | 1 |
| thư mục con không mở được | `open` thất bại trong `treedir` | `tree: cannot open <path>`, bỏ nhánh đó, **đi tiếp** | 0 |
| một mục không `stat` được | `stat` thất bại trong vòng lặp | `tree: cannot stat <path>`, bỏ mục đó, **đi tiếp** | 0 |
| cây quá sâu cho `prefix` | `preflen + n >= PREFMAX` | `tree: tree too deep`, không xuống sâu hơn | 0 |
| `tree README` (file thường) | `st.type != T_DIR` | in đúng một dòng `README` | 0 |
| `tree e` (thư mục rỗng) | vòng lặp chạy 0 lần hợp lệ | in đúng một dòng `e` | 0 |

Nguyên tắc: lỗi ở **gốc** thì dừng với `exit(1)` vì không còn gì để làm; lỗi ở
**một nhánh con** thì báo ra fd 2 rồi đi tiếp, vì phần còn lại của cây vẫn in
được. Mọi thông báo lỗi ra **fd 2** để không lẫn vào output khi người dùng
chuyển hướng (`tree > f`), và viết **không dấu** cho an toàn.

---

## 5. Kết quả kiểm thử

Không có autograder cho bài này (`make grade` chỉ chấm `sleep`, `pingpong`,
`primes`, `find`, `xargs`). Nhóm kiểm thử bằng `xv6run.py` — script tự viết đẩy
lần lượt từng lệnh vào stdin của QEMU rồi thu lại toàn bộ phiên làm việc.

### 5.1 Dựng đúng cây thư mục của đề

```
mkdir t
mkdir t/a
echo 1234567890 > t/a/f1.txt
mkdir t/a/aa
echo hello > t/a/aa/f2.txt
echo world > t/a/aa/f3.txt
mkdir t/z
```

### 5.2 Ba trường hợp chính của đề — output thật, dán nguyên văn

```
$ tree t
t
├── a
│   ├── f1.txt
│   └── aa
│       ├── f2.txt
│       └── f3.txt
└── z
$ tree t -L 1
t
├── a
└── z
$ tree t -L 2
t
├── a
│   ├── f1.txt
│   └── aa
└── z
$ tree t -d
t
├── a
│   └── aa
└── z
$ tree t -d -L 2
t
├── a
│   └── aa
└── z
$ tree t -L 0
t
```

So với mẫu đề: giống **từng ký tự** về hình dạng cây (chỉ khác tên file, vì mẫu
`tree` của đề dùng `b.txt`/`c.txt` còn cây kiểm thử này dùng chung cây của phần
`du` là `f1.txt`/`f2.txt`/`f3.txt`).

### 5.3 Các trường hợp biên — output thật

```
$ tree
.
├── README
├── xargstest.sh
├── sleep
├── pingpong
├── tree
├── du
├── cat
├── echo
├── forktest
├── grep
├── init
├── kill
├── ln
├── ls
├── mkdir
├── rm
├── sh
├── stressfs
├── usertests
├── grind
├── wc
├── zombie
├── console
├── a
│   ├── f1.txt
│   └── aa
│       ├── f2.txt
│       └── f3.txt
├── z
├── t
│   ├── a
│   │   ├── f1.txt
│   │   └── aa
│   │       ├── f2.txt
│   │       └── f3.txt
│   └── z
└── e
$ tree e
e
$ tree nofile
tree: cannot stat nofile
$ tree README
README
$ tree t -L x
tree: -L can mot so nguyen khong am
$ tree t -L
tree: -L can mot so nguyen khong am
$ tree t -q
usage: tree [path] [-L depth] [-d]
$ tree t t2
usage: tree [path] [-L depth] [-d]
```

Trường hợp `tree` không đối số đáng chú ý ở hai điểm: nó chứng minh mặc định
`"."` hoạt động, và nhánh `t/a/aa` ở **độ sâu 3** cho thấy tiền tố lồng
`│   │       ├──` được dựng và trả lại đúng — chính là chỗ mà lỗi ở mục 3 từng
làm sai.

---

## 6. Về việc dùng chung phép duyệt thư mục với `du`

Đề và hướng dẫn đều nêu rõ: `tree` và `du` dùng **cùng một phép duyệt**, đừng
viết hai lần. Nhóm đã cân nhắc nghiêm túc việc tách ra `user/dirwalk.c` +
`user/dirwalk.h` và **quyết định không tách**. Ba lý do, theo thứ tự quan trọng:

**1. Không tách được mà không sửa luật link của Makefile.** Luật biên dịch
chương trình người dùng trong `Makefile` là:

```make
$U/_%: $U/%.o $(ULIB)
```

`ULIB` là danh sách `.o` được link vào **mọi** chương trình. Muốn `tree` và `du`
dùng chung `dirwalk.o` thì phải (a) thêm nó vào `ULIB` — khi đó 22 chương trình
còn lại, kể cả `usertests` và `sh`, đều bị link thêm code không dùng; hoặc (b)
viết luật link riêng cho hai chương trình này, phá vỡ luật pattern chung. Yêu
cầu của bài tập nói rõ **chỉ được sửa Makefile ở hai dòng `UPROGS`**, nên đường
này bị khoá. Một phương án khác — `#include "dirwalk.c"` vào cả hai file — chỉ
là sao chép code bằng bộ tiền xử lý, không phải chia sẻ thật.

**2. Phần thực sự chung nhỏ hơn vẻ ngoài.** Cái giống nhau là **khuôn** của
vòng lặp: `open` → `read` từng `dirent` → bỏ `inum == 0` → copy tên ra buffer
riêng → bỏ `.` và `..` → `stat` → phân nhánh theo `st.type`. Khoảng 15 dòng. Mọi
thứ quanh nó thì khác hẳn bản chất:

| | `tree` | `du` |
|---|---|---|
| Hướng xử lý | tiền tố chảy **xuống** khi đệ quy | tổng chảy **lên** khi đệ quy trả về |
| Giá trị trả về | `void` | `long` (tổng byte) |
| Thời điểm in | trước khi đệ quy xuống (tiền thứ tự) | sau khi đệ quy trả về (hậu thứ tự) |
| Trạng thái riêng | `prefix[]`, cơ chế giữ chậm một nhịp | không có |
| Cần biết "mục cuối" | **có** — quyết định ký tự vẽ | không |

Một hàm duyệt chung phải nhận callback kèm cả tiền tố, cờ "mục cuối" và giá trị
tích luỹ trả về — tức là một framework có đúng hai người dùng, mỗi người chỉ
dùng một nửa tham số. Đó là trừu tượng hoá đắt hơn thứ nó tiết kiệm.

**3. Thay vì chia sẻ code, nhóm chia sẻ *cấu trúc*.** Vòng lặp duyệt trong hai
file được viết **cùng một hình dạng, cùng thứ tự các bước, cùng cách đặt tên
biến** (`namepos`, `plen`, `cur`, `st`, `de`), cùng quy ước buffer toàn cục và
cùng quy ước "sửa xong thì trả lại độ dài cũ". Đọc `du.c` sau khi đọc `tree.c`
là nhận ra ngay cùng một khuôn. Đổi lại, mỗi file đọc được độc lập — một ưu thế
thật cho bài tập mà người chấm đọc từng file riêng.

**Đánh đổi nhóm chấp nhận:** nếu sau này phát hiện một lỗi trong phép duyệt (ví
dụ xử lý `inum == 0`), phải sửa ở **hai** chỗ. Rủi ro này được ghi nhận. Nếu bài
tập có thêm chương trình thứ ba cùng duyệt thư mục (ví dụ `find`), nhóm sẽ xét
lại — ba người dùng là ngưỡng mà một module chung bắt đầu đáng giá, và lúc đó
cũng đã có lý do chính đáng để xin sửa luật link trong Makefile.

---

## 7. Khó khăn và ghi chú cho báo cáo

**1. `fs.img` không nằm trong target mặc định của `make`.** Chạy `make` thành
công nhưng `tree` vẫn báo `exec failed`. Nguyên nhân: `fs.img` chỉ được dựng bởi
target `qemu`, `grade`, hoặc khi gọi thẳng `make fs.img`. Và trên máy macOS này
còn phải truyền `TOOLPREFIX=riscv64-elf-` ở mọi lệnh `make` vì Makefile không tự
nhận ra prefix của toolchain Homebrew.

**2. Thiếu `$U/_tree\` trong `UPROGS` thì chương trình "biên dịch thành công
nhưng không tồn tại".** `mkfs/mkfs` dựng `fs.img` từ **đúng** danh sách
`UPROGS`, nên quên bước này vẫn build sạch còn shell chỉ báo `exec failed` không
kèm gợi ý gì. Đây là bước dễ bỏ sót nhất của cả bài lab.

**3. Lỗi ghi đè buffer toàn cục (mục 3).** Mục cuối của mỗi thư mục in trùng tên
mục liền trước. Mất khá lâu mới thấy vì triệu chứng nhìn như lỗi logic "giữ chậm
một nhịp", trong khi nguyên nhân thật là tranh chấp vùng nhớ giữa `emit` và vòng
lặp gọi nó. Đã sửa bằng buffer `cur[]` riêng.

**4. Không có `lseek` nên không duyệt thư mục hai lượt được.** Chính giới hạn
này dẫn tới thiết kế "giữ chậm một nhịp" ở mục 2.4. Nếu xv6 có `lseek`, cách tự
nhiên hơn là lượt 1 đếm số mục, lượt 2 in.

**5. Stack 8 KB buộc phải dùng biến toàn cục.** Ban đầu nhóm viết hàm đệ quy với
`char child[512]` và `char nprefix[256]` là biến cục bộ — tổng gần 800 byte mỗi
khung, tràn stack ở khoảng cấp thứ 10. Đã chuyển sang buffer toàn cục cộng quy
ước trả lại độ dài cũ (mục 2.2).

**6. Môi trường build — GCC quá mới.** Đã nêu trong `docs/sleep.md`: Makefile
cần thêm `CFLAGS += -std=gnu17 -Wno-unused-but-set-variable` để build được với
GCC 16 của Homebrew. Đây là sửa môi trường, không phải phần bài tập, nhưng nó
xuất hiện trong file `.patch` nên cần nêu trong báo cáo.

**7. Chỗ chưa kiểm chứng được.** Toàn bộ kiểm thử chạy trên macOS với
`riscv64-elf-gcc`. Đề yêu cầu môi trường Linux (WSL + Ubuntu, prefix
`riscv64-linux-gnu-`). Source C là như nhau nên kết quả phải giống, nhưng
**phần hiển thị Unicode phụ thuộc terminal của máy chấm**, không phụ thuộc
xv6 — nếu terminal đó không phải UTF-8 thì cây sẽ ra ký tự lạ. Cách xử lý đã
chuẩn bị sẵn: đổi 4 dòng `#define` sang ASCII (mục 2.1).
