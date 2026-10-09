# Giải thích chi tiết `user/du.c`

`du` (*disk usage*) tính và in tổng dung lượng theo byte của từng thư mục trong
cây, đệ quy từ dưới lên.

Cú pháp: `du [path] [-a] [-s]`

| Tham số | Mặc định | Ý nghĩa |
|---|---|---|
| `path` | `"."` | Gốc của cây cần tính |
| `-a` | tắt | In **cả file** lẫn thư mục (mặc định chỉ in thư mục) |
| `-s` | tắt | Chỉ in **một dòng tổng** của `path`, bỏ hết dòng trung gian |

Định dạng mỗi dòng: `<bytes>\t<path>`.

---

## 1. QUY ƯỚC TÍNH KÍCH THƯỚC THƯ MỤC

Đề cho phép bỏ qua kích thước của chính inode thư mục, miễn là nhất quán và ghi
rõ. Nhóm chọn:

> **Tổng của một thư mục = tổng kích thước của các mục bên trong nó (đệ quy).
> KHÔNG cộng `st.size` của chính inode thư mục đó.**

Áp dụng nhất quán ở **mọi cấp**, kể cả cấp gốc.

### Vì sao chọn như vậy

Trong xv6, một thư mục cũng là một file và nó có `st.size` khác 0: đó là kích
thước của mảng `struct dirent` lưu danh sách mục con. Mỗi `dirent` là 16 byte,
và `kernel/fs.c` cấp phát theo block 1024 byte, nên một thư mục rỗng vừa tạo
vẫn có `st.size` cỡ 32–1024 byte.

Nếu cộng phần này vào, một thư mục rỗng sẽ không bao giờ ra 0. Mà **mẫu của đề
ghi rõ `0 ./z` cho thư mục rỗng `z`**. Vậy mẫu đề chính là quy ước "không cộng
inode thư mục", và nhóm làm theo.

Hệ quả cần biết khi đọc output: con số nhóm in ra là **tổng dung lượng dữ liệu
người dùng**, không phải dung lượng thật chiếm trên đĩa. `du` của Linux đếm
block đã cấp phát (nên mới có tuỳ chọn `--apparent-size` để làm điều ngược lại).
Phiên bản này luôn báo theo kiểu `--apparent-size` và không cộng metadata.

### Về việc con số khác mẫu đề

Mẫu trong đề **không nhất quán với `st.size`**, và đây là chuyện đã lường trước:

| | Mẫu đề | Thực tế đo được |
|---|---|---|
| `a/f1.txt` sau `echo 1234567890 >` | `16` | `11` |
| `a/aa/f2.txt` sau `echo hello >` | `32` | `6` |
| `a/aa` | `64` | `12` |
| `a` | `96` | `23` |

`echo 1234567890` ghi ra đúng 11 byte (10 chữ số + `'\n'`), `echo hello` ghi 6
byte. Các con số của đề (16, 32, 64, 96) đều là bội số của 16 và tăng theo bội
số đẹp — rất giống số làm tròn theo một đơn vị nào đó, hoặc chỉ là ví dụ minh
hoạ để người đọc thấy *thứ tự* và *định dạng* dòng output.

Đề nói rõ **"lấy size từ `st.size`"**, nên nhóm dùng đúng `st.size` thật và
**không chế số cho khớp mẫu**. Cái khớp từng chi tiết với mẫu đề là *thứ tự các
dòng*, *định dạng* `<bytes>\t<path>`, và *tập hợp đường dẫn được in ra* — xem
mục 5.

---

## 2. Kiến thức nền

Giống `tree`, `du` đọc thư mục bằng `open()` + `read()` từng `struct dirent`
(`kernel/fs.h`) và phân loại bằng `stat()` → `st.type` (`kernel/stat.h`). Ba bẫy
của `dirent` — `inum == 0` là ô trống, `name` không chắc có `'\0'`, và phải lọc
`.` với `..` — đã được giải thích đầy đủ ở `docs/tree.md` mục 1.

Riêng với `du`, việc lọc `.` và `..` còn một lý do thứ hai ngoài chuyện lặp vô
tận: `.` trỏ về chính thư mục đang duyệt và `..` trỏ về cha, nên nếu không lọc
thì kích thước của chúng **bị đếm trùng** vào tổng.

Kích thước lấy từ `st.size`, kiểu `uint64`. Tổng được tích luỹ trong `long`
(64 bit trên riscv64) và in bằng `%ld` — `user/printf.c` có hỗ trợ `%ld` (ánh xạ
sang `uint64`), còn `%d` sẽ cắt mất 32 bit cao.

---

## 3. Các quyết định thiết kế

### 3.1 Tách làm hai phần như đề gợi ý: `du()` tính, `main()` in dòng gốc

Đề gợi ý tách thành "hàm `long du(path)` trả tổng bytes" và "phần in theo tuỳ
chọn". Nhóm làm theo tinh thần đó nhưng có một điều chỉnh bắt buộc, vì **thứ tự
in trong mẫu đề là hậu thứ tự**:

```
64      ./a/aa      <- con in trước
96      ./a         <- cha in sau
0       ./z
96      .           <- gốc in cuối cùng
```

Tổng của `./a` chỉ biết được **sau khi** đã đệ quy xong `./a/aa`. Nghĩa là không
thể tính xong toàn bộ rồi mới in — việc in phải xen vào lúc đệ quy trả về.

Cách phân chia đã chọn:

- `du(plen, printall, print)` trả về tổng byte **bên trong** thư mục đang xét,
  và tự in dòng cho **các con của nó** (file chỉ khi có `-a`; thư mục thì luôn,
  ngay sau khi lời gọi đệ quy cho thư mục đó trả về).
- `du()` **không** in dòng của chính nó. Dòng của gốc do `main()` in, sau khi
  `du()` trả về.

Quy ước "cha in dòng của con" này cho ra đúng thứ tự hậu thứ tự của mẫu đề một
cách tự nhiên, và dòng gốc tự động nằm cuối cùng.

### 3.2 `-s` chỉ là một cờ `print`, không phải một nhánh xử lý riêng

`-s` nghĩa là "chỉ in tổng của `path`". Vì `main()` đã là nơi in dòng gốc, `-s`
không cần code riêng: chỉ cần truyền `print = !sflag` xuống `du()` để tắt hết
dòng trung gian. Phép tính vẫn chạy y nguyên.

Hệ quả: `-s` **thắng** `-a` khi dùng chung (`du t -s -a` in một dòng). Giống
`du` của Linux, và không cần thêm dòng code nào để có hành vi đó.

### 3.3 Buffer toàn cục thay vì mảng trên stack

`kernel/param.h` đặt `USERSTACK 2`, tức stack tiến trình chỉ có 8 KB. Một mảng
`char[128]` cục bộ mỗi khung đệ quy sẽ giới hạn độ sâu ở vài chục cấp và tràn
stack một cách khó debug. Nên `path[]` là biến toàn cục, `du()` chỉ nhận chỉ số
`plen` và sửa chuỗi tại chỗ, với quy ước **trước khi `return` phải trả lại độ
dài cũ** (`path[plen] = 0`).

Quy ước này còn mang lại một tiện lợi: ngay sau khi lời gọi `du()` đệ quy trả
về, `path` đang chứa **đúng** đường dẫn của thư mục con vừa xử lý, nên dòng
`printf("%ld\t%s\n", sub, path)` dùng được luôn mà không phải ghép lại.

### 3.4 Kích thước buffer lấy đúng `MAXPATH`

`kernel/syscall.c:argstr` từ chối đường dẫn dài hơn `MAXPATH` (= 128), nên
buffer lớn hơn thế là vô dụng: `open()` thất bại trước khi dùng tới phần dư.
`#define PATHMAX MAXPATH` cho giới hạn chương trình trùng khít với giới hạn
kernel, và vẫn kiểm tra `namepos + DIRSIZ >= PATHMAX` trước mỗi lần ghép tên.

### 3.5 `du` trên một file thường vẫn hợp lệ

Nếu `path` không phải thư mục, `main()` lấy thẳng `st.size` và in một dòng.
`du README` → `2403	README`. Hợp lý và khớp hành vi của `du` Linux.

### 3.6 `T_DEVICE` được tính như file

`st.size` của device trong xv6 là 0, nên nhánh `else` (không phải `T_DIR`) xử lý
đúng cả `T_FILE` và `T_DEVICE` mà không cần phân biệt. Thấy được trong output
`tree`: `/console` là một `T_DEVICE` và nó góp 0 byte.

---

## 4. Luồng chạy

```
main
 ├─ duyệt argv: nhận -a, -s, và path đầu tiên không phải tuỳ chọn
 │   tuỳ chọn lạ / hai path  -> usage, exit 1
 ├─ stat(root) thất bại      -> "du: cannot stat <root>", exit 1
 ├─ root là T_DIR ?
 │     có     -> total = du(strlen(root), printall, !sflag)
 │     không  -> total = st.size
 └─ printf("%ld\t%s\n", total, root)      <- dòng gốc, luôn in, luôn cuối

du(plen, printall, print)  -> long
 ├─ open(path)       -> thất bại thì báo lỗi, return 0
 ├─ ghép dấu '/' vào path, kiểm tra tràn PATHMAX
 ├─ total = 0
 ├─ vòng lặp read() từng dirent:
 │     inum == 0           -> bỏ
 │     tên là "." / ".."   -> bỏ (lặp vô tận + đếm trùng)
 │     stat thất bại       -> báo lỗi, bỏ
 │     T_DIR:
 │        sub = du(...)                   <- đệ quy, in các dòng bên trong
 │        total += sub
 │        print -> in "<sub>\t<path con>" <- cha in dòng của con
 │     ngược lại (file / device):
 │        total += st.size
 │        print && printall -> in "<size>\t<path con>"
 ├─ close(fd)
 ├─ path[plen] = 0                        <- trả buffer cho người gọi
 └─ return total
```

Lưu ý `fd` vẫn mở trong lúc đệ quy (xv6 không có `lseek` nên không thể đóng rồi
mở lại mà giữ vị trí đọc). Tiến trình chỉ có `NOFILE = 16` fd, nên cây sâu hơn
~14 cấp sẽ hết fd — nhưng `MAXPATH = 128` thực tế đã chặn độ sâu trước đó
(mỗi cấp tốn ít nhất 2 ký tự đường dẫn). Đây là đơn giản hoá có ý thức.

---

## 5. Bảng nhánh lỗi

| Lệnh | Nhánh rẽ | Kết quả | Mã thoát |
|---|---|---|---|
| `du nofile` | `stat` thất bại ở gốc | `du: cannot stat nofile` | 1 |
| `du t -q` | tuỳ chọn không nhận ra | `usage: du [path] [-a] [-s]` | 1 |
| `du t t2` | hai path | như trên | 1 |
| `du <path dài hơn 128>` | `strlen(root) >= PATHMAX` | `du: path too long: ...` | 1 |
| thư mục con không mở được | `open` thất bại trong `du()` | `du: cannot open <path>`, nhánh đó tính **0**, đi tiếp | 0 |
| một mục không `stat` được | `stat` thất bại trong vòng lặp | `du: cannot stat <path>`, bỏ mục đó, đi tiếp | 0 |
| ghép tên sẽ tràn `PATHMAX` | `namepos + DIRSIZ >= PATHMAX` | `du: path too long: <path>`, nhánh đó tính **0** | 0 |
| `du e` (thư mục rỗng) | vòng lặp 0 mục hợp lệ | `0	e` | 0 |
| `du README` (file thường) | `st.type != T_DIR` | `2403	README` | 0 |
| `du t -s -a` | `-s` tắt mọi dòng trung gian | chỉ một dòng tổng | 0 |

Nguyên tắc: lỗi ở **gốc** thì `exit(1)` vì con số in ra sẽ vô nghĩa; lỗi ở **một
nhánh con** thì báo ra fd 2 rồi đi tiếp, coi nhánh đó là 0 byte. Cách này làm
tổng bị **thiếu** chứ không bao giờ bị **thừa** — và người dùng luôn thấy dòng
cảnh báo trên fd 2 nên biết con số không đầy đủ. Mọi thông báo lỗi ra **fd 2**
và viết **không dấu** cho an toàn trên console xv6.

---

## 6. Kết quả kiểm thử

Không có autograder cho bài này. Nhóm kiểm thử bằng `xv6run.py` — script tự viết
đẩy lần lượt từng lệnh vào stdin của QEMU rồi thu lại toàn bộ phiên làm việc.

### 6.1 Dựng đúng cây thư mục của đề

```
mkdir t
mkdir t/a
echo 1234567890 > t/a/f1.txt
mkdir t/a/aa
echo hello > t/a/aa/f2.txt
echo world > t/a/aa/f3.txt
mkdir t/z
```

### 6.2 Ba trường hợp chính của đề — output thật, dán nguyên văn

```
$ du t
12	t/a/aa
23	t/a
0	t/z
23	t
$ du t -s
23	t
$ du t -a
11	t/a/f1.txt
6	t/a/aa/f2.txt
6	t/a/aa/f3.txt
12	t/a/aa
23	t/a
0	t/z
23	t
```

Đối chiếu với mẫu đề:

| Khía cạnh | Khớp? |
|---|---|
| Tập hợp đường dẫn được in | **khớp** — `du` chỉ thư mục; `du -a` thêm cả 3 file |
| Thứ tự các dòng (hậu thứ tự, gốc cuối) | **khớp từng dòng** |
| Định dạng `<bytes>\t<path>` | **khớp** |
| Thư mục rỗng `z` ra `0` | **khớp** — xác nhận quy ước ở mục 1 |
| `-s` in đúng một dòng | **khớp** |
| Giá trị số | **khác** — xem giải thích ở mục 1 |

Kiểm tra lại số học bằng tay: `11 + 6 + 6 = 23`, và `t/a/aa = 6 + 6 = 12`,
`t/a = 11 + 12 = 23`, `t/z = 0`, `t = 23 + 0 = 23`. Đúng, và đúng với quy ước
"không cộng inode thư mục" (nếu cộng thì `t/z` không thể bằng 0).

### 6.3 Các trường hợp biên — output thật

```
$ du e
0	e
$ du nofile
du: cannot stat nofile
$ du README
2403	README
$ du README -a
2403	README
$ du t -q
usage: du [path] [-a] [-s]
$ du t -s -a
23	t
```

Và chạy trên một nhánh con để xác nhận `path` không bắt buộc là `.`:

```
$ du a
12	a/aa
23	a
$ du a -s
23	a
$ du a -a
11	a/f1.txt
6	a/aa/f2.txt
6	a/aa/f3.txt
12	a/aa
23	a
```

---

## 7. Về việc dùng chung phép duyệt thư mục

Quyết định và lý do đầy đủ nằm ở **`docs/tree.md` mục 6**. Tóm lại: nhóm **không
tách** thành `user/dirwalk.c` dùng chung, vì luật link `$U/_%: $U/%.o $(ULIB)`
trong `Makefile` buộc phải thêm `dirwalk.o` vào `ULIB` (link vào cả 22 chương
trình không liên quan) hoặc phải viết luật riêng, trong khi yêu cầu bài tập chỉ
cho sửa Makefile ở hai dòng `UPROGS`.

Thay vào đó, hai chương trình **chia sẻ cấu trúc**: vòng lặp duyệt trong
`du.c` và `tree.c` có cùng thứ tự các bước, cùng tên biến (`plen`, `namepos`,
`de`, `st`), cùng quy ước buffer toàn cục và cùng quy ước "sửa xong trả lại độ
dài cũ".

Riêng với `du`, có một lý do kỹ thuật nữa để không tách: `du` cần giá trị trả về
chảy **lên** theo đệ quy (tổng byte), còn `tree` cần trạng thái chảy **xuống**
(tiền tố vẽ cây) cộng thêm cơ chế nhìn trước một mục để biết "mục cuối". Một hàm
duyệt chung phục vụ cả hai sẽ phải nhận callback kèm cả tiền tố, cờ "mục cuối"
và giá trị tích luỹ — một framework có đúng hai người dùng, mỗi người dùng một
nửa tham số.

Đánh đổi chấp nhận: lỗi trong phép duyệt phải sửa hai nơi. Nếu có chương trình
thứ ba cùng duyệt thư mục thì nhóm sẽ xét lại.

---

## 8. Khó khăn và ghi chú cho báo cáo

**1. Mẫu output của đề không nhất quán với `st.size`.** Điểm gây nhiễu nhất của
bài. `echo 1234567890 > f1.txt` tạo file **11 byte** nhưng đề ghi **16**. Nhóm
đã kiểm bằng `du README` (file có kích thước biết trước là 2403 byte — đúng bằng
kích thước file `README` trong source tree) để xác nhận `st.size` trả về số thật
chứ không phải lỗi của chương trình. Kết luận: mẫu đề là ví dụ minh hoạ định
dạng và thứ tự, không phải số liệu đo thật. Nhóm giữ `st.size` thật theo đúng
câu "lấy size từ `st.size`" của đề, và **không chế số cho khớp mẫu**.

**2. Thứ tự in buộc phải là hậu thứ tự, nên không tách được "tính" khỏi "in"
hoàn toàn.** Đề gợi ý "hàm `long du(path)` trả tổng bytes" cộng "phần in theo
tuỳ chọn" — nghe như hai pha riêng biệt. Nhưng tổng của cha chỉ biết sau khi
duyệt hết con, nên việc in phải xen vào lúc đệ quy trả về. Giải pháp "cha in
dòng của con, `main` in dòng gốc" (mục 3.1) giữ được tinh thần tách hai phần mà
vẫn ra đúng thứ tự mẫu đề.

**3. Phải dùng `%ld` chứ không phải `%d`.** `st.size` là `uint64`. `user/printf.c`
chỉ xử lý `%d`, `%ld`, `%lld`, `%u`, `%x`, `%p`, `%s` — và `%d` cắt mất 32 bit
cao. Với `fs.img` cỡ 2 MB thì không bao giờ thấy lỗi, nhưng vẫn là sai nguyên
tắc nên nhóm dùng `%ld` và tích luỹ vào `long`.

**4. Quy ước kích thước thư mục cần nghĩ trước khi viết code, không phải sau.**
Ban đầu nhóm tính cộng cả `st.size` của inode thư mục cho "đúng thực tế đĩa".
Nhưng mẫu đề ghi `0` cho thư mục rỗng `z`, mà thư mục rỗng trong xv6 luôn có
`st.size` khác 0. Đó là manh mối cho thấy đề muốn quy ước "không cộng inode thư
mục". Đã chọn và áp dụng nhất quán ở mọi cấp (mục 1).

**5. `fs.img` không nằm trong target mặc định của `make`**, và trên máy macOS
này phải truyền `TOOLPREFIX=riscv64-elf-` ở mọi lệnh `make`. Thiếu
`$U/_du\` trong `UPROGS` thì chương trình build sạch nhưng không tồn tại trong
`fs.img`, shell chỉ báo `exec failed`.

**6. Môi trường build — GCC quá mới.** Đã nêu trong `docs/sleep.md`: Makefile
cần `CFLAGS += -std=gnu17 -Wno-unused-but-set-variable` để build với GCC 16 của
Homebrew. Sửa môi trường, không phải phần bài tập, nhưng xuất hiện trong
`.patch`.

**7. Chỗ chưa kiểm chứng được.** Toàn bộ kiểm thử chạy trên macOS với
`riscv64-elf-gcc`. Đề yêu cầu Linux (WSL + Ubuntu, prefix
`riscv64-linux-gnu-`). Source C giống nhau nên kết quả phải giống. Hai giới hạn
chưa chạm tới trong kiểm thử: cây sâu quá `NOFILE = 16` cấp (không dựng được vì
`MAXPATH = 128` chặn trước), và tổng vượt 32 bit (`fs.img` chỉ 2 MB).
